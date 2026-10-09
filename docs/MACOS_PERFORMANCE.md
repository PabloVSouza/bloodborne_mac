# macOS performance

Measurements on an Apple M3 Pro (18-core GPU, 18 GB), macOS 27, MoltenVK, native arm64 build,
in the clinic at the start of the game (`tools/mac_bench.sh`: boots, loads the save, holds,
reports the frame times). GPU breakdowns come from Metal System Traces (`TRACE=1`,
`BB_PASS_LABELS=1`, `tools/mst_passes.py`, `tools/mst_sequence.py`, `tools/mst_shaders.py`).

## Where it stands (2026-10-09)

| 1080p output | FPS |
|---|---|
| Native 1080p | ~31 |
| 720p, plain upscale | ~44–45 |
| FSR 3.1 preset 2 (1130×636), character motion vectors off | ~44 |
| FSR 3.1, character motion vectors on | ~36–37 |

At native 1080p the GPU is the limit: ~25 ms of work and 6–8 ms of idle time per frame, with
the game waiting on the previous frame for most of each frame. At 720p a large part of the work
does not scale with resolution (~10 ms: vertex work, shadow maps, small passes, copies, idle).

GPU time per frame at native 1080p:

| Work | ms |
|---|---|
| G-buffer pass | ~7.2 (vertex ~2.6) |
| Lighting (RGBA16F + depth) | ~4.1 |
| Compute dispatches | ~2.7 |
| Post-processing passes | ~5 |
| Image clears (separate passes) | ~1.2 |
| Copies (buffers and images) | ~2 |
| Idle between Metal encoders | 6–8 |

## Changes that helped

| Change | Effect |
|---|---|
| GPU memory kept resident in a Metal residency set (`gpu/shim/bbport_metal_residency.*`) | 9–14 FPS with 400–650 ms hitches → 16.8 FPS steady (Rosetta build) |
| No SPIR-V `NoContraction`: SPIRV-Cross turned it into unoptimizable `[[clang::optnone]]` add/multiply helpers in every shader | 16.7 → 30.4 FPS |
| Copy-shader copies recorded before the render pass instead of splitting it (`BB_HOIST_COPIES`), batched | ~1–2 ms |
| Upscaler presets at 1080p output render the game smaller from the start (the live path kept post-processing at 1080p) | FSR preset: ~31 → ~37–44 FPS |
| Larger direct memory only above 1080p output (9152 MiB ran the clinic at 7 FPS on 18 GB) | 7 → 36 FPS with a preset |
| Motion-vector positions stored with one plain store instead of four atomic exchanges | FSR + motion vectors 32.7 → 34.2 FPS |
| Character motion vectors off by default on macOS | FSR 36 → 42 FPS |

## Measured without gain

- Fast transcendental math (no `SignedZeroInfNanPreserve`, `BB_SHADER_INFNAN`).
- MoltenVK fast math, argument buffers off, prefill, more command buffers, Metal heaps,
  `BB_FRAMES_AHEAD=2`, anisotropic filtering off.
- Skipping draws that render into nothing (~13 a frame).
- Rect lists as triangles (`BB_RECTS_AS_TRIANGLES`): the game's heavy tessellated draws are real
  tessellation, not rect lists.
- Motion vectors: a smaller target (RGBA16F), no history stores, gating still objects. The cost
  is the motion variant's vertex outputs: a variant without them (`BB_MOTION_LITE=1`,
  measurement) runs as fast as no motion vectors.
- KosmicKrisp (Mesa's Vulkan on Metal 4): renders correctly, ~3× slower than MoltenVK.

## Where more could be gained

1. **Idle time between Metal encoders, 6–8 ms a frame at 1080p.** Each switch between render,
   compute and blit work leaves the GPU idle (~630 encoders a frame).
   - The game's tessellated light draws (~48 a frame) become a compute pass plus a render-pass
     restart each on MoltenVK: ~2–3 ms (removing them entirely: 31 → 34 FPS). Their hull shader
     uses constant factors (outer 3, inner 1), so they could be pre-tessellated into ordinary
     triangles (merging the LS, HS and DS stages in the recompiler).
   - Image clears as separate passes (~10 a frame, ~1.2 ms): fold them into the next pass's load
     action.
   - Small buffer copies between passes (~1–1.5 ms): batch further.
2. **Vertex work, ~6 ms a frame at native.** Vertex shaders read constants from storage buffers
   one value at a time; uniformly indexed read-only buffers declared as constant buffers might
   let Metal preload them. Some tiny passes (32×32, 64×64, 256×256 targets) are almost only
   vertex work (~1.5 ms).
3. **FSR overhead, ~3 ms over a plain upscale** (~1.7 ms the FSR passes, the rest idle time).
4. **Cheaper character motion vectors**, if wanted back: pass only the previous position and
   rebuild the current one from the pixel position (exact where the game's viewport is the
   render size: startup presets and native).

Expected: native 1080p in the mid-to-high 30s; FSR presets around 50 FPS. 60 FPS at native 1080p
would need much less GPU work than the game issues.
