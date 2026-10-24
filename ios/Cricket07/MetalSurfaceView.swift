import SwiftUI
import MetalKit

/// An iOS-native Metal surface smoke test.
/// Rendering a solid color proves device/surface/command-buffer presentation;
/// it DOES NOT render the Cricket 07 engine.
struct MetalSurfaceView: UIViewRepresentable {
    @Binding var status: String

    func makeCoordinator() -> Coordinator { Coordinator(status: $status) }

    func makeUIView(context: Context) -> MTKView {
        let view = MTKView(frame: .zero)
        view.device = MTLCreateSystemDefaultDevice()
        view.colorPixelFormat = .bgra8Unorm
        view.framebufferOnly = true
        view.clearColor = MTLClearColor(red: 0.06, green: 0.14, blue: 0.19, alpha: 1)
        view.preferredFramesPerSecond = 30
        view.isPaused = false
        view.enableSetNeedsDisplay = false
        view.delegate = context.coordinator
        context.coordinator.configure(view: view)
        return view
    }

    func updateUIView(_ uiView: MTKView, context: Context) {}

    final class Coordinator: NSObject, MTKViewDelegate {
        private var queue: MTLCommandQueue?
        private var hasPresented = false
        private let status: Binding<String>

        init(status: Binding<String>) { self.status = status }

        func configure(view: MTKView) {
            guard let device = view.device, let queue = device.makeCommandQueue() else {
                status.wrappedValue = "Metal unavailable"
                return
            }
            self.queue = queue
            status.wrappedValue = "Metal initialized"
        }

        func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}

        func draw(in view: MTKView) {
            guard let drawable = view.currentDrawable,
                  let descriptor = view.currentRenderPassDescriptor,
                  let command = queue?.makeCommandBuffer(),
                  let encoder = command.makeRenderCommandEncoder(descriptor: descriptor) else {
                return
            }
            encoder.endEncoding()
            command.present(drawable)
            command.commit()
            if !hasPresented {
                hasPresented = true
                DispatchQueue.main.async { [weak self] in
                    self?.status.wrappedValue = "Metal frame submitted"
                }
            }
        }
    }
}
