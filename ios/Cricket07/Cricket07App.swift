import SwiftUI
import GameController

@main
struct Cricket07FoundationApp: App {
    var body: some Scene {
        WindowGroup {
            FoundationView()
        }
    }
}

/// Temporary native shell for integration testing. No game runtime is linked yet.
struct FoundationView: View {
    @State private var controllerConnected = !GCController.controllers().isEmpty
    @State private var eventCount = 0
    @State private var memoryProbeResult = "Not tested"
    @State private var cpuProbeResult = "Not tested"
    @State private var metalStatus = "Not initialized"
    private let bridge = ControllerBridge()
    @State private var touchEvents = 0
    var body: some View {
        VStack(spacing: 18) {
            Text("Cricket 07 • iOS Foundation").font(.title.bold())
            Text("Native runtime linking probe. Game engine execution and assets are not integrated.")
                .multilineTextAlignment(.center)
            Text(cricket07_runtime_linked() == 1 ? "C runtime symbol linked" : "C runtime unavailable")
            Text(cricket07_runtime_registry_smoke_test() == 1 ? "Runtime registry smoke test passed" : "Runtime registry smoke test failed")
            Text("Guest memory: \(memoryProbeResult)")
            Button("Test guest virtual memory") {
                memoryProbeResult = "Testing"
                Task.detached(priority: .utility) {
                    let success = cricket07_guest_memory_probe() == 1
                    await MainActor.run {
                        memoryProbeResult = success ? "Reservation passed" : "Reservation failed"
                    }
                }
            }
            Text("CPU and stack: \(cpuProbeResult)")
            Button("Test guest CPU and stack") {
                cpuProbeResult = "Testing"
                Task.detached(priority: .utility) {
                    let success = cricket07_cpu_stack_probe() == 1
                    await MainActor.run {
                        cpuProbeResult = success ? "CPU and stack passed" : "CPU and stack failed"
                    }
                }
            }
            Text("Metal renderer: \(metalStatus)")
            MetalSurfaceView(status: $metalStatus)
                .frame(height: 100)
                .cornerRadius(10)
            GameFilesImportView()
            TouchGamepadView { key, pressed in
                if cricket07_controller_enqueue_vk(key, pressed ? 1 : 0) == 1 {
                    touchEvents += 1
                }
            }
            Text("Touch events queued: \(touchEvents)")
            Text(controllerConnected ? "Controller connected" : "Connect a Bluetooth controller")
            Text("Controller key events received: \(eventCount)")
                .monospacedDigit()
        }
        .padding(30)
        .onAppear {
            cricket07_controller_reset()
            bridge.start()
            controllerConnected = !GCController.controllers().isEmpty
        }
        .onDisappear {
            bridge.stop()
            cricket07_controller_reset()
        }
        .task {
            while !Task.isCancelled {
                let events = bridge.drain()
                // Keep controller callbacks off the translated game thread.
                // The native queue stores HID events for a future engine pump.
                for event in events {
                    _ = cricket07_controller_enqueue_vk(event.virtualKey, event.pressed ? 1 : 0)
                }
                eventCount += events.count
                controllerConnected = !GCController.controllers().isEmpty
                try? await Task.sleep(nanoseconds: 100_000_000)
            }
        }
    }
}
