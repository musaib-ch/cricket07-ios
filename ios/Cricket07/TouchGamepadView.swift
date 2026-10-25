import SwiftUI

/// Touch controls emit the same virtual-key events as Bluetooth controllers.
/// They enter the native HID queue; live gameplay still requires the game runtime.
struct TouchGamepadView: View {
    let send: (UInt16, Bool) -> Void

    var body: some View {
        HStack(alignment: .bottom, spacing: 22) {
            VStack(spacing: 4) {
                key("▲", 0x26)
                HStack(spacing: 4) {
                    key("◀", 0x25)
                    key("▼", 0x28)
                    key("▶", 0x27)
                }
            }
            Spacer(minLength: 8)
            VStack(spacing: 8) {
                key("ESC", 0x1B, width: 72)
                HStack(spacing: 12) {
                    key("A", 0x0D)
                    key("B", 0x20)
                }
            }
        }
        .padding(12)
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 18))
        .accessibilityElement(children: .contain)
        .accessibilityLabel("On-screen game controls")
    }

    private func key(_ title: String, _ code: UInt16, width: CGFloat = 54) -> some View {
        TouchKey(title: title, width: width) { pressed in send(code, pressed) }
    }
}

private struct TouchKey: View {
    let title: String
    let width: CGFloat
    let onChange: (Bool) -> Void
    @State private var held = false

    var body: some View {
        Text(title)
            .font(.headline.bold())
            .frame(width: width, height: 50)
            .foregroundStyle(.white)
            .background(held ? Color.blue : Color.black.opacity(0.75), in: RoundedRectangle(cornerRadius: 12))
            .contentShape(Rectangle())
            .gesture(DragGesture(minimumDistance: 0)
                .onChanged { _ in
                    if !held { held = true; onChange(true) }
                }
                .onEnded { _ in
                    if held { held = false; onChange(false) }
                })
            .onDisappear {
                if held { held = false; onChange(false) }
            }
            .accessibilityLabel(title)
            .accessibilityAddTraits(.isButton)
    }
}
