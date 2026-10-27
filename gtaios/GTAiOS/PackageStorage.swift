import Foundation

/// GTAiOS keeps the converted game outside the app bundle. The selected
/// playgta5.com folder is retained as a security-scoped bookmark and streamed
/// on demand by the loopback server; importing never copies the 21 GB data tree.
final class PackageStorage {
    let root: URL
    private let bookmarkURL: URL
    private var scopedURL: URL?

    init(root: URL? = nil) throws {
        let fm = FileManager.default
        self.root = root ?? fm.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0].appendingPathComponent("GTAiOS", isDirectory: true)
        try fm.createDirectory(at: self.root, withIntermediateDirectories: true)
        self.bookmarkURL = self.root.appendingPathComponent("game-folder.bookmark")
        _ = resolveBookmark()
    }

    var current: URL? { resolveBookmark() }

    @discardableResult
    func importFolder(_ source: URL) throws -> URL {
        let scoped = source.startAccessingSecurityScopedResource()
        defer { if scoped { source.stopAccessingSecurityScopedResource() } }
        let folder = source.standardizedFileURL
        for item in ["index.html", "b", "data"] {
            var directory: ObjCBool = false
            let exists = FileManager.default.fileExists(atPath: folder.appendingPathComponent(item).path, isDirectory: &directory)
            guard exists, (item == "index.html" ? !directory.boolValue : directory.boolValue) else {
                throw BoundaryError.invalid("Choose the converted playgta5.com folder containing index.html, b and data.")
            }
        }
        guard let wasm = firstFile(named: "game.wasm", under: folder.appendingPathComponent("b")),
              (try? wasm.resourceValues(forKeys: [.fileSizeKey]).fileSize ?? 0) > 8 else {
            throw BoundaryError.invalid("The selected folder does not contain a complete WebAssembly game.wasm file.")
        }
        let data = try source.bookmarkData(options: [.withSecurityScope], includingResourceValuesForKeys: nil, relativeTo: nil)
        try data.write(to: bookmarkURL, options: .atomic)
        scopedURL?.stopAccessingSecurityScopedResource()
        scopedURL = folder
        _ = folder.startAccessingSecurityScopedResource()
        return folder
    }

    private func resolveBookmark() -> URL? {
        guard let data = try? Data(contentsOf: bookmarkURL) else { return scopedURL }
        var stale = false
        guard let url = try? URL(resolvingBookmarkData: data, options: [.withSecurityScope], relativeTo: nil, bookmarkDataIsStale: &stale) else { return nil }
        if stale, let refreshed = try? url.bookmarkData(options: [.withSecurityScope], includingResourceValuesForKeys: nil, relativeTo: nil) { try? refreshed.write(to: bookmarkURL, options: .atomic) }
        if scopedURL?.path != url.path { scopedURL?.stopAccessingSecurityScopedResource(); scopedURL = url; _ = url.startAccessingSecurityScopedResource() }
        return url
    }

    private func firstFile(named name: String, under root: URL) -> URL? {
        guard let e = FileManager.default.enumerator(at: root, includingPropertiesForKeys: [.isRegularFileKey]) else { return nil }
        for case let url as URL in e where url.lastPathComponent == name { return url }
        return nil
    }
}
