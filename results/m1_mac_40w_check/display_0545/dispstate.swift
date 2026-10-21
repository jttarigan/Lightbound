import CoreGraphics
var ids = [CGDirectDisplayID](repeating: 0, count: 8); var n: UInt32 = 0
CGGetOnlineDisplayList(8, &ids, &n)
var parts: [String] = []
for i in 0..<Int(n) { let d = ids[i]; let m = CGDisplayCopyDisplayMode(d)
  parts.append("\(CGDisplayIsBuiltin(d) != 0 ? "internal" : "external"):\(CGDisplayPixelsWide(d))x\(CGDisplayPixelsHigh(d))@\(Int(m?.refreshRate ?? 0))Hz:\(CGDisplayIsAsleep(d) != 0 ? "asleep" : "awake")") }
print(parts.joined(separator: " "))
