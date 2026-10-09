#!/usr/bin/env bash
# macOS ships bash 3.2; this script needs 4.4 or newer (brew install bash).
if (( BASH_VERSINFO[0] < 4 || (BASH_VERSINFO[0] == 4 && BASH_VERSINFO[1] < 4) )); then
    for candidate in /opt/homebrew/bin/bash /usr/local/bin/bash; do
        if [[ -x $candidate ]]; then exec "$candidate" "$0" "$@"; fi
    done
    echo 'bash 4.4 or newer is needed (macOS: brew install bash).' >&2; exit 1
fi
set -euo pipefail
cd -- "$(dirname -- "$0")"
if [[ ${1:-} == --software ]]; then
    shift
    if [[ -z ${VK_DRIVER_FILES:-} ]]; then
        for candidate in /run/opengl-driver/share/vulkan/icd.d/lvp_icd*.json /usr/share/vulkan/icd.d/lvp_icd*.json; do
            if [[ -f $candidate ]]; then export VK_DRIVER_FILES=$candidate; break; fi
        done
    fi
    if [[ -z ${VK_DRIVER_FILES:-} ]]; then echo 'Lavapipe not found; set VK_DRIVER_FILES.' >&2; exit 1; fi
    export VK_LOADER_LAYERS_DISABLE='~implicit~'
fi
# macOS: on MoltenVK; natively on Apple Silicon (BB_ARCH=arm64), or as an x86-64 process under Rosetta 2.
if [[ $(uname -s) == Darwin ]]; then
    # Apple Silicon: the native arm64 build (bbcpu translates the game's code); BB_ARCH=x86_64
    # runs the Rosetta 2 build instead. build.sh, started below, follows it.
    if [[ -z ${BB_ARCH:-} ]]; then
        [[ $(sysctl -n hw.optional.arm64 2>/dev/null) == 1 ]] && BB_ARCH=arm64 || BB_ARCH=x86_64
    fi
    export BB_ARCH
    # The MoltenVK from scripts/macos/build-deps.sh, unless a Vulkan driver is chosen.
    # BB_VK_DRIVER=kosmickrisp (native arm64, macOS 26+): Mesa's Vulkan-on-Metal 4 driver from
    # scripts/macos/build-kosmickrisp.sh instead.
    icd=$PWD/deps/macos-$BB_ARCH/share/vulkan/icd.d/MoltenVK_icd.json
    if [[ ${BB_VK_DRIVER:-} == kosmickrisp ]]; then
        icd=$(ls "$PWD"/deps/macos-arm64-kk/share/vulkan/icd.d/kosmickrisp*.json 2>/dev/null | head -1)
        [[ -n $icd ]] || { echo 'KosmicKrisp not built (scripts/macos/build-kosmickrisp.sh)' >&2; exit 1; }
    fi
    if [[ -z ${VK_DRIVER_FILES:-} && -f $icd ]]; then export VK_DRIVER_FILES=$icd; fi
    export MVK_CONFIG_LOG_LEVEL=${MVK_CONFIG_LOG_LEVEL:-1} # errors only
    # Rosetta runs AVX/AVX2/F16C/FMA/BMI code but hides them from CPUID unless asked: the game
    # then sees the features of the PS4's CPU.
    export ROSETTA_ADVERTISE_AVX=${ROSETTA_ADVERTISE_AVX:-1}
    # The new memory model maps dma-buf chunks of GPU memory: Linux only.
    if [[ ${BB_GUEST_IN_PLACE:-0} == 1 ]]; then
        echo 'New memory model: not available on macOS; the default model is used' >&2
    fi
    export BB_GUEST_IN_PLACE=0
fi
# BB_PREBUILT=1 (packaged builds, the AppImage): out/bb-probe and its GPU library are installed
# next to this script; nothing is built and no nix-shell is needed.
# BB_DATA_DIR: writable directory for the generated files (out/), saves (user/) and bbport.ini;
# by default this directory.
data=${BB_DATA_DIR:-.}
out=$data/out
mkdir -p "$out"
export BB_CONFIG=${BB_CONFIG:-$data/bbport.ini}
# FSR 4.1.1 assets (tools/fsr4cap/build_assets.sh): next to run.sh or in the data directory.
if [[ -z ${BB_FSR411_DIR:-} && ! -d fsr4_411 && -d $data/fsr4_411 ]]; then
    export BB_FSR411_DIR=$data/fsr4_411
