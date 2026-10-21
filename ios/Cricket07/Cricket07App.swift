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
    private let bridge = ControllerBridge()
    var body: some View {
        VStack(spacing: 18) {
            Text("Cricket 07 • iOS Foundation").font(.title.bold())
            Text("Native shell only. Game engine and assets are not integrated.")
                .multilineTextAlignment(.center)
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
