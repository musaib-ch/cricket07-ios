import SwiftUI
import MetalKit

/// Diagnostic Metal consumer for a synthetic CLEAR/DRAW/PRESENT frame.
/// No real game graphics or engine execution is implemented here.
struct MetalSurfaceView: UIViewRepresentable {
    @Binding var status: String

    func makeCoordinator() -> Coordinator { Coordinator(status: $status) }

    func makeUIView(context: Context) -> MTKView {
        let view = MTKView(frame: .zero)
        view.device = MTLCreateSystemDefaultDevice()
        view.colorPixelFormat = .bgra8Unorm
        // Diagnostic depth attachment for the first untextured triangle pipeline.
        // Real game depth-state commands still require translation.
        view.depthStencilPixelFormat = .depth32Float
        view.clearDepth = 1.0
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
        private var diagnosticDepthState: MTLDepthStencilState?
        private var stagedVertices: MTLBuffer?
        private var stagedVertexCount = 0
        private var hasReceivedDraw = false
        private let status: Binding<String>

        init(status: Binding<String>) { self.status = status }

        func configure(view: MTKView) {
            guard let device = view.device, let queue = device.makeCommandQueue() else {
                status.wrappedValue = "Metal unavailable"
                return
            }
            self.queue = queue
            if let library = device.makeDefaultLibrary(),
               let vertex = library.makeFunction(name: "cricket07_staged_vertex"),
               let fragment = library.makeFunction(name: "cricket07_test_fragment") {
                let descriptor = MTLRenderPipelineDescriptor()
                descriptor.vertexFunction = vertex
                descriptor.fragmentFunction = fragment
                descriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat
                descriptor.depthAttachmentPixelFormat = view.depthStencilPixelFormat
                testPipeline = try? device.makeRenderPipelineState(descriptor: descriptor)
                let depth = MTLDepthStencilDescriptor()
                depth.depthCompareFunction = .lessEqual
                depth.isDepthWriteEnabled = true
                diagnosticDepthState = device.makeDepthStencilState(descriptor: depth)
            }
            // Queue a complete synthetic draw frame. Only the supported
            // diagnostic command subset is consumed by this Metal view.
            let submitted = cricket07_submit_test_triangle_frame() == 1
            status.wrappedValue = submitted
                ? "Metal initialized; diagnostic draw frame queued"
                : "Metal initialized; diagnostic draw frame failed"
        }

        func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}

        func draw(in view: MTKView) {
            // Prefer a queued supported CMD_DRAW frame. The real game producer
            // is still not linked; diagnostic sample geometry is the fallback.
            if let device = view.device {
                var color = [Float](repeating: 0, count: 4)
                var vertices = [Float](repeating: 0, count: 4096)
                var count = 0
                var frame: UInt32 = 0
                let found = color.withUnsafeMutableBufferPointer { c in
                    vertices.withUnsafeMutableBufferPointer { v in
                        cricket07_take_triangle_frame(c.baseAddress, c.count,
                                                      v.baseAddress, v.count, &count, &frame) == 1
                    }
                }
                if found && count > 0 {
                    let required = count * 4
                    let active = Array(vertices.prefix(required))
                    let buffer = active.withUnsafeBytes { bytes -> MTLBuffer? in
                        guard let base = bytes.baseAddress else { return nil }
                        return device.makeBuffer(bytes: base, length: bytes.count, options: [])
                    }
                    if let buffer {
                        stagedVertices = buffer
                        stagedVertexCount = count
                        view.clearColor = MTLClearColor(red: Double(color[0]), green: Double(color[1]),
                                                      blue: Double(color[2]), alpha: Double(color[3]))
                        if !hasReceivedDraw {
                            hasReceivedDraw = true
                            DispatchQueue.main.async { [weak self] in
                                self?.status.wrappedValue = "Diagnostic CMD_DRAW decoded; Metal buffer ready"
                            }
                        }
                    }
                }
            }
            guard let drawable = view.currentDrawable,
                  let descriptor = view.currentRenderPassDescriptor,
                  let command = queue?.makeCommandBuffer(),
                  let encoder = command.makeRenderCommandEncoder(descriptor: descriptor) else {
                return
            }
            if let testPipeline, let stagedVertices, stagedVertexCount > 0 {
                encoder.setRenderPipelineState(testPipeline)
                if let diagnosticDepthState {
                    encoder.setDepthStencilState(diagnosticDepthState)
                }
                encoder.setVertexBuffer(stagedVertices, offset: 0, index: 0)
                encoder.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: stagedVertexCount)
            }
            encoder.endEncoding()
            command.present(drawable)
            command.commit()
        }
    }
}
