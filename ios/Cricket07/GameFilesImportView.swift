import SwiftUI
import UniformTypeIdentifiers

/// Copies user-provided game files into this app's private Application Support directory.
/// This does not launch Cricket 07; the native game/graphics integration is still pending.
struct GameFilesImportView: View {
    @State private var pickingFolder = false
    @State private var isImporting = false
    @State private var message = "No game folder imported"
    @State private var importedPath = ""

    var body: some View {
        VStack(spacing: 8) {
            Text("Game files").font(.headline)
            Text(message).font(.footnote).multilineTextAlignment(.center)
            if !importedPath.isEmpty {
                Text(importedPath)
                    .font(.caption2)
                    .monospaced()
                    .lineLimit(2)
                    .textSelection(.enabled)
            }
            Button(isImporting ? "Importing…" : "Import your Cricket 07 folder") {
                pickingFolder = true
            }
            .disabled(isImporting)
        }
        .fileImporter(isPresented: $pickingFolder,
                      allowedContentTypes: [.folder],
                      allowsMultipleSelection: false) { result in
            switch result {
            case .failure(let error):
                message = "Folder selection failed: \(error.localizedDescription)"
            case .success(let urls):
                guard let folder = urls.first else { return }
                isImporting = true
                message = "Copying files into private app storage…"
                Task.detached(priority: .utility) {
                    let access = folder.startAccessingSecurityScopedResource()
                    defer { if access { folder.stopAccessingSecurityScopedResource() } }
                    do {
                        let destination = try GameFolderStorage.importFolder(folder)
                        await MainActor.run {
                            importedPath = destination.path
                            message = "Game files imported. Engine launch is not yet implemented."
                            isImporting = false
                        }
                    } catch {
                        await MainActor.run {
                            message = "Import failed: \(error.localizedDescription)"
                            isImporting = false
                        }
                    }
                }
            }
        }
    }
}

enum GameFolderStorage {
    private static let maxBytes: Int64 = 10 * 1024 * 1024 * 1024
    private static let maxEntries = 50_000

    static func importFolder(_ source: URL) throws -> URL {
        let fm = FileManager.default
        guard let support = fm.urls(for: .applicationSupportDirectory, in: .userDomainMask).first else {
            throw ImportError.storageUnavailable
        }
        let root = support.appendingPathComponent("Cricket07Games", isDirectory: true)
        try fm.createDirectory(at: root, withIntermediateDirectories: true)
        let destination = root.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try fm.createDirectory(at: destination, withIntermediateDirectories: false)

        var count = 0
        var bytes: Int64 = 0
        do {
            try copyDirectoryContents(source, into: destination, count: &count, bytes: &bytes)
            guard count > 0 else { throw ImportError.emptyFolder }
            return destination
        } catch {
            try? fm.removeItem(at: destination)
            throw error
        }
    }

    private static func copyDirectoryContents(_ source: URL, into destination: URL,
                                               count: inout Int, bytes: inout Int64) throws {
        let fm = FileManager.default
        let entries = try fm.contentsOfDirectory(
            at: source, includingPropertiesForKeys: [.isSymbolicLinkKey, .isDirectoryKey, .fileSizeKey],
            options: []
        )
        for item in entries {
            count += 1
            guard count <= maxEntries else { throw ImportError.tooManyFiles }
            let details = try item.resourceValues(forKeys: [.isSymbolicLinkKey, .isDirectoryKey, .fileSizeKey])
            guard details.isSymbolicLink != true else { throw ImportError.symlinkNotAllowed }
            let output = destination.appendingPathComponent(item.lastPathComponent)
            if details.isDirectory == true {
                try fm.createDirectory(at: output, withIntermediateDirectories: false)
                try copyDirectoryContents(item, into: output, count: &count, bytes: &bytes)
            } else {
                let size = Int64(details.fileSize ?? 0)
                guard size >= 0, bytes <= maxBytes - size else { throw ImportError.tooLarge }
                bytes += size
                try fm.copyItem(at: item, to: output)
            }
        }
    }

    private enum ImportError: LocalizedError {
        case storageUnavailable, emptyFolder, tooManyFiles, tooLarge, symlinkNotAllowed
        var errorDescription: String? {
            switch self {
            case .storageUnavailable: return "Application Support storage is unavailable."
            case .emptyFolder: return "Selected folder is empty."
            case .tooManyFiles: return "Folder contains more than 50,000 entries."
            case .tooLarge: return "Folder exceeds the 10 GiB import limit."
            case .symlinkNotAllowed: return "Symbolic links are not permitted in imported folders."
            }
        }
    }
}
