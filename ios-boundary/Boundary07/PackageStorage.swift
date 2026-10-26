import Foundation
import CryptoKit

struct RuntimeFile: Codable {
    let sha256: String
    let bytes: Int64
}
struct PrivatePackageManifest: Codable {
    let format: String
    let runtime: [String: RuntimeFile]
    let gameExecutable: String
    let gameExecutableSHA256: String
}
enum BoundaryError: LocalizedError {
    case invalid(String)
    var errorDescription: String? { if case .invalid(let message) = self { return message }; return nil }
}

final class PackageStorage {
    let root: URL
    private let fm = FileManager.default
    init(root: URL? = nil) throws {
        if let root = root { self.root = root }
        else { self.root = try fm.url(for: .applicationSupportDirectory, in: .userDomainMask,
                                     appropriateFor: nil, create: true).appendingPathComponent("Boundary07", isDirectory: true) }
        try fm.createDirectory(at: self.root, withIntermediateDirectories: true)
    }
    var current: URL? {
        guard let name = try? String(contentsOf: root.appendingPathComponent("current.txt"), encoding: .utf8), UUID(uuidString: name) != nil else { return nil }
        let url = root.appendingPathComponent(name, isDirectory: true)
        return fm.fileExists(atPath: url.appendingPathComponent("bundle.json").path) ? url : nil
    }

    func importFolder(_ source: URL) throws -> URL {
        let scoped = source.startAccessingSecurityScopedResource()
        defer { if scoped { source.stopAccessingSecurityScopedResource() } }
        let values = try source.appendingPathComponent("bundle.json").resourceValues(forKeys: [.isSymbolicLinkKey, .fileSizeKey])
        guard values.isSymbolicLink != true, (values.fileSize ?? Int.max) < 1_000_000 else { throw BoundaryError.invalid("Invalid package manifest.") }
        let manifestData = try Data(contentsOf: source.appendingPathComponent("bundle.json"))
        let manifest = try JSONDecoder().decode(PrivatePackageManifest.self, from: manifestData)
        guard manifest.format == "boundary07-private-v1", manifest.gameExecutable == "Cricket07.exe" else {
            throw BoundaryError.invalid("Choose the Boundary07-iOS-Private folder containing bundle.json, Runtime and Game.")
        }
        let destination = root.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try fm.createDirectory(at: destination, withIntermediateDirectories: false)
        do {
            var count = 0; var total: Int64 = 0
            for name in ["Runtime", "Game"] {
                try copyTree(source.appendingPathComponent(name), destination.appendingPathComponent(name), count: &count, total: &total)
            }
            try manifestData.write(to: destination.appendingPathComponent("bundle.json"), options: .atomic)
            for name in ["recomp.js", "recomp.wasm"] {
                guard let expected = manifest.runtime[name] else { throw BoundaryError.invalid("Runtime manifest is incomplete.") }
                let file = destination.appendingPathComponent("Runtime/" + name)
                let size = try file.resourceValues(forKeys: [.fileSizeKey]).fileSize ?? 0
                guard Int64(size) == expected.bytes, try Self.sha256(file) == expected.sha256.lowercased() else {
                    throw BoundaryError.invalid("The transferred runtime failed its checksum: \(name). Copy the private bundle again.")
                }
            }
            let wasm = try FileHandle(forReadingFrom: destination.appendingPathComponent("Runtime/recomp.wasm"))
            defer { try? wasm.close() }
            guard try wasm.read(upToCount: 8) == Data([0,97,115,109,1,0,0,0]) else { throw BoundaryError.invalid("Invalid WebAssembly module.") }
            guard try Self.sha256(destination.appendingPathComponent("Game/Cricket07.exe")) == manifest.gameExecutableSHA256.lowercased() else {
                throw BoundaryError.invalid("This executable does not match the compiled runtime.")
            }
            for folder in ["Data", "Audio"] {
                var isDirectory: ObjCBool = false
                guard fm.fileExists(atPath: destination.appendingPathComponent("Game/" + folder).path, isDirectory: &isDirectory), isDirectory.boolValue else {
                    throw BoundaryError.invalid("The game folder is missing \(folder).")
                }
            }
            try destination.lastPathComponent.write(to: root.appendingPathComponent("current.txt"), atomically: true, encoding: .utf8)
            return destination
        } catch {
            try? fm.removeItem(at: destination)
            throw error
        }
    }

    private func copyTree(_ source: URL, _ destination: URL, count: inout Int, total: inout Int64) throws {
        let values = try source.resourceValues(forKeys: [.isSymbolicLinkKey, .isDirectoryKey, .isRegularFileKey, .fileSizeKey])
        guard values.isSymbolicLink != true else { throw BoundaryError.invalid("Linked files are not supported in private bundles.") }
        count += 1
        guard count <= 50_000 else { throw BoundaryError.invalid("Too many files in this bundle.") }
        if values.isDirectory == true {
            try fm.createDirectory(at: destination, withIntermediateDirectories: false)
            for file in try fm.contentsOfDirectory(at: source, includingPropertiesForKeys: nil) {
                try copyTree(file, destination.appendingPathComponent(file.lastPathComponent), count: &count, total: &total)
            }
        } else if values.isRegularFile == true {
            total += Int64(values.fileSize ?? 0)
            guard total <= 5 * 1024 * 1024 * 1024 else { throw BoundaryError.invalid("This bundle exceeds the 5 GiB import limit.") }
            try fm.copyItem(at: source, to: destination)
        } else { throw BoundaryError.invalid("Unsupported item in private bundle.") }
    }
    static func sha256(_ url: URL) throws -> String {
        let handle = try FileHandle(forReadingFrom: url)
        defer { try? handle.close() }
        var hash = SHA256()
        while let chunk = try handle.read(upToCount: 1024 * 1024), !chunk.isEmpty { hash.update(data: chunk) }
        return hash.finalize().map { String(format: "%02x", $0) }.joined()
    }
}
