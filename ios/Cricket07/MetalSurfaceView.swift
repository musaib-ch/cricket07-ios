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
        private var depthStates: [UInt32: MTLDepthStencilState] = [:]
        private var stagedVertices: MTLBuffer?
        private var stagedVertexCount = 0
        private var stagedCalls: [Cricket07DrawCall] = []
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
            }
            // Queue a complete synthetic draw frame. Only the supported
            // diagnostic command subset is consumed by this Metal view.
            let submitted = cricket07_submit_test_triangle_frame() == 1
            status.wrappedValue = submitted
                ? "Metal initialized; diagnostic draw frame queued"
                : "Metal initialized; diagnostic draw frame failed"
        }

        // Cache immutable Metal depth states. Direct3D compare values are
        // normalized and validated by the C draw-state bridge.
        private func depthState(for state: Cricket07DrawState,
                                device: MTLDevice) -> MTLDepthStencilState? {
            let key = state.depth_compare * 2 + state.depth_write
            if let cached = depthStates[key] { return cached }
            let compare: MTLCompareFunction
            switch state.depth_compare {
            case 1: compare = .never
            case 2: compare = .less
            case 3: compare = .equal
            case 4: compare = .lessEqual
            case 5: compare = .greater
            case 6: compare = .notEqual
            case 7: compare = .greaterEqual
            case 8: compare = .always
            default: return nil
            }
            let descriptor = MTLDepthStencilDescriptor()
            descriptor.depthCompareFunction = compare
            descriptor.isDepthWriteEnabled = state.depth_write != 0
            guard let result = device.makeDepthStencilState(descriptor: descriptor) else {
                return nil
            }
            depthStates[key] = result
            return result
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
                var calls = [Cricket07DrawCall](repeating: Cricket07DrawCall(), count: 64)
                var callCount = 0
                let pixelWidth = Int(view.drawableSize.width)
                let pixelHeight = Int(view.drawableSize.height)
                guard pixelWidth > 0 && pixelHeight > 0 &&
                      pixelWidth <= 16384 && pixelHeight <= 16384 else { return }
                let found = color.withUnsafeMutableBufferPointer { c in
                    vertices.withUnsafeMutableBufferPointer { v in
                        calls.withUnsafeMutableBufferPointer { d in
                            cricket07_take_viewport_triangle_frame(
                                c.baseAddress, c.count, v.baseAddress, v.count,
                                &count, &frame, d.baseAddress, d.count, &callCount,
                                UInt32(pixelWidth), UInt32(pixelHeight)) == 1
                        }
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
                        stagedCalls = Array(calls.prefix(callCount))
                        view.clearColor = MTLClearColor(red: Double(color[0]), green: Double(color[1]),
                                                      blue: Double(color[2]), alpha: Double(color[3]))
                        if !hasReceivedDraw {
                            hasReceivedDraw = true
                            DispatchQueue.main.async { [weak self] in
                                self?.status.wrappedValue = "Diagnostic CMD_DRAW transformed to Metal clip space"
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
            if let testPipeline, let stagedVertices, stagedVertexCount > 0,
               !stagedCalls.isEmpty {
                encoder.setRenderPipelineState(testPipeline)
                encoder.setVertexBuffer(stagedVertices, offset: 0, index: 0)
                encoder.setFrontFacing(.clockwise)
                for call in stagedCalls {
                    guard let device = view.device,
                          let depthState = depthState(for: call.state, device: device) else {
                        continue
                    }
                    encoder.setDepthStencilState(depthState)
                    switch call.state.cull_mode {
                    case 1: encoder.setCullMode(.front)
                    case 2: encoder.setCullMode(.back)
                    default: encoder.setCullMode(.none)
                    }
                    let vp = call.viewport
                    encoder.setViewport(MTLViewport(originX: vp.x, originY: vp.y,
                                                    width: vp.width, height: vp.height,
                                                    znear: vp.min_depth, zfar: vp.max_depth))
                    encoder.drawPrimitives(type: .triangle,
                                           vertexStart: Int(call.first_vertex),
                                           vertexCount: Int(call.vertex_count))
                }
            }
            encoder.endEncoding()
            command.present(drawable)
            command.commit()
        }
    }
}
