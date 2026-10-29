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
        // Keep the game surface native-app-like even when an older/private web
        // bundle does not include the current touch-controls.js file.
        let touchGuard = #"""
        (() => {
          const stop = e => { e.preventDefault(); e.stopPropagation(); };
          for (const name of ['gesturestart','gesturechange','gestureend','dblclick'])
            document.addEventListener(name, stop, {capture:true, passive:false});
          document.addEventListener('touchmove', e => {
            if (e.touches && e.touches.length > 1) stop(e);
          }, {capture:true, passive:false});
          const meta = document.createElement('meta'); meta.name='viewport';
          meta.content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover';
          document.head.appendChild(meta);
          const fallback = () => {
            if (document.getElementById('b07-controls') || document.getElementById('b07-native-controls')) return;
            const root=document.createElement('div'); root.id='b07-native-controls';
            root.style.cssText='position:fixed;inset:0;z-index:2147483000;pointer-events:none;user-select:none;touch-action:none;font:600 12px -apple-system,BlinkMacSystemFont,sans-serif';
            const css=document.createElement('style'); css.textContent='#b07-native-controls button{pointer-events:auto;border:1px solid #ffffff55;border-radius:14px;background:#18221ddd;color:#fff;min-width:48px;min-height:44px;font-weight:700}#b07-native-controls .pad{position:absolute;left:18px;bottom:24px;display:grid;grid-template-columns:repeat(3,48px);gap:6px}#b07-native-controls .pad button:nth-child(1){grid-column:2}#b07-native-controls .pad button:nth-child(4){grid-column:2}#b07-native-controls .face{position:absolute;right:18px;bottom:42px;display:grid;grid-template-columns:repeat(3,48px);gap:6px}#b07-native-controls .face button:nth-child(1){grid-column:2}#b07-native-controls .face button:nth-child(4){grid-column:2}.sys{position:absolute;left:50%;bottom:20px;transform:translateX(-50%);display:flex;gap:8px}.loft{position:absolute;right:18px;bottom:150px}.toggle{position:absolute;right:18px;top:18px;min-width:92px!important;min-height:34px!important}#b07-native-controls.hidden>*:not(.toggle){display:none!important}#b07-native-controls button.on{background:#a9d34b;color:#071006}'; root.append(css);
            root.insertAdjacentHTML('beforeend','<button class="toggle">CONTROLS</button><button class="loft" data-k="ShiftLeft">LOFT OFF</button><div class="pad"><button data-k="ArrowUp">▲</button><button data-k="ArrowLeft">◀</button><button data-k="ArrowRight">▶</button><button data-k="ArrowDown">▼</button></div><div class="face"><button data-k="KeyW">Y</button><button data-k="KeyA">X</button><button data-k="KeyD">B</button><button data-k="KeyS">A</button></div><div class="sys"><button data-k="Tab">SELECT</button><button data-k="Escape">START</button></div>'); document.body.append(root);
            const map={ArrowUp:'ArrowUp',ArrowDown:'ArrowDown',ArrowLeft:'ArrowLeft',ArrowRight:'ArrowRight',KeyW:'w',KeyA:'a',KeyD:'d',KeyS:'s',ShiftLeft:'Shift',Tab:'Tab',Escape:'Escape'}; const held=new Map();
            const release=el=>{const k=el.dataset.k;if(!k)return; if(held.get(el)){window.boundaryTouch?.(k,false);window.dispatchEvent(new KeyboardEvent('keyup',{key:map[k],code:k,bubbles:true}));held.delete(el)}el.classList.remove('on')};
            root.querySelectorAll('[data-k]').forEach(el=>{el.addEventListener('pointerdown',e=>{e.preventDefault();e.stopPropagation();const k=el.dataset.k;if(!held.get(el)){window.boundaryTouch?.(k,true);window.dispatchEvent(new KeyboardEvent('keydown',{key:map[k],code:k,bubbles:true}));held.set(el,true);el.classList.add('on')}el.setPointerCapture?.(e.pointerId)},{passive:false});['pointerup','pointercancel','lostpointercapture'].forEach(n=>el.addEventListener(n,e=>{e.preventDefault();release(el)},{passive:false}))});
            root.querySelector('.loft').addEventListener('click',e=>{e.currentTarget.classList.toggle('on');e.currentTarget.textContent=e.currentTarget.classList.contains('on')?'LOFT ON':'LOFT OFF'});
            root.querySelector('.toggle').addEventListener('click',()=>{root.classList.toggle('hidden');root.querySelector('.toggle').textContent=root.classList.contains('hidden')?'SHOW CONTROLS':'CONTROLS'});
            window.addEventListener('blur',()=>root.querySelectorAll('[data-k]').forEach(release));
          };
          let attempts=0; const probe=()=>{ if(document.getElementById('b07-controls')) return; if(window.BoundaryControls?.start) window.BoundaryControls.start(); if(++attempts<20) setTimeout(probe,250); else fallback(); }; setTimeout(probe,500);
        })();
        """#
        config.userContentController.addUserScript(WKUserScript(source: String(touchGuard), injectionTime: .atDocumentEnd, forMainFrameOnly: true))
        let web = WKWebView(frame: .zero, configuration: config)
        web.isOpaque = false; web.backgroundColor = .black
        web.scrollView.contentInsetAdjustmentBehavior = .never
        web.scrollView.bouncesZoom = false
        web.scrollView.pinchGestureRecognizer?.isEnabled = false
        web.scrollView.minimumZoomScale = 1
        web.scrollView.maximumZoomScale = 1
        web.pageZoom = 1
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
