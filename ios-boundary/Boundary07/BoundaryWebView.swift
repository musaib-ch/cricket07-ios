import SwiftUI
import WebKit

struct BoundaryWebView: UIViewRepresentable {
    @ObservedObject var model: BoundaryModel
    let url: URL
    func makeCoordinator() -> Coordinator { Coordinator(model: model, base: model.baseURL!) }
    func makeUIView(context: Context) -> WKWebView {
        let config = WKWebViewConfiguration()
        config.websiteDataStore = .default()
        config.ignoresViewportScaleLimits = false
        config.allowsInlineMediaPlayback = true
        config.mediaTypesRequiringUserActionForPlayback = []
        config.userContentController.add(context.coordinator, name: "boundary")
        let web = WKWebView(frame: .zero, configuration: config)
        web.isOpaque = false; web.backgroundColor = .black
        web.scrollView.contentInsetAdjustmentBehavior = .never
        web.scrollView.bouncesZoom = false
        web.scrollView.pinchGestureRecognizer?.isEnabled = false
        web.isMultipleTouchEnabled = true
        web.navigationDelegate = context.coordinator
        model.webView = web
        web.load(URLRequest(url: url, cachePolicy: .reloadIgnoringLocalCacheData))
        return web
    }
    func updateUIView(_ web: WKWebView, context: Context) {}
    static func dismantleUIView(_ web: WKWebView, coordinator: Coordinator) {
        web.stopLoading(); web.configuration.userContentController.removeScriptMessageHandler(forName: "boundary")
        // Permit our own teardown navigation so the old page and its wasm workers actually unload.
        web.navigationDelegate = nil
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
        func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
            webView.pageZoom = 1
            webView.scrollView.minimumZoomScale = 1
            webView.scrollView.maximumZoomScale = 1
            webView.scrollView.setZoomScale(1, animated: false)
            webView.scrollView.pinchGestureRecognizer?.isEnabled = false
        }
        private func navigationFailed(_ error: Error) {
            if (error as NSError).code != NSURLErrorCancelled { model.gameProblem = error.localizedDescription; model.launching = false }
        }
        func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) { navigationFailed(error) }
        func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: Error) { navigationFailed(error) }
        func webViewWebContentProcessDidTerminate(_ webView: WKWebView) {
            model.running = false; model.launching = false
            model.gameProblem = "iOS stopped the game process, possibly because of memory pressure. Restart in Performance mode. Previously saved files are kept; an unsaved match cannot be restored."
            model.message = model.gameProblem!
        }
    }
}
