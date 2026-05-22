// Posts a synthetic mouse click at the given screen coordinates via
// CoreGraphics CGEvent. Used by test-harness.sh to drive the GD menu.
// System Events `click at` doesn't work for cocos2d games because they
// don't expose accessibility elements — CGEventPost goes through the
// HID input path that the game's own input handler reads from.
//
// Usage: ./click <x> <y>
// Build:  swiftc -O click.swift -o click

import CoreGraphics
import Foundation

guard CommandLine.arguments.count >= 3,
      let x = Double(CommandLine.arguments[1]),
      let y = Double(CommandLine.arguments[2]) else {
    FileHandle.standardError.write("usage: click <x> <y>\n".data(using: .utf8)!)
    exit(2)
}

let p = CGPoint(x: x, y: y)

// Move the cursor first so the click lands on the correct point AND any
// hover state the game tracks is updated. Skipping this can land the
// click correctly but leave the visible cursor in the wrong place, which
// is confusing to watch and can also miss hover-triggered hot-tracking
// behaviors in some menus.
CGWarpMouseCursorPosition(p)

guard let down = CGEvent(mouseEventSource: nil,
                          mouseType: .leftMouseDown,
                          mouseCursorPosition: p,
                          mouseButton: .left),
      let up   = CGEvent(mouseEventSource: nil,
                          mouseType: .leftMouseUp,
                          mouseCursorPosition: p,
                          mouseButton: .left) else {
    FileHandle.standardError.write("failed to create mouse events\n".data(using: .utf8)!)
    exit(3)
}

down.post(tap: .cghidEventTap)
// Tiny hold so the game registers a deliberate click rather than a flicker.
usleep(40_000)
up.post(tap: .cghidEventTap)
