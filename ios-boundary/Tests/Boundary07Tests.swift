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

    func testGameAssetPathsUseCanonicalRootAndRejectOutsideFiles() throws {
        let source = try fixture()
        let alias = temporary.appendingPathComponent("PackageAlias")
        try FileManager.default.createSymbolicLink(at: alias, withDestinationURL: source)
        let canonicalFile = source.appendingPathComponent("Game/Cricket07.exe").resolvingSymlinksInPath()
        XCTAssertEqual(try LoopbackServer.relativeGamePath(canonicalFile, root: alias.appendingPathComponent("Game")), "Cricket07.exe")
        XCTAssertEqual(try LoopbackServer.relativeGamePath(alias.appendingPathComponent("Game/Cricket07.exe"), root: source.appendingPathComponent("Game")), "Cricket07.exe")
        XCTAssertThrowsError(try LoopbackServer.relativeGamePath(source.appendingPathComponent("Runtime/recomp.wasm"), root: source.appendingPathComponent("Game")))
        XCTAssertThrowsError(try LoopbackServer.relativeGamePath(source.appendingPathComponent("GameOther/file.bin"), root: source.appendingPathComponent("Game")))
    }

    func testSavedProgressBackupRejectsUnsafeAndDuplicatePaths() throws {
        let file: [String: Any] = ["path": "Profiles/test.sav", "base64": Data([1, 2, 3]).base64EncodedString()]
        let valid: [String: Any] = ["format": "boundary07-saves-v1", "files": [file]]
        XCTAssertFalse(try SaveBackup.validate(valid).isEmpty)
        for path in ["../outside", "/absolute", "Profiles//x", "Profiles/./x", "C:/x", "Profiles\\x"] {
            XCTAssertThrowsError(try SaveBackup.validate(["format": "boundary07-saves-v1", "files": [["path": path, "base64": "AQ=="]]]))
        }
        XCTAssertThrowsError(try SaveBackup.validate(["format": "boundary07-saves-v1", "files": [file, file]]))
        XCTAssertThrowsError(try SaveBackup.validate(["format": "boundary07-saves-v1", "files": [["path": "x", "base64": "invalid!"]]]))
    }

    @MainActor func testLocalServerHeadersStreamingAndWKCapabilities() async throws {
        let source = try fixture()
        let storage = try PackageStorage(root: temporary.appendingPathComponent("Storage"))
        let imported = try storage.importFolder(source)
        let bytes = Data((0..<(700 * 1024)).map { UInt8($0 % 251) })
        try bytes.write(to: imported.appendingPathComponent("Game/Data/stream.bin"))
        let web = try XCTUnwrap(Bundle.main.url(forResource: "Web", withExtension: nil))
        let alias = temporary.appendingPathComponent("ImportedPackageAlias")
        try FileManager.default.createSymbolicLink(at: alias, withDestinationURL: imported)
        let server = LoopbackServer(webRoot: web, token: "test-token", package: { alias })
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
        let manifestObject = try XCTUnwrap(try JSONSerialization.jsonObject(with: manifest) as? [String: Any])
        let files = try XCTUnwrap(manifestObject["files"] as? [[String: Any]])
        let paths = Set(files.compactMap { $0["path"] as? String })
        XCTAssertEqual(paths, Set(["Cricket07.exe", "Data/stream.bin"]))
        for path in paths {
            let (_, fileResponse) = try await URLSession.shared.data(from: base.appendingPathComponent("game/" + path))
            XCTAssertEqual((fileResponse as? HTTPURLResponse)?.statusCode, 200, path)
        }

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
        // Exercise the shipped controller in real WebKit with fake engine exports.
        // The synthetic pointer fixture bypasses capture only; physical multitouch needs device testing.
        _ = try await view.evaluateJavaScript("""
        document.body.innerHTML='<canvas id="canvas" width="1024" height="768"></canvas>';
        window.testKeys=new Set();window.testClicks=[];window.testMoves=[];
        window.recomp={_input_key_hid:(k,d)=>d?testKeys.add(k):testKeys.delete(k),_input_mouse_move:(x,y)=>testMoves.push([x,y]),_input_mouse_button:(b,d)=>testClicks.push([b,d])};
        Element.prototype.setPointerCapture=function(){};
        void 0;
        """)
        // Pointer coordinates depend on the shipped square stick layout. Without its CSS,
        // a div expands to the page width and the same gesture points above its centre.
        let controllerCSS = try String(contentsOf: web.appendingPathComponent("touch-controls.css"), encoding: .utf8)
        let cssJSON = String(decoding: try JSONSerialization.data(withJSONObject: [controllerCSS]), as: UTF8.self)
        _ = try await view.evaluateJavaScript("const controllerStyle=document.createElement('style');controllerStyle.textContent=\(cssJSON)[0];document.head.append(controllerStyle);void 0;")
        let controls = try String(contentsOf: web.appendingPathComponent("touch-controls.js"), encoding: .utf8)
        _ = try await view.evaluateJavaScript(controls)
        let inputResult = try await view.evaluateJavaScript("""
        (()=>{
          BoundaryControls.start();
          const send=(selector,type,id,x,y)=>document.querySelector(selector).dispatchEvent(new PointerEvent(type,{bubbles:true,cancelable:true,pointerId:id,clientX:x,clientY:y}));
          const pad=document.querySelector('.b07-stick').getBoundingClientRect();
          send('.b07-stick','pointerdown',1,pad.left+pad.width*.15,pad.top+pad.height*.85);
          send('.b07-a','pointerdown',2,0,0);send('.b07-b','pointerdown',3,0,0);
          const together=[...testKeys].sort((a,b)=>a-b).join(',');
          send('.b07-a','pointerup',2,0,0);const independent=[...testKeys].sort((a,b)=>a-b).join(',');
          send('.b07-stick','pointercancel',1,0,0);send('.b07-b','pointerup',3,0,0);
          send('.b07-loft','pointerdown',4,0,0);send('.b07-a','pointerdown',5,0,0);send('.b07-a','pointerup',5,0,0);
          const loftOnly=[...testKeys].join(',');
          send('.b07-trackpad','pointerdown',6,200,100);send('.b07-trackpad','pointermove',6,220,110);send('.b07-trackpad','pointercancel',6,220,110);
          BoundaryControls.reset();
          return {together,independent,loftOnly,stickSquare:pad.width>0&&Math.abs(pad.width-pad.height)<1,released:testKeys.size===0,mouseMoved:testMoves.some(v=>v[0]>0&&v[1]>0)};
        })()
        """)
        let input = try XCTUnwrap(inputResult as? [String: Any])
        XCTAssertEqual(input["stickSquare"] as? Bool, true)
        XCTAssertEqual(input["together"] as? String, "7,22,80,81")
        XCTAssertEqual(input["independent"] as? String, "7,80,81")
        XCTAssertEqual(input["loftOnly"] as? String, "225")
        XCTAssertEqual(input["released"] as? Bool, true)
        XCTAssertEqual(input["mouseMoved"] as? Bool, true)
        let sessionSource = try String(contentsOf: web.appendingPathComponent("mobile-session.js"), encoding: .utf8)
        _ = try await view.evaluateJavaScript(sessionSource)
        let singleLaunch = try await view.evaluateJavaScript("BoundarySession.begin() && !BoundarySession.begin()")
        XCTAssertEqual(singleLaunch as? Bool, true)
        _ = try await view.evaluateJavaScript("BoundarySession.frame();void 0")
        let sessionState = try await view.evaluateJavaScript("BoundarySession.state")
        XCTAssertEqual(sessionState as? String, "running")
        // Stop the synthetic session before restore: live game files must never be replaced.
        _ = try await view.evaluateJavaScript("delete window.recomp;window.BoundarySession={active:false};void 0")
        let savesSource = try String(contentsOf: web.appendingPathComponent("saves.js"), encoding: .utf8)
        _ = try await view.evaluateJavaScript(savesSource)
        if let checks = report["checks"] as? [String: [String: Any]], checks["privateStorage"]?["pass"] as? Bool == true {
            let saveResult = try await view.callAsyncJavaScript("""
            await BoundarySaves.prepare();
            const backup={format:'boundary07-saves-v1',files:[{path:'Profiles/test.sav',base64:'AQID'}]};
            await BoundarySaves.restore(backup);
            const saved=await BoundarySaves.backup();
            let rejected=false;try{await BoundarySaves.restore({format:'boundary07-saves-v1',files:[{path:'../outside',base64:'AQ=='}]})}catch{rejected=true}
            const after=await BoundarySaves.backup();
            return saved.files[0].base64==='AQID'&&after.files[0].base64==='AQID'&&rejected;
            """, arguments: [:], in: nil, contentWorld: .page)
            XCTAssertEqual(saveResult as? Bool, true)
        }
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
