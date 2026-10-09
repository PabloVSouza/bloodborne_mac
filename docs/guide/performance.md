# Performance

[← Documentation](../README.md)

Measured in the clinic area at the start of the game, on an Apple M3 Pro (18-core GPU) with a
1080p output:

| Setting | FPS |
|---|---|
| Native 1080p | ~31 |
| FSR 3.1, *Balanced* (renders 1130×636), character motion vectors off | ~44 |

- The frame rate is uncapped by default (a community frame-time patch). It can be capped on the
  **Game** tab.
- At 1080p the game is limited by the GPU, so the upscaler preset matters most. See
  [Settings](settings.md#recommended-settings).
- Only one machine has been measured so far. Results on other Macs are welcome.

The details of what was measured and changed, and what could still be gained, are in
[macOS performance](../MACOS_PERFORMANCE.md).
