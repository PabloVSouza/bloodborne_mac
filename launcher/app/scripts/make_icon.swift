// Draws the app icon into an .iconset folder: the source of the launcher's icons (src-tauri/icons).
// a pale crescent moon over a dark red night, on the macOS rounded-square shape.
//   swift launcher/app/scripts/make_icon.swift OUT.iconset
// then: npx tauri icon OUT.iconset/icon_512x512@2x.png (src-tauri/icons; keep the macOS ones).
import AppKit

func render(_ size: Int) -> Data {
    let s = CGFloat(size)
    let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: size, pixelsHigh: size,
                               bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
                               colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
    let ctx = NSGraphicsContext.current!.cgContext

    // Body: the rounded square, inset as macOS app icons are.
    let inset = s * 0.1
    let body = CGRect(x: inset, y: inset, width: s - 2 * inset, height: s - 2 * inset)
    let shape = CGPath(roundedRect: body, cornerWidth: body.width * 0.225,
                       cornerHeight: body.width * 0.225, transform: nil)
    ctx.saveGState()
    ctx.addPath(shape)
    ctx.clip()
    let colors = [CGColor(red: 0.30, green: 0.03, blue: 0.05, alpha: 1),
                  CGColor(red: 0.06, green: 0.01, blue: 0.02, alpha: 1)] as CFArray
    let gradient = CGGradient(colorsSpace: CGColorSpaceCreateDeviceRGB(), colors: colors,
                              locations: [0, 1])!
    ctx.drawRadialGradient(gradient, startCenter: CGPoint(x: body.midX, y: body.midY + body.height * 0.15),
                           startRadius: 0, endCenter: CGPoint(x: body.midX, y: body.midY),
                           endRadius: body.width * 0.75, options: .drawsAfterEndLocation)
    // The moon: a disc with a darker disc cut out of it.
    let r = body.width * 0.27
    let center = CGPoint(x: body.midX + r * 0.1, y: body.midY + r * 0.15)
    let moon = CGRect(x: center.x - r, y: center.y - r, width: 2 * r, height: 2 * r)
    let bite = CGRect(x: center.x - r * 0.55, y: center.y - r * 0.6, width: 2 * r * 0.95,
                      height: 2 * r * 0.95)
    // A faint glow around the whole disc, then the crescent (the disc minus the bite).
    ctx.setFillColor(CGColor(red: 1, green: 0.85, blue: 0.7, alpha: 0.10))
    ctx.fillEllipse(in: moon.insetBy(dx: -r * 0.25, dy: -r * 0.25))
    ctx.setFillColor(CGColor(red: 1, green: 0.85, blue: 0.7, alpha: 0.10))
    ctx.fillEllipse(in: moon.insetBy(dx: -r * 0.12, dy: -r * 0.12))
    ctx.addEllipse(in: moon)
    ctx.clip()
    ctx.addEllipse(in: moon)
    ctx.addEllipse(in: bite)
    ctx.setFillColor(CGColor(red: 0.96, green: 0.91, blue: 0.80, alpha: 1))
    ctx.fillPath(using: .evenOdd)
    ctx.restoreGState()

    // A thin rim.
    ctx.addPath(shape)
    ctx.setStrokeColor(CGColor(red: 0.55, green: 0.12, blue: 0.12, alpha: 0.6))
    ctx.setLineWidth(max(1, s * 0.006))
    ctx.strokePath()
    NSGraphicsContext.restoreGraphicsState()
    return rep.representation(using: .png, properties: [:])!
}

let out = URL(fileURLWithPath: CommandLine.arguments[1])
try FileManager.default.createDirectory(at: out, withIntermediateDirectories: true)
for base in [16, 32, 128, 256, 512] {
    try render(base).write(to: out.appendingPathComponent("icon_\(base)x\(base).png"))
    try render(base * 2).write(to: out.appendingPathComponent("icon_\(base)x\(base)@2x.png"))
}
