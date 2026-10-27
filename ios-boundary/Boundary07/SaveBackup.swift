import Foundation

enum SaveBackup {
    static func validate(_ value: [String: Any]) throws -> Data {
        guard value["format"] as? String == "boundary07-saves-v1", let files = value["files"] as? [[String: Any]], !files.isEmpty, files.count <= 1000 else { throw BoundaryError.invalid("Invalid saved-progress backup.") }
        var paths = Set<String>(); var total = 0
        for file in files {
            guard let path = file["path"] as? String, !path.isEmpty, path.count < 1024, !path.contains("\\"), !path.contains("\0"), !path.contains(":"),
                  path.split(separator: "/", omittingEmptySubsequences: false).allSatisfy({ !$0.isEmpty && $0 != "." && $0 != ".." }), paths.insert(path).inserted,
                  let encoded = file["base64"] as? String, encoded.count <= 11_184_812,
                  let bytes = Data(base64Encoded: encoded), bytes.count <= 8 * 1024 * 1024 else { throw BoundaryError.invalid("Invalid saved file in backup.") }
            total += bytes.count
            guard total <= 32 * 1024 * 1024 else { throw BoundaryError.invalid("Saved progress exceeds 32 MB.") }
        }
        return try JSONSerialization.data(withJSONObject: value, options: [.sortedKeys])
    }
}
