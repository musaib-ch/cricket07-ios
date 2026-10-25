import Foundation
import Metal

/// Bounded RGBA8 texture staging for the diagnostic iOS renderer.
/// This does not interpret texture stages or bind textures to game draws.
final class MetalTextureStore {
    private struct Entry {
        let texture: MTLTexture
        let bytes: Int
    }

    private let device: MTLDevice
    private var entries: [UInt32: Entry] = [:]
    private var residentBytes = 0
    private let maxResidentBytes = 64 * 1024 * 1024
    private let maxTextures = 256

    init(device: MTLDevice) { self.device = device }

    /// The decoder returns offsets into this caller-owned buffer. All levels
    /// are uploaded synchronously before the Data storage is released.
    /// Invalid input or allocation failure leaves existing textures intact.
    @discardableResult
    func upload(command: Data) -> Bool {
        var decoded = Cricket07TextureUpload()
        let valid = command.withUnsafeBytes { raw -> Bool in
            guard let base = raw.baseAddress else { return false }
            return cricket07_decode_texture_upload(base, raw.count, &decoded) == 1
        }
        guard valid, decoded.texture_id != 0 else { return false }
        let id = decoded.texture_id
        let levels = Int(decoded.levels)
        guard levels > 0, levels <= 14 else { return false }
        var cost = 0
        for i in 0..<levels {
            let bytes = withUnsafePointer(to: &decoded.pixel_bytes) {
                $0.withMemoryRebound(to: Int.self, capacity: 14) { $0[i] }
            }
            guard bytes <= maxResidentBytes - cost else { return false }
            cost += bytes
        }
        let previous = entries[id]?.bytes ?? 0
        guard residentBytes >= previous,
              cost <= maxResidentBytes - (residentBytes - previous),
              entries[id] != nil || entries.count < maxTextures else { return false }

        let widths = withUnsafePointer(to: &decoded.width) {
            $0.withMemoryRebound(to: UInt32.self, capacity: 14) { Array(UnsafeBufferPointer(start: $0, count: 14)) }
        }
        let heights = withUnsafePointer(to: &decoded.height) {
            $0.withMemoryRebound(to: UInt32.self, capacity: 14) { Array(UnsafeBufferPointer(start: $0, count: 14)) }
        }
        let offsets = withUnsafePointer(to: &decoded.pixel_offset) {
            $0.withMemoryRebound(to: Int.self, capacity: 14) { Array(UnsafeBufferPointer(start: $0, count: 14)) }
        }
        let descriptor = MTLTextureDescriptor.texture2DDescriptor(
            pixelFormat: .rgba8Unorm, width: Int(widths[0]),
            height: Int(heights[0]), mipmapped: levels > 1)
        descriptor.mipmapLevelCount = levels
        descriptor.usage = [.shaderRead]
        descriptor.storageMode = .shared
        guard let texture = device.makeTexture(descriptor: descriptor) else { return false }

        let copied = command.withUnsafeBytes { raw -> Bool in
            guard let base = raw.baseAddress else { return false }
            for level in 0..<levels {
                let width = Int(widths[level])
                let height = Int(heights[level])
                let offset = offsets[level]
                guard width > 0, height > 0, offset <= raw.count,
                      width <= Int.max / 4,
                      height <= (raw.count - offset) / (width * 4) else { return false }
                texture.replace(region: MTLRegionMake2D(0, 0, width, height),
                                mipmapLevel: level,
                                withBytes: base.advanced(by: offset),
                                bytesPerRow: width * 4)
            }
            return true
        }
        guard copied else { return false }
        entries[id] = Entry(texture: texture, bytes: cost)
        residentBytes = residentBytes - previous + cost
        return true
    }

    func texture(id: UInt32) -> MTLTexture? { entries[id]?.texture }

    func remove(id: UInt32) {
        if let entry = entries.removeValue(forKey: id) {
            residentBytes -= entry.bytes
        }
    }
}