fi
if [[ -z ${BB_PREBUILT:-} && -z ${BB_IN_NIX_SHELL:-} ]] && ! { command -v pkg-config >/dev/null && pkg-config --exists vulkan sdl3; } && command -v nix-shell >/dev/null; then
    args=''; if (( $# )); then args=$(printf '%q ' "$@"); fi
    exec env BB_IN_NIX_SHELL=1 nix-shell shell.nix --run "bash run.sh $args"
fi
# BB_SAVE_LOG=1 (launcher: "Save the log and statistics to a file"): this run's output and its
# per-frame statistics also go to $data/logs/<time>.log, .frames.csv and .readbacks.csv.
if [[ ${BB_SAVE_LOG:-} == 1 ]]; then
    logs=$data/logs
    mkdir -p "$logs"
    printf -v stamp '%(%Y%m%d_%H%M%S)T' -1
    export BB_FRAME_STATS=1 BB_FRAME_LOG=${BB_FRAME_LOG:-$logs/$stamp.frames.csv}
    export BB_READBACK_LOG=${BB_READBACK_LOG:-$logs/$stamp.readbacks.csv}
    echo "Log: $logs/$stamp.log"
    exec 3>"$logs/$stamp.log"
    # Bash builtins only (the AppImage's PATH has no tee).
    exec > >(while IFS= read -r line || [[ -n $line ]]; do printf '%s\n' "$line"; printf '%s\n' "$line" >&3; done) 2>&1
fi
if [[ -z ${PYTHON:-} ]]; then
    PYTHON=$(command -v python3 || true)
    if [[ -z $PYTHON ]]; then
        for candidate in /nix/store/*-python3-*/bin/python3; do
            if [[ -x $candidate ]]; then PYTHON=$candidate; break; fi
        done
    fi
fi
if [[ -z ${PYTHON:-} ]]; then echo 'Install Python 3 or set PYTHON.' >&2; exit 1; fi
# BB_GAME_DIR: the game's folder (eboot.bin, sce_module, ...); default next to this directory.
game=${BB_GAME_DIR:-../CUSA03173}
if [[ ! -f $game/eboot.bin ]]; then echo "No eboot.bin in $game (set BB_GAME_DIR)." >&2; exit 1; fi
original_game=$game
game=$("$PYTHON" scripts/mods.py "$game" --out "$out" \
    --mods-dir "${BB_MODS_DIR:-$data/mods}" --config "${BB_MODS_CONFIG:-$data/mods.json}" \
    --enabled "${BB_MODS_ENABLED:-1}")
# A private merged view lasts for this launch, including restarts. Cleanup only our own view.
if [[ $game != "$(realpath "$original_game")" ]]; then
    mod_game=$game
    trap '"$PYTHON" -c '\''import shutil,sys; shutil.rmtree(sys.argv[1])'\'' "$mod_game"' EXIT
fi
"$PYTHON" scripts/prepare.py "$game" --out "$out"
"$PYTHON" scripts/link_libc.py "$game" --out "$out"
"$PYTHON" scripts/link_modules.py "$game" --out "$out"
"$PYTHON" scripts/content_profile.py "$game" --out "$out" --sku "${BB_CONTENT_SKU:-full}"
# Sizes chosen below for the previous launch are recomputed after an in-game restart.
if [[ ${BB_AUTO_RENDER_RES:-} == 1 ]]; then
    unset BB_RENDER_RES BB_OUTPUT_RES BB_AUTO_RENDER_RES
fi
# BB_RENDER_RES=WxH explicitly sets the game's render resolution (a patch at start).
# Frame rate: BB_FPS=uncap (default; delta-time patch, vblank 480 Hz, frames shown at once),
# 60/90 (fixed-timestep patches) or 30 (unpatched). BB_PATCHES adds patch names ("a;b").
fps=${BB_FPS:-uncap}
# bbport.ini output_res other than 1080p (720p for the Steam Deck, 1440p, 2160p): the whole game
# renders at the preset's size of the output (a patch), the upscaler fills the output, the UI is
# drawn at the output size. Preset and output changes need a restart.
# Live resolution changes keep the guest at 1920x1080 and scale host targets at run time instead
# (output and presets change in the menu without a restart, but post-processing stays at 1080p
# and scene targets are copied back: much slower on the Steam Deck and older GPUs). Chosen by
# BB_LIVE_RES=0/1, else bbport.ini live_resolution=0/1/auto (auto: the GPU check, strong
# discrete GPUs get them); off when unset. TAA and native 1080p always use the live path.
if [[ -z ${BB_RENDER_RES:-} ]]; then
    read -r scaled_render scaled_output < <("$PYTHON" scripts/patches.py --print-scaled --settings "$BB_CONFIG") || true
fi
live=0
if [[ -n ${scaled_output:-} ]]; then
    live=${BB_LIVE_RES:-}
    # Bash builtins only: the AppImage's PATH has no sed/grep (a missing one ended run.sh silently).
    if [[ -z $live && -f $BB_CONFIG ]]; then
        while IFS= read -r line || [[ -n $line ]]; do
            [[ $line =~ ^live_resolution=([01]|auto)$ ]] && live=${BASH_REMATCH[1]}
        done < "$BB_CONFIG"
    fi
    if [[ $live == auto ]]; then
        if [[ -n ${BB_PROBE:-} ]]; then caps=$(dirname -- "$BB_PROBE")/bb-gpu-capabilities
        elif [[ -n ${BB_PREBUILT:-} ]]; then caps=bin/bb-gpu-capabilities
        else caps=out/bb-gpu-capabilities; fi
        live=$("$caps" --live-resolution 2> >(while IFS= read -r line; do
            [[ $line == *MANGOHUD* ]] || printf '%s\n' "$line"; done >&2)) || live=0
    fi
    [[ $live == 1 ]] || live=0
fi
if [[ $live == 1 ]]; then
    echo "Output ${scaled_output}: live resolution changes (live_resolution=0: startup patch)"
elif [[ -n ${scaled_output:-} ]]; then
    export BB_RENDER_RES=$scaled_render BB_OUTPUT_RES=$scaled_output BB_AUTO_RENDER_RES=1
    # Outputs above 1080p need more direct memory for their targets; at 1080p the default
    # (9152 MiB there ran the clinic at 7 FPS instead of 36 on an 18 GB M3 Pro).
    if (( ${scaled_output%x*} > 1920 )); then
        export BB_DMEM_MB=${BB_DMEM_MB:-9152}
    fi
    echo "Output ${scaled_output}: scene ${scaled_render}, direct memory ${BB_DMEM_MB:-default} MiB (live_resolution=1: live changes)"
fi
"$PYTHON" scripts/patches.py --out "$out" --fps "$fps" --extra "${BB_PATCHES:-}" --settings "$BB_CONFIG" --game-dir "$game" --render-res "${BB_RENDER_RES:-}" --output-res "${BB_OUTPUT_RES:-}" \
    --patches-dir "${BB_PATCHES_DIR:-$data/patches}" --patches-config "${BB_PATCHES_CONFIG:-$data/patches.json}"
# Background upload of the game's GPU memory into VRAM ahead of use (BufferCache::Preupload):
# 1 = only memory already in VRAM that the game rewrote (no new VRAM), 2 = all of it (~3 GB more
# VRAM), 0 = off. BB_GUEST_GPU_MEMORY=1 puts guest direct memory in GPU-visible dma-buf chunks
# (BB_GUEST_IN_PLACE below implies it: it commits all allocated memory up front and rules out BB_UFFD).
export BB_PREUPLOAD=${BB_PREUPLOAD:-1}
# Memory model. BB_GUEST_IN_PLACE=1 (experimental; the launcher's "New memory model"): as a PC game,
# the GPU uses the game's memory where it is (GPU-visible system memory) and keeps the data it reads
# often in VRAM, giving VRAM back when it is no longer used. Tested only on an RX 7800 XT and the
# Steam Deck; NVIDIA cannot map its memory as needed and falls back. 0 (default): the model of 0.2
# (VRAM copies of the game's memory, write tracking).
export BB_GUEST_IN_PLACE=${BB_GUEST_IN_PLACE:-0}
# MangoHud (launcher switch: MANGOHUD=1) must be drawn once. Two overlays on top of each other
# showed doubled, offset text: the Steam Deck's performance overlay (mangoapp, game mode) plus the
# in-game layer, or the AppImage's bundled layer plus a system MangoHud (the layer names differ,
# so the Vulkan loader loads both).
if [[ ${MANGOHUD:-0} == 1 ]]; then
    if pgrep -x mangoapp > /dev/null 2>&1; then
        echo "MangoHud: Steam's performance overlay is on; the in-game MangoHud stays off"
        unset MANGOHUD
        export DISABLE_MANGOHUD=1
    elif grep -qs '"VK_LAYER_MANGOHUD' /usr/share/vulkan/implicit_layer.d/*.json \
            /etc/vulkan/implicit_layer.d/*.json \
            "${XDG_DATA_HOME:-$HOME/.local/share}"/vulkan/implicit_layer.d/*.json; then
        # The system's own MangoHud (and its config) is used; the bundled one is skipped.
        export VK_LOADER_LAYERS_DISABLE=${VK_LOADER_LAYERS_DISABLE:+$VK_LOADER_LAYERS_DISABLE,}VK_LAYER_MANGOHUD_overlay_64_x86_64
    fi
fi
# Write tracking with userfaultfd write-protection instead of mprotect (no address-space write lock:
# a streaming burst re-protected thousands of pages, ~10-20 ms); falls back to mprotect when the
# kernel lacks it. Off by default for now: the Steam Deck crashed 4 times in 9 minutes with it
# (2026-10-04, PM4 type 0 = zeroed command buffers, heap corruption); 1 turns it on.
export BB_UFFD=${BB_UFFD:-0}
# The command buffers of each submission are copied when the game submits them and decoded from
# the copy: while an area loads the game reused that memory before the GPU thread got to it
# ("Unimplemented PM4 type 0" a few seconds after loading a save). 0 decodes guest memory.
export BB_COPY_GPU_BUFFERS=${BB_COPY_GPU_BUFFERS:-1}
# A guest write next to small GPU outputs the GPU is still writing (counters, compute results,
# up to BB_GPU_WRITE_TWINS_MAX bytes per page) does not wait for the GPU (BufferCache twins).
export BB_GPU_WRITE_TWINS=${BB_GPU_WRITE_TWINS:-1} BB_GPU_WRITE_TWINS_MAX=${BB_GPU_WRITE_TWINS_MAX:-65536}
if [[ -z ${BB_VBLANK_HZ:-} ]]; then
    case $fps in uncap) export BB_VBLANK_HZ=480 ;; 90) export BB_VBLANK_HZ=90 ;; *) export BB_VBLANK_HZ=60 ;; esac
fi
# FSR 4: faster post passes next to the downloaded ones (incremental; tools/fsr4_optimize.sh).
if [[ -z ${BB_PREBUILT:-} && -d fsr4_shaders ]] && command -v spirv-cross >/dev/null; then
    bash tools/fsr4_optimize.sh || echo 'FSR 4: optimized post passes not built' >&2
fi
if [[ -n ${BB_PREBUILT:-} ]]; then
    probe=${BB_PROBE:-bin/bb-probe}
else
    bash build.sh
    probe=out/bb-probe
fi
probe_args=("$out/boot-linked.bin" --content-profile "$out/content.bin" --patches "$out/patches.bin" --app0 "$game" --user "${BB_USER_DIR:-$data/user}" --timeout "${BB_TIMEOUT:-0}" "$@")
if [[ -n ${mod_game:-} ]]; then
    "$probe" "${probe_args[@]}" &
    mod_pid=$!
    trap 'kill -TERM "$mod_pid" 2>/dev/null || true' TERM INT
    mod_status=0
    wait "$mod_pid" || mod_status=$?
    # An interrupted wait must finish the child before removing its mounted view.
    if kill -0 "$mod_pid" 2>/dev/null; then wait "$mod_pid" || mod_status=$?; fi
    exit "$mod_status"
fi
exec "$probe" "${probe_args[@]}"
