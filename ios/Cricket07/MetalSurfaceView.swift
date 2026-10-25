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
        private var textureStore: MetalTextureStore?
        private var pipelineStates: [UInt64: MTLRenderPipelineState] = [:]
        private var vertexShader: MTLFunction?
        private var fragmentShader: MTLFunction?
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
            if let library = device.makeDefaultLibrary() {
                vertexShader = library.makeFunction(name: "cricket07_staged_vertex")
                fragmentShader = library.makeFunction(name: "cricket07_test_fragment")
            }
            // Exercise RGBA8 mip allocation without binding textures to draws.
            // This uses generated pixels, never user-owned Cricket 07 assets.
            let store = MetalTextureStore(device: device)
            var sample = [UInt8](repeating: 0, count: 256)
            var sampleLength = 0
            let generated = sample.withUnsafeMutableBytes { raw in
                cricket07_make_test_texture_upload(
                    raw.baseAddress, raw.count, &sampleLength) == 1
            }
            let textureReady = generated &&
                store.upload(command: Data(sample.prefix(sampleLength))) &&
                store.texture(id: 1) != nil
            textureStore = store

            // Queue a complete synthetic draw frame. Only the supported
            // diagnostic command subset is consumed by this Metal view.
            let submitted = cricket07_submit_test_triangle_frame() == 1
            status.wrappedValue = submitted
                ? (textureReady
                   ? "Diagnostic draw queued; RGBA8 mip texture allocated (not sampled)"
                   : "Diagnostic draw queued; texture allocation failed")
                : "Metal initialized; diagnostic draw frame failed"
        }

        // The Direct3D pipeline key is translated once per unique blend/mask
        // combination. These are immutable Metal pipeline properties, not
        // dynamic encoder state. Unsupported D3D modes are rejected in C.
        private func blendFactor(_ value: UInt32) -> MTLBlendFactor? {
            switch value {
            case 1: return .zero
            case 2: return .one
            case 3: return .sourceColor
            case 4: return .oneMinusSourceColor
            case 5: return .sourceAlpha
            case 6: return .oneMinusSourceAlpha
            case 7: return .destinationAlpha
            case 8: return .oneMinusDestinationAlpha
            case 9: return .destinationColor
            case 10: return .oneMinusDestinationColor
            case 11: return .sourceAlphaSaturated
            default: return nil
            }
        }

        private func blendOperation(_ value: UInt32) -> MTLBlendOperation? {
            switch value {
            case 1: return .add
            case 2: return .subtract
            case 3: return .reverseSubtract
            case 4: return .min
            case 5: return .max
            default: return nil
            }
        }

        private func pipeline(for state: Cricket07DrawState,
                              view: MTKView) -> MTLRenderPipelineState? {
            let key = UInt64(state.color_write_mask)
                | (UInt64(state.blend) << 4)
                | (UInt64(state.src_blend) << 5)
                | (UInt64(state.dst_blend) << 9)
                | (UInt64(state.blend_op) << 13)
            if let cached = pipelineStates[key] { return cached }
            guard let device = view.device, let vertexShader, let fragmentShader else {
                return nil
            }
            let descriptor = MTLRenderPipelineDescriptor()
            descriptor.vertexFunction = vertexShader
            descriptor.fragmentFunction = fragmentShader
            descriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat
            descriptor.depthAttachmentPixelFormat = view.depthStencilPixelFormat
            guard let attachment = descriptor.colorAttachments[0] else { return nil }
            var mask: MTLColorWriteMask = []
            if state.color_write_mask & 1 != 0 { mask.insert(.red) }
            if state.color_write_mask & 2 != 0 { mask.insert(.green) }
            if state.color_write_mask & 4 != 0 { mask.insert(.blue) }
            if state.color_write_mask & 8 != 0 { mask.insert(.alpha) }
            attachment.writeMask = mask
            if state.blend != 0 {
                guard let src = blendFactor(state.src_blend),
                      let dst = blendFactor(state.dst_blend),
                      let op = blendOperation(state.blend_op) else { return nil }
                attachment.isBlendingEnabled = true
                attachment.sourceRGBBlendFactor = src
                attachment.destinationRGBBlendFactor = dst
                attachment.rgbBlendOperation = op
                // D3D8 uses the same blend factors and operation for alpha.
                // Source-alpha-saturate contributes ONE to source alpha.
                attachment.sourceAlphaBlendFactor =
                    state.src_blend == 11 ? .one : src
                attachment.destinationAlphaBlendFactor = dst
                attachment.alphaBlendOperation = op
            }
            guard let result = try? device.makeRenderPipelineState(descriptor: descriptor) else {
                return nil
            }
            pipelineStates[key] = result
            return result
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
                var diffuse = [Float](repeating: 0, count: 4096)
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
                        diffuse.withUnsafeMutableBufferPointer { colors in
                            calls.withUnsafeMutableBufferPointer { d in
                                cricket07_take_colored_viewport_triangle_frame(
                                    c.baseAddress, c.count, v.baseAddress, v.count,
                                    colors.baseAddress, colors.count,
                                    &count, &frame, d.baseAddress, d.count, &callCount,
                                    UInt32(pixelWidth), UInt32(pixelHeight)) == 1
                            }
                        }
                    }
                }
                if found && count > 0 {
                    // Metal receives tightly packed position+diffuse float4 pairs.
                    // C validates both arrays before this upload.
                    var active = [Float](repeating: 0, count: count * 8)
                    for i in 0..<count {
                        for channel in 0..<4 {
                            active[i * 8 + channel] = vertices[i * 4 + channel]
                            active[i * 8 + 4 + channel] = diffuse[i * 4 + channel]
                        }
                    }
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
                                self?.status.wrappedValue = "Diagnostic CMD_DRAW: transformed positions and diffuse colors"
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
            if let stagedVertices, stagedVertexCount > 0,
               !stagedCalls.isEmpty {
                encoder.setVertexBuffer(stagedVertices, offset: 0, index: 0)
                encoder.setFrontFacing(.clockwise)
                for call in stagedCalls {
                    guard let device = view.device,
                          let depthState = depthState(for: call.state, device: device),
                          let drawPipeline = pipeline(for: call.state, view: view) else {
                        continue
                    }
                    encoder.setRenderPipelineState(drawPipeline)
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
