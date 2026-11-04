// Simulates a user on the 3D view: right click (enter move mode), hold W, release.
// usage: swift input.swift <x> <y>   (global screen coordinates in points)
import Cocoa

let args = CommandLine.arguments
let x = Double(args[1]) ?? 600, y = Double(args[2]) ?? 400
let p = CGPoint(x: x, y: y)
func post(_ e: CGEvent?) { e?.post(tap: .cghidEventTap) }

post(CGEvent(mouseEventSource: nil, mouseType: .mouseMoved, mouseCursorPosition: p, mouseButton: .left))
usleep(400_000)
post(CGEvent(mouseEventSource: nil, mouseType: .leftMouseDown, mouseCursorPosition: p, mouseButton: .left))
post(CGEvent(mouseEventSource: nil, mouseType: .leftMouseUp, mouseCursorPosition: p, mouseButton: .left))
usleep(600_000)
print("right click"); fflush(stdout)
post(CGEvent(mouseEventSource: nil, mouseType: .rightMouseDown, mouseCursorPosition: p, mouseButton: .right))
usleep(50_000)
post(CGEvent(mouseEventSource: nil, mouseType: .rightMouseUp, mouseCursorPosition: p, mouseButton: .right))
usleep(1_200_000)
print("W down"); fflush(stdout)
post(CGEvent(keyboardEventSource: nil, virtualKey: 13, keyDown: true))   // 13 = W
usleep(2_000_000)
post(CGEvent(keyboardEventSource: nil, virtualKey: 13, keyDown: false))
print("W up"); fflush(stdout)
usleep(1_000_000)
print("D down"); fflush(stdout)
post(CGEvent(keyboardEventSource: nil, virtualKey: 2, keyDown: true))    // 2 = D
usleep(1_000_000)
post(CGEvent(keyboardEventSource: nil, virtualKey: 2, keyDown: false))
usleep(800_000)
print("done"); fflush(stdout)
