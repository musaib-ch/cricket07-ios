import Foundation
import Network

/// Streams local resources with COOP/COEP. It never binds to a LAN address.
final class LoopbackServer {
    private let queue = DispatchQueue(label: "Boundary07.loopback")
    private var listener: NWListener?
    private let webRoot: URL
    private let package: () -> URL?
    private let token: String
    private var connections: [ObjectIdentifier: NWConnection] = [:]
    private(set) var origin: URL?
    init(webRoot: URL, token: String, package: @escaping () -> URL?) {
        self.webRoot = webRoot; self.token = token; self.package = package
    }
    func start(port: UInt16 = 8077, completion: @escaping (Result<URL, Error>) -> Void) {
        do {
            let params = NWParameters.tcp
            params.requiredLocalEndpoint = .hostPort(host: "127.0.0.1", port: NWEndpoint.Port(rawValue: port)!)
            params.allowLocalEndpointReuse = true
            let listener = try NWListener(using: params)
            self.listener = listener
            var delivered = false
            listener.stateUpdateHandler = { [weak self] state in
                guard let self = self else { return }
                switch state {
                case .ready:
                    guard !delivered, let port = listener.port else { return }; delivered = true
                    let url = URL(string: "http://127.0.0.1:\(port.rawValue)/\(self.token)/")!
                    self.origin = url
                    DispatchQueue.main.async { completion(.success(url)) }
                case .failed(let error):
                    if !delivered { delivered = true; DispatchQueue.main.async { completion(.failure(error)) } }
                default: break
                }
            }
            listener.newConnectionHandler = { [weak self] connection in
                guard let self = self else { connection.cancel(); return }
                self.connections[ObjectIdentifier(connection)] = connection
                connection.stateUpdateHandler = { [weak self, weak connection] state in
                    guard let connection = connection else { return }
                    if case .cancelled = state { self?.connections.removeValue(forKey: ObjectIdentifier(connection)) }
                    if case .failed = state { connection.cancel() }
                }
                connection.start(queue: self.queue)
                self.receive(connection, accumulated: Data())
            }
            listener.start(queue: queue)
        } catch { completion(.failure(error)) }
    }
    func stop() {
        queue.async { [self] in
            listener?.cancel(); listener = nil
            for connection in connections.values { connection.cancel() }
            connections.removeAll()
        }
    }
    private func receive(_ connection: NWConnection, accumulated: Data) {
        connection.receive(minimumIncompleteLength: 1, maximumLength: 8192) { [weak self] data, _, complete, error in
            guard let self = self else { return }
            var buffer = accumulated; if let data = data { buffer.append(data) }
            guard buffer.count <= 16384 else { self.reply(connection, status: "431 Request Header Fields Too Large"); return }
            if let text = String(data: buffer, encoding: .utf8), text.contains("\r\n\r\n") { self.route(connection, request: text); return }
            if complete || error != nil { connection.cancel(); return }
            self.receive(connection, accumulated: buffer)
        }
    }
    private func route(_ connection: NWConnection, request: String) {
        guard let first = request.components(separatedBy: "\r\n").first else { reply(connection, status: "400 Bad Request"); return }
        let fields = first.split(separator: " ")
        guard fields.count == 3, fields[0] == "GET" else { reply(connection, status: "405 Method Not Allowed"); return }
        let target = String(fields[1]).components(separatedBy: "?")[0]
        guard let decoded = target.removingPercentEncoding, decoded.hasPrefix("/" + token + "/") else { reply(connection, status: "404 Not Found"); return }
        let relative = String(decoded.dropFirst(token.count + 2))
        guard !relative.contains("\\"), !relative.contains("\0"), !relative.split(separator: "/").contains(where: { $0 == ".." || $0 == "." }) else { reply(connection, status: "403 Forbidden"); return }
        if relative == "native-manifest.json" {
            do {
                guard let imported = package() else { reply(connection, status: "404 Not Found"); return }
                // iOS may enumerate /private/var/... while the stored URL uses /var/....
                // Canonicalize both sides before forming paths relative to Game.
                let root = imported.appendingPathComponent("Game", isDirectory: true)
                    .resolvingSymlinksInPath().standardizedFileURL
                let keys: [URLResourceKey] = [.isRegularFileKey, .isSymbolicLinkKey, .fileSizeKey]
                guard let entries = FileManager.default.enumerator(at: root, includingPropertiesForKeys: keys) else { throw BoundaryError.invalid("Cannot read imported game.") }
                var files: [[String: Any]] = []
                for case let file as URL in entries {
                    let values = try file.resourceValues(forKeys: Set(keys))
                    if values.isSymbolicLink == true { throw BoundaryError.invalid("Linked game file.") }
                    if values.isRegularFile == true {
                        files.append(["path": try Self.relativeGamePath(file, root: root), "bytes": values.fileSize ?? 0])
                    }
                }
                let data = try JSONSerialization.data(withJSONObject: ["version": imported.lastPathComponent, "files": files])
                reply(connection, body: data, type: "application/json")
            } catch { reply(connection, status: "500 Internal Server Error") }
            return
        }
        let root: URL; let path: String
        if relative.hasPrefix("build/"), let imported = package() { root = imported.appendingPathComponent("Runtime"); path = String(relative.dropFirst(6)) }
        else if relative.hasPrefix("game/"), let imported = package() { root = imported.appendingPathComponent("Game"); path = String(relative.dropFirst(5)) }
        else { root = webRoot; path = relative.isEmpty ? "index.html" : relative }
        let file = root.appendingPathComponent(path).resolvingSymlinksInPath().standardizedFileURL
        let base = root.resolvingSymlinksInPath().standardizedFileURL
        guard file.path.hasPrefix(base.path + "/"), let values = try? file.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey]), values.isRegularFile == true,
              let handle = try? FileHandle(forReadingFrom: file) else { reply(connection, status: "404 Not Found"); return }
        let types = ["html":"text/html; charset=utf-8", "js":"text/javascript", "css":"text/css", "wasm":"application/wasm", "json":"application/json", "txt":"text/plain; charset=utf-8"]
        let header = headers(status: "200 OK", count: values.fileSize ?? 0, type: types[file.pathExtension] ?? "application/octet-stream")
        connection.send(content: Data(header.utf8), completion: .contentProcessed { [weak self] error in
            if error != nil { try? handle.close(); connection.cancel(); return }
            self?.stream(handle, connection)
        })
    }
    static func relativeGamePath(_ file: URL, root: URL) throws -> String {
        let base = root.resolvingSymlinksInPath().standardizedFileURL.pathComponents
        let path = file.resolvingSymlinksInPath().standardizedFileURL.pathComponents
        guard path.count > base.count, Array(path.prefix(base.count)) == base else {
            throw BoundaryError.invalid("An imported asset is outside the Game folder.")
        }
        return path.dropFirst(base.count).joined(separator: "/")
    }

    private func stream(_ file: FileHandle, _ connection: NWConnection) {
        do {
            guard let chunk = try file.read(upToCount: 256 * 1024), !chunk.isEmpty else {
                try? file.close()
                connection.send(content: nil, contentContext: .finalMessage, isComplete: true, completion: .contentProcessed { _ in connection.cancel() })
                return
            }
            connection.send(content: chunk, completion: .contentProcessed { [weak self] error in
                if error != nil { try? file.close(); connection.cancel() } else { self?.stream(file, connection) }
            })
        } catch { try? file.close(); connection.cancel() }
    }
    private func headers(status: String, count: Int, type: String) -> String {
        "HTTP/1.1 \(status)\r\nContent-Length: \(count)\r\nContent-Type: \(type)\r\nConnection: close\r\nCache-Control: no-store\r\nCross-Origin-Opener-Policy: same-origin\r\nCross-Origin-Embedder-Policy: require-corp\r\nCross-Origin-Resource-Policy: same-origin\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'self' blob:; script-src 'self' 'unsafe-inline' 'wasm-unsafe-eval'; worker-src 'self' blob:; style-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'self' data: blob:; media-src 'self' blob:; object-src 'none'; frame-ancestors 'none'\r\n\r\n"
    }
    private func reply(_ connection: NWConnection, status: String = "200 OK", body: Data = Data(), type: String = "text/plain") {
        var content = Data(headers(status: status, count: body.count, type: type).utf8); content.append(body)
        connection.send(content: content, completion: .contentProcessed { _ in connection.cancel() })
    }
}
