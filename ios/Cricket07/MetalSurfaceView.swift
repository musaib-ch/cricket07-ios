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
        var channels = [Float](repeating: 0, count: 4)
        var frameNumber: UInt32 = 0
        let decoded = channels.withUnsafeMutableBufferPointer { buffer in
            cricket07_sample_frame_stream(buffer.baseAddress, buffer.count, &frameNumber) == 1
        }
        if decoded {
            view.clearColor = MTLClearColor(red: Double(channels[0]), green: Double(channels[1]), blue: Double(channels[2]), alpha: Double(channels[3]))
        } else {
            view.clearColor = MTLClearColor(red: 0.06, green: 0.14, blue: 0.19, alpha: 1)
        }
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
        private var testPipeline: MTLRenderPipelineState?
        private var hasPresented = false
        private var consumedGeneration: UInt64 = 0
        private let status: Binding<String>

        init(status: Binding<String>) { self.status = status }

        func configure(view: MTKView) {
            guard let device = view.device, let queue = device.makeCommandQueue() else {
                status.wrappedValue = "Metal unavailable"
                return
            }
            self.queue = queue
            if let library = device.makeDefaultLibrary(),
               let vertex = library.makeFunction(name: "cricket07_test_vertex"),
               let fragment = library.makeFunction(name: "cricket07_test_fragment") {
                let descriptor = MTLRenderPipelineDescriptor()
                descriptor.vertexFunction = vertex
                descriptor.fragmentFunction = fragment
                descriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat
                testPipeline = try? device.makeRenderPipelineState(descriptor: descriptor)
            }
            // Synthetic frame exercises producer -> mailbox -> Metal consumer.
            // Real game-thread producer integration remains outstanding.
            let submitted = cricket07_submit_test_frame() == 1
            status.wrappedValue = submitted ? "Metal initialized; test frame queued" : "Metal initialized; frame test failed"
        }

        func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}

        func draw(in view: MTKView) {
            var channels = [Float](repeating: 0, count: 4)
            var frameNumber: UInt32 = 0
            let received = channels.withUnsafeMutableBufferPointer { buffer in
                cricket07_take_frame(buffer.baseAddress, buffer.count, &frameNumber, &consumedGeneration) == 1
            }
            if received {
                view.clearColor = MTLClearColor(red: Double(channels[0]), green: Double(channels[1]), blue: Double(channels[2]), alpha: Double(channels[3]))
            }
            guard let drawable = view.currentDrawable,
                  let descriptor = view.currentRenderPassDescriptor,
                  let command = queue?.makeCommandBuffer(),
                  let encoder = command.makeRenderCommandEncoder(descriptor: descriptor) else {
                return
            }
            if let testPipeline {
                encoder.setRenderPipelineState(testPipeline)
                encoder.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 3)
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
