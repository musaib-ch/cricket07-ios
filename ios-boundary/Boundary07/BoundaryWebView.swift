import SwiftUI
import WebKit

struct BoundaryWebView: UIViewRepresentable {
    @ObservedObject var model: BoundaryModel
    let url: URL
    func makeCoordinator() -> Coordinator { Coordinator(model: model, base: model.baseURL!) }
    func makeUIView(context: Context) -> WKWebView {
        let config = WKWebViewConfiguration()
        config.websiteDataStore = .default()
        config.allowsInlineMediaPlayback = true
        config.mediaTypesRequiringUserActionForPlayback = []
        config.userContentController.add(context.coordinator, name: "boundary")
        let web = WKWebView(frame: .zero, configuration: config)
        web.isOpaque = false; web.backgroundColor = .black
        web.scrollView.contentInsetAdjustmentBehavior = .never
        web.navigationDelegate = context.coordinator
        model.webView = web
        web.load(URLRequest(url: url, cachePolicy: .reloadIgnoringLocalCacheData))
        return web
    }
    func updateUIView(_ web: WKWebView, context: Context) {}
    static func dismantleUIView(_ web: WKWebView, coordinator: Coordinator) {
        web.stopLoading(); web.configuration.userContentController.removeScriptMessageHandler(forName: "boundary")
        web.loadHTMLString("", baseURL: nil)
    }
    @MainActor final class Coordinator: NSObject, WKNavigationDelegate, WKScriptMessageHandler {
        let model: BoundaryModel
        let base: URL
        init(model: BoundaryModel, base: URL) { self.model = model; self.base = base }
        private func allowed(_ url: URL?) -> Bool {
            guard let url = url else { return false }
            return url.scheme == base.scheme && url.host == base.host && url.port == base.port && url.path.hasPrefix(base.path)
        }
        func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
            guard message.frameInfo.isMainFrame, allowed(message.frameInfo.request.url), let data = message.body as? [String: Any] else { return }
            model.receive(data)
        }
        func webView(_ webView: WKWebView, decidePolicyFor action: WKNavigationAction, decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
            decisionHandler(allowed(action.request.url) ? .allow : .cancel)
        }
        func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) { model.message = error.localizedDescription }
        func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: Error) { model.message = error.localizedDescription }
        func webViewWebContentProcessDidTerminate(_ webView: WKWebView) {
            model.running = false; model.message = "WebKit stopped the game process. Export the device report; memory or runtime compatibility needs investigation."
        }
    }
}
