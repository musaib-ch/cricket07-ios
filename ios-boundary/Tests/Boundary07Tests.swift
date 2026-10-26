import XCTest
import WebKit
import CryptoKit
@testable import Boundary07

final class Boundary07Tests: XCTestCase {
    private var temporary: URL!
    override func setUpWithError() throws {
        temporary = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: temporary, withIntermediateDirectories: true)
    }
    override func tearDownWithError() throws { try? FileManager.default.removeItem(at: temporary) }

    private func fixture() throws -> URL {
        let source = temporary.appendingPathComponent("Synthetic")
        for name in ["Runtime", "Game/Data", "Game/Audio"] { try FileManager.default.createDirectory(at: source.appendingPathComponent(name), withIntermediateDirectories: true) }
        try Data([0,97,115,109,1,0,0,0]).write(to: source.appendingPathComponent("Runtime/recomp.wasm"))
        try Data("// Synthetic fixture, no game code\n".utf8).write(to: source.appendingPathComponent("Runtime/recomp.js"))
        try Data("Synthetic executable fixture".utf8).write(to: source.appendingPathComponent("Game/Cricket07.exe"))
        var runtime: [String: RuntimeFile] = [:]
        for name in ["recomp.js", "recomp.wasm"] {
            let url = source.appendingPathComponent("Runtime/" + name)
            runtime[name] = RuntimeFile(sha256: try PackageStorage.sha256(url), bytes: Int64(try Data(contentsOf: url).count))
        }
        let manifest = PrivatePackageManifest(format: "boundary07-private-v1", runtime: runtime, gameExecutable: "Cricket07.exe", gameExecutableSHA256: try PackageStorage.sha256(source.appendingPathComponent("Game/Cricket07.exe")))
        try JSONEncoder().encode(manifest).write(to: source.appendingPathComponent("bundle.json"))
        return source
    }
    func testImportRejectsCorruptionAndPreservesPreviousBundle() throws {
        let source = try fixture()
        let storage = try PackageStorage(root: temporary.appendingPathComponent("Storage"))
        let imported = try storage.importFolder(source)
        XCTAssertEqual(storage.current, imported)
        try Data("corrupt".utf8).write(to: source.appendingPathComponent("Runtime/recomp.wasm"))
        XCTAssertThrowsError(try storage.importFolder(source))
        XCTAssertEqual(storage.current, imported)
        XCTAssertTrue(FileManager.default.fileExists(atPath: source.appendingPathComponent("Game/Cricket07.exe").path))
    }
    func testImportRejectsLinkedAssets() throws {
        let source = try fixture()
        try FileManager.default.createSymbolicLink(at: source.appendingPathComponent("Game/Data/link"), withDestinationURL: source.appendingPathComponent("Game/Cricket07.exe"))
        let storage = try PackageStorage(root: temporary.appendingPathComponent("Storage"))
        XCTAssertThrowsError(try storage.importFolder(source))
        XCTAssertNil(storage.current)
    }

    @MainActor func testLocalServerHeadersStreamingAndWKCapabilities() async throws {
        let source = try fixture()
        let storage = try PackageStorage(root: temporary.appendingPathComponent("Storage"))
        let imported = try storage.importFolder(source)
        let bytes = Data((0..<(700 * 1024)).map { UInt8($0 % 251) })
        try bytes.write(to: imported.appendingPathComponent("Game/Data/stream.bin"))
        let web = try XCTUnwrap(Bundle.main.url(forResource: "Web", withExtension: nil))
        let server = LoopbackServer(webRoot: web, token: "test-token", package: { imported })
        let base: URL = try await withCheckedThrowingContinuation { continuation in server.start(port: 0) { continuation.resume(with: $0) } }
        defer { server.stop() }
        let (body, response) = try await URLSession.shared.data(from: base.appendingPathComponent("game/Data/stream.bin"))
        XCTAssertEqual(body, bytes)
        let http = try XCTUnwrap(response as? HTTPURLResponse)
        XCTAssertEqual(http.value(forHTTPHeaderField: "Cross-Origin-Opener-Policy"), "same-origin")
        XCTAssertEqual(http.value(forHTTPHeaderField: "Cross-Origin-Embedder-Policy"), "require-corp")
        let (_, wasmResponse) = try await URLSession.shared.data(from: base.appendingPathComponent("build/recomp.wasm"))
        XCTAssertEqual((wasmResponse as? HTTPURLResponse)?.mimeType, "application/wasm")
        let wrong = URL(string: "http://127.0.0.1:\(base.port!)/wrong-token/index.html")!
        let (_, wrongResponse) = try await URLSession.shared.data(from: wrong)
        XCTAssertEqual((wrongResponse as? HTTPURLResponse)?.statusCode, 404)
        let (manifest, _) = try await URLSession.shared.data(from: base.appendingPathComponent("native-manifest.json"))
        XCTAssertTrue(String(decoding: manifest, as: UTF8.self).contains("stream.bin"))

        let receiver = ProbeReceiver(expectation: expectation(description: "WKWebView capability report"))
        let configuration = WKWebViewConfiguration()
        configuration.userContentController.add(receiver, name: "boundary")
        let view = WKWebView(frame: CGRect(x: 0, y: 0, width: 900, height: 400), configuration: configuration)
        view.load(URLRequest(url: base.appendingPathComponent("probe.html")))
        await fulfillment(of: [receiver.expectation], timeout: 40)
        let report = try XCTUnwrap(receiver.report)
        XCTAssertNotNil(report["checks"])
        XCTAssertNotNil(report["allRequiredPassed"])
        // A simulator report is evidence about this simulator only; feature availability is not asserted.
        let data = try JSONSerialization.data(withJSONObject: report, options: [.prettyPrinted, .sortedKeys])
        let attachment = XCTAttachment(data: data, uniformTypeIdentifier: "public.json")
        attachment.name = "Boundary07-simulator-capabilities"; attachment.lifetime = .keepAlways; add(attachment)
        print("BOUNDARY_SIMULATOR_REPORT " + String(decoding: data, as: UTF8.self))
        view.stopLoading(); configuration.userContentController.removeScriptMessageHandler(forName: "boundary")
    }
}

@MainActor final class ProbeReceiver: NSObject, WKScriptMessageHandler {
    let expectation: XCTestExpectation
    var report: [String: Any]?
    init(expectation: XCTestExpectation) { self.expectation = expectation }
    func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
        guard report == nil, let body = message.body as? [String: Any], body["type"] as? String == "capabilities", let report = body["report"] as? [String: Any] else { return }
        self.report = report; expectation.fulfill()
    }
}
