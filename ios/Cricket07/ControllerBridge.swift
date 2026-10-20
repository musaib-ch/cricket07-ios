import Foundation
import GameController

/// Translates physical gamepad events into a small platform-neutral key queue.
/// The native runtime must consume these events on its game thread.
final class ControllerBridge {
    struct KeyEvent {
        let virtualKey: UInt16
        let pressed: Bool
    }
    private var pending: [KeyEvent] = []
    private let lock = NSLock()
    private var observed: [ObjectIdentifier: GCController] = [:]
    private var held: Set<UInt16> = []

    func start() {
        NotificationCenter.default.addObserver(self, selector: #selector(connected(_:)), name: .GCControllerDidConnect, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(disconnected(_:)), name: .GCControllerDidDisconnect, object: nil)
        GCController.controllers().forEach(configure)
    }
    func stop() {
        NotificationCenter.default.removeObserver(self)
        observed.values.forEach { $0.extendedGamepad?.valueChangedHandler = nil }
        observed.removeAll()
        lock.lock()
        pending.removeAll()
        held.removeAll()
        lock.unlock()
    }
    @objc private func connected(_ note: Notification) {
        guard let controller = note.object as? GCController else { return }
        configure(controller)
    }
    @objc private func disconnected(_ note: Notification) {
        guard let controller = note.object as? GCController else { return }
        controller.extendedGamepad?.valueChangedHandler = nil
        observed.removeValue(forKey: ObjectIdentifier(controller))
        releaseAll()
    }
    private func configure(_ controller: GCController) {
        guard let pad = controller.extendedGamepad else { return }
        observed[ObjectIdentifier(controller)] = controller
        pad.valueChangedHandler = { [weak self] gamepad, element in
            guard let self else { return }
            // Default mapping is deliberately conservative. Batting/bowling bindings
            // must be verified against the user's Cricket 07 control configuration.
            if element === gamepad.dpad || element === gamepad.leftThumbstick {
                let x = gamepad.dpad.xAxis.value + gamepad.leftThumbstick.xAxis.value
                let y = gamepad.dpad.yAxis.value + gamepad.leftThumbstick.yAxis.value
                self.setKey(0x25, down: x < -0.5) // VK_LEFT
                self.setKey(0x27, down: x > 0.5)  // VK_RIGHT
                self.setKey(0x26, down: y > 0.5)  // VK_UP
                self.setKey(0x28, down: y < -0.5) // VK_DOWN
            }
            if element === gamepad.buttonA { self.setKey(0x0D, down: gamepad.buttonA.isPressed) } // Enter
            if element === gamepad.buttonB { self.setKey(0x1B, down: gamepad.buttonB.isPressed) } // Escape
            if element === gamepad.buttonMenu { self.setKey(0x1B, down: gamepad.buttonMenu.isPressed) }
        }
    }
    private func setKey(_ key: UInt16, down: Bool) {
        lock.lock()
        defer { lock.unlock() }
        if down {
            guard held.insert(key).inserted else { return }
        } else {
            guard held.remove(key) != nil else { return }
        }
        pending.append(KeyEvent(virtualKey: key, pressed: down))
    }
    private func releaseAll() {
        lock.lock()
        for key in held { pending.append(KeyEvent(virtualKey: key, pressed: false)) }
        held.removeAll()
        lock.unlock()
    }
    func drain() -> [KeyEvent] {
        lock.lock()
        defer { lock.unlock() }
        let result = pending
        pending.removeAll(keepingCapacity: true)
        return result
    }
}
