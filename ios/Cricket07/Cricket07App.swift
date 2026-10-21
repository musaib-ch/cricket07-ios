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
    private let bridge = ControllerBridge()
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
            Text(controllerConnected ? "Controller connected" : "Connect a Bluetooth controller")
            Text("Controller key events received: \(eventCount)")
                .monospacedDigit()
        }
        .padding(30)
        .onAppear {
            bridge.start()
            controllerConnected = !GCController.controllers().isEmpty
        }
        .onDisappear { bridge.stop() }
        .task {
            while !Task.isCancelled {
                eventCount += bridge.drain().count
                controllerConnected = !GCController.controllers().isEmpty
                try? await Task.sleep(nanoseconds: 100_000_000)
            }
        }
    }
}
