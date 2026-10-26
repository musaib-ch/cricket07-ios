import SwiftUI
import WebKit
import UniformTypeIdentifiers

@MainActor final class BoundaryModel: ObservableObject {
    @Published var message = "Import your private bundle, then check this iPhone."
    @Published var imported = false
    @Published var busy = false
    @Published var checksPassed = false
    @Published var report = "No device check has run."
    @Published var showingWeb = false
    @Published var pickingFolder = false
    @Published var running = false
    @Published var baseURL: URL?
    var page = "probe.html"
    weak var webView: WKWebView?
    private(set) var storage: PackageStorage?
    private var server: LoopbackServer?
    init() {
        do {
            let storage = try PackageStorage(); self.storage = storage; imported = storage.current != nil
            guard let web = Bundle.main.url(forResource: "Web", withExtension: nil) else { throw BoundaryError.invalid("The bundled launcher is missing.") }
            let token = UserDefaults.standard.string(forKey: "BoundaryOriginToken") ?? UUID().uuidString
            UserDefaults.standard.set(token, forKey: "BoundaryOriginToken")
            let server = LoopbackServer(webRoot: web, token: token, package: { storage.current })
            self.server = server
            server.start { [weak self] result in
                switch result {
                case .success(let url): self?.baseURL = url
                case .failure(let error): self?.message = "Local runtime could not start: " + error.localizedDescription
                }
            }
        } catch { message = error.localizedDescription }
    }
    func open(_ page: String) {
        guard baseURL != nil else { return }
        self.page = page; running = false; showingWeb = true
    }
    func importSelected(_ result: Result<[URL], Error>) {
        switch result {
        case .failure(let error): message = error.localizedDescription
        case .success(let urls):
            guard let url = urls.first, let storage = storage else { return }
            busy = true; message = "Copying your private bundle into the app. Keep Boundary 07 open."
            Task {
                do {
                    _ = try await Task.detached(priority: .userInitiated) { try storage.importFolder(url) }.value
                    imported = true; busy = false
                    message = "Private bundle imported and runtime checksums verified."
                } catch { busy = false; message = error.localizedDescription }
            }
        }
    }
    func receive(_ object: [String: Any]) {
        switch object["type"] as? String {
        case "capabilities":
            guard let data = object["report"] as? [String: Any] else { return }
            checksPassed = data["allRequiredPassed"] as? Bool == true
            if let bytes = try? JSONSerialization.data(withJSONObject: data, options: [.prettyPrinted, .sortedKeys]), let text = String(data: bytes, encoding: .utf8) { report = text }
            message = checksPassed ? "Required features passed. Real game execution still needs testing." : "This WebKit context is missing a required feature. See the device report."
        case "running": running = true; message = "WebAssembly runtime started. Menu and match play still need device verification."
        case "error": message = object["message"] as? String ?? "The game runtime stopped."
        case "import": showingWeb = false; DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { self.pickingFolder = true }
        default: break
        }
    }
    func key(_ code: String, down: Bool) {
        let allowed = ["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "KeyS", "KeyD", "KeyA", "KeyW", "ShiftLeft", "Escape"]
        guard allowed.contains(code) else { return }
        webView?.evaluateJavaScript("window.boundaryTouch?.('\(code)',\(down ? "true" : "false"))", completionHandler: nil)
    }
    var reportURL: URL? {
        guard let directory = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first else { return nil }
        let file = directory.appendingPathComponent("Boundary07-device-report.json")
        do { try report.write(to: file, atomically: true, encoding: .utf8); return file } catch { return nil }
    }
}

@main struct Boundary07App: App {
    @StateObject private var model = BoundaryModel()
    var body: some Scene { WindowGroup { PavilionView(model: model).preferredColorScheme(.dark) } }
}

struct PavilionView: View {
    @ObservedObject var model: BoundaryModel
    @State private var showReport = false
    private let lime = Color(red: 0.83, green: 0.96, blue: 0.36)
    var body: some View {
        GeometryReader { geometry in
            ScrollView {
                HStack(alignment: .center, spacing: 35) {
                    VStack(alignment: .leading, spacing: 12) {
                        Text("BOUNDARY 07").font(.system(size: 20, weight: .black)).foregroundStyle(lime)
                        Text("TAKE THE\nCREASE.").font(.system(size: 48, weight: .black, design: .rounded)).lineSpacing(-5)
                        Text("Your private matchday.\nCricket 07, on your terms.").foregroundStyle(.secondary)
                        Text(model.message).font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                        if model.busy { ProgressView() }
                    }.frame(maxWidth: .infinity, alignment: .leading)
                    VStack(alignment: .leading, spacing: 12) {
                        Button { model.pickingFolder = true } label: {
                            Label(model.imported ? "Replace private bundle" : "Import private bundle", systemImage: "folder.badge.plus")
                                .frame(maxWidth: .infinity).padding(5)
                        }.disabled(model.busy).buttonStyle(.bordered)
                        Text("Select the folder containing bundle.json, Runtime and Game in Files.").font(.caption2).foregroundStyle(.secondary)
                        Button { model.open("probe.html") } label: {
                            Label("Check this device", systemImage: "checkmark.shield").frame(maxWidth: .infinity).padding(5)
                        }.buttonStyle(.bordered).disabled(model.baseURL == nil || model.busy)
                        Button { model.open("index.html") } label: {
                            Label("Open the pavilion", systemImage: "play.fill").frame(maxWidth: .infinity).padding(7)
                        }.buttonStyle(.borderedProminent).tint(lime).foregroundStyle(.black)
                            .disabled(!model.imported || !model.checksPassed || model.busy)
                        HStack {
                            Label(model.imported ? "Imported" : "No game files", systemImage: model.imported ? "checkmark.circle" : "circle")
                            Spacer()
                            Button("Device report") { showReport = true }
                        }.font(.caption2).foregroundStyle(.secondary)
                        Text("This app imports your files locally. The downloaded host contains no game code or assets.")
                            .font(.caption2).foregroundStyle(.secondary)
                    }.frame(maxWidth: .infinity)
                }.padding(28).frame(minHeight: geometry.size.height)
            }.background(Color(red: 0.06, green: 0.08, blue: 0.06))
        }
        .fileImporter(isPresented: $model.pickingFolder, allowedContentTypes: [.folder], allowsMultipleSelection: false, onCompletion: model.importSelected)
        .fullScreenCover(isPresented: $model.showingWeb, onDismiss: { model.running = false; model.webView = nil; UIApplication.shared.isIdleTimerDisabled = false }) {
            GameScreen(model: model)
        }
        .sheet(isPresented: $showReport) {
            NavigationStack {
                ScrollView { Text(model.report).font(.system(.caption, design: .monospaced)).textSelection(.enabled).padding() }
                    .navigationTitle("Device report")
                    .toolbar {
                        ToolbarItem(placement: .confirmationAction) { Button("Done") { showReport = false } }
                        ToolbarItem(placement: .bottomBar) { if let url = model.reportURL { ShareLink(item: url) { Label("Export report", systemImage: "square.and.arrow.up") } } }
                    }
            }
        }
    }
}

struct GameScreen: View {
    @ObservedObject var model: BoundaryModel
    @State private var touchVisible = true
    var body: some View {
        ZStack(alignment: .bottom) {
            if let base = model.baseURL { BoundaryWebView(model: model, url: base.appendingPathComponent(model.page)).ignoresSafeArea() }
            if model.running && touchVisible { TouchControls(model: model).padding(.horizontal, 20).padding(.bottom, 12) }
        }.overlay(alignment: .topLeading) {
            Button { model.showingWeb = false } label: { Label("Pavilion", systemImage: "chevron.left").font(.caption.bold()).padding(9).background(.ultraThinMaterial, in: Capsule()) }
                .padding(12)
        }.overlay(alignment: .topTrailing) {
            if model.running {
                Button { touchVisible.toggle() } label: { Label("Controls", systemImage: "gamecontroller").font(.caption.bold()).padding(9).background(.ultraThinMaterial, in: Capsule()) }.padding(12)
            }
        }.onChange(of: model.running) { _, running in UIApplication.shared.isIdleTimerDisabled = running }
    }
}

struct TouchControls: View {
    @ObservedObject var model: BoundaryModel
    var body: some View {
        HStack(alignment: .bottom) {
            VStack(spacing: 3) {
                TouchKey(label: "↑", code: "ArrowUp", model: model)
                HStack(spacing: 3) { TouchKey(label: "←", code: "ArrowLeft", model: model); TouchKey(label: "↓", code: "ArrowDown", model: model); TouchKey(label: "→", code: "ArrowRight", model: model) }
            }
            Spacer()
            HStack(spacing: 6) {
                TouchKey(label: "Esc", code: "Escape", model: model)
                TouchKey(label: "W", code: "KeyW", model: model)
                TouchKey(label: "Loft", code: "ShiftLeft", model: model)
                TouchKey(label: "A", code: "KeyA", model: model)
                TouchKey(label: "D", code: "KeyD", model: model)
                TouchKey(label: "S", code: "KeyS", model: model)
            }
        }
    }
}

struct TouchKey: View {
    let label: String
    let code: String
    @ObservedObject var model: BoundaryModel
    @State private var held = false
    var body: some View {
        Text(label).font(.system(size: 15, weight: .bold)).frame(width: 43, height: 39)
            .background(held ? Color(red: 0.83, green: 0.96, blue: 0.36) : Color.black.opacity(0.55), in: RoundedRectangle(cornerRadius: 10))
            .foregroundStyle(held ? Color.black : Color.white)
            .overlay(RoundedRectangle(cornerRadius: 10).stroke(.white.opacity(0.25)))
            .accessibilityLabel(label)
            .gesture(DragGesture(minimumDistance: 0).onChanged { _ in if !held { held = true; model.key(code, down: true) } }
                .onEnded { _ in held = false; model.key(code, down: false) })
            .onDisappear { if held { model.key(code, down: false); held = false } }
    }
}
