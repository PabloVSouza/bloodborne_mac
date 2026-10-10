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
mkdir -p out
# macOS: BB_ARCH=arm64 (the default on Apple Silicon) builds the native program, where bbcpu
# translates the game's x86-64 code; BB_ARCH=x86_64 the Rosetta 2 one that runs it as it is
# (docs/ARM64_NATIVE.md). The libraries come from deps/macos-<arch> (scripts/macos/build-deps.sh);
# Homebrew's are arm64. Apple's clang: its LTO objects match the system linker.
macos=0
arch=()
if [[ $(uname -s) == Darwin ]]; then
    macos=1
    if [[ -z ${BB_ARCH:-} ]]; then
        [[ $(sysctl -n hw.optional.arm64 2>/dev/null) == 1 ]] && BB_ARCH=arm64 || BB_ARCH=x86_64
    fi
    macarch=$BB_ARCH
    deps=$PWD/deps/macos-$macarch
    if [[ ! -f $deps/.built-ffmpeg ]]; then BB_ARCH=$macarch bash scripts/macos/build-deps.sh; fi
    export PKG_CONFIG_LIBDIR=$deps/lib/pkgconfig:$deps/share/pkgconfig
    export PATH=$deps/bin:$PATH
    CC=${CC:-/usr/bin/clang}
    CXX=${CXX:-/usr/bin/clang++}
    arch=(-arch "$macarch")
fi
if [[ -z ${CC:-} ]]; then
    CC=$(command -v cc || command -v gcc || true)
    if [[ -z $CC ]]; then
        for candidate in /nix/store/*-gcc-wrapper-*/bin/gcc; do
            if [[ -x $candidate ]]; then CC=$candidate; break; fi
        done
    fi
fi
if [[ -z ${CC:-} ]]; then echo 'Install GCC/Clang or set CC.' >&2; exit 1; fi
# Dependencies come from pkg-config (Vulkan loader/headers, SDL3). On NixOS the
# environment is provided by shell.nix; re-enter it automatically if needed.
if ! { command -v pkg-config >/dev/null && pkg-config --exists vulkan sdl3 && command -v cmake >/dev/null && command -v ninja >/dev/null; }; then
    if [[ -z ${BB_IN_NIX_SHELL:-} ]] && command -v nix-shell >/dev/null; then
        exec env BB_IN_NIX_SHELL=1 nix-shell shell.nix --run "bash build.sh $*"
    fi
    echo 'Need pkg-config with vulkan and sdl3, cmake and ninja (see shell.nix).' >&2; exit 1
fi
read -r -a includes <<< "$(pkg-config --cflags vulkan sdl3)"
read -r -a libraries <<< "$(pkg-config --libs vulkan sdl3)"
# GPU library (shadPS4 video core + drivers), built by CMake into out/gpu/libbbgpu.so.
# BB_PGO: generate (instrumented build that writes pgo/ while the game runs), use, off.
# Default: use the profile in pgo/ when there is one. BB_LTO=OFF disables link-time optimization.
pgo=${BB_PGO:-}
if (( macos )); then pgo=off; fi # the profiles are GCC's
if [[ -z $pgo ]]; then
    if [[ -n $(find pgo -name '*.gcda' -print -quit 2>/dev/null) ]]; then pgo=use; else pgo=off; fi
fi
mkdir -p pgo
# Submodules (git clone --recursive, or: git submodule update --init) and this port's changes
# to FSR-Vulkan (gpu/patches/fsr-vulkan), applied to its working tree once. Not on macOS: they
# only instrument the FSR 4 provider, which Apple GPUs cannot run (the submodule stays clean).
if [[ ! -f gpu/third_party/fsr-vulkan/CMakeLists.txt || ! -f gpu/third_party/imgui/imgui.h ]]; then
    git submodule update --init --recursive
fi
if (( ! macos )); then
    for patch in gpu/patches/fsr-vulkan/*.patch; do
        if ! git -C gpu/third_party/fsr-vulkan apply --reverse --check "$PWD/$patch" 2>/dev/null; then
            git -C gpu/third_party/fsr-vulkan apply "$PWD/$patch"
        fi
    done
fi
cmake_platform=()
gpudir=out/gpu
if (( macos )) && [[ $macarch == arm64 ]]; then gpudir=out/gpu-arm64; fi
if (( macos )); then
    cmake_platform=(-DCMAKE_OSX_ARCHITECTURES="$macarch" -DCMAKE_PREFIX_PATH="$deps"
        -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX")
fi
cmake -S gpu -B "$gpudir" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBB_PGO="$pgo" \
    -DBB_LTO="${BB_LTO:-ON}" -DBB_PGO_DIR="$PWD/pgo" "${cmake_platform[@]}" >/dev/null
echo "GPU library: PGO $pgo, LTO ${BB_LTO:-ON}"
# A failed GPU build must stop here: an older libbbgpu.so would otherwise be used silently.
if ! ninja -C "$gpudir" bbgpu > out/gpu-build.log 2>&1; then
    grep -v '^\[' out/gpu-build.log | tail -40 >&2
    echo 'GPU library build failed (full log: out/gpu-build.log)' >&2; exit 1
fi
# Optional DLSS (NVIDIA RTX): DLSS_SDK_ROOT=<github.com/NVIDIA/DLSS checkout> builds the bridge
# (gpu/dlss_bridge, the only code using NVIDIA's SDK) and puts it next to bb-probe with NVIDIA's
# libnvidia-ngx-dlss.so. Without them the DLSS upscaler is listed as unavailable.
if [[ -n ${DLSS_SDK_ROOT:-} ]]; then
    cmake -S gpu/dlss_bridge -B out/dlss-bridge -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DDLSS_SDK_ROOT="$DLSS_SDK_ROOT" >/dev/null
    ninja -C out/dlss-bridge >/dev/null
    rm -f out/libnvidia-ngx-dlss.so.*
    cp out/dlss-bridge/libbbport_dlss.so "$DLSS_SDK_ROOT"/lib/Linux_x86_64/rel/libnvidia-ngx-dlss.so.* out/
    cp "$DLSS_SDK_ROOT/LICENSE.txt" out/NVIDIA-DLSS-LICENSE.txt
    echo "DLSS bridge: out/libbbport_dlss.so"
fi
# $ORIGIN/gpu: packaged copies keep the library next to the binary without patching it.
gpu=(-Lout/gpu -lbbgpu -Wl,-rpath,'$ORIGIN/gpu' -Wl,-rpath,"$PWD/out/gpu" -rdynamic)
# Linux: -no-pie keeps the loader and its brk heap below 1 TiB. macOS executables are always
# position-independent (and load at 4 GiB); the runtime keeps its heap low itself (runtime_heap.h).
probe_flags=(-no-pie)
warnings=()
if (( macos )); then
    gpu=(-L"$gpudir" -lbbgpu -Wl,-rpath,@executable_path/gpu -Wl,-rpath,"$PWD/$gpudir")
    probe_flags=()
    warnings=(-Wno-unknown-warning-option -Wno-unused-but-set-global)
    gpu+=(-framework Foundation) # platform.c: NSProcessInfo (bb_latency_critical)
fi
runtime=(src/runtime*.c src/platform.c)
# Third-party decoders: compiled once, without this project's -Werror policy.
atrac9=(third_party/LibAtrac9/C/src/*.c)
atrac9_lib=out/libatrac9.a
if (( macos )) && [[ $macarch == arm64 ]]; then atrac9_lib=out/libatrac9-arm64.a; fi
if [[ ! -f $atrac9_lib || -n $(find third_party/LibAtrac9/C/src -newer "$atrac9_lib" -name '*.c') ]]; then
    rm -rf out/atrac9 && mkdir -p out/atrac9
    for source in "${atrac9[@]}"; do "$CC" "${arch[@]}" -std=c99 -O2 -g -w -c "$source" -o "out/atrac9/$(basename "${source%.c}").o"; done
    rm -f "$atrac9_lib" && ar rcs "$atrac9_lib" out/atrac9/*.o
fi
# bbcpu (src/cpu, docs/ARM64_NATIVE.md): runs the game's x86-64 code through the translator
# (macOS; BB_CPU=interp on x86-64). Zydis decodes the guest's instructions.
cpu=()
if (( macos )); then
    rm -rf out/cpu && mkdir -p out/cpu
    for source in src/cpu/*.c src/cpu/hostcall.S; do
        "$CC" "${arch[@]}" -std=gnu11 -O2 -g -Wall -Wextra -Werror -Wno-unused-parameter -I"$deps/include" \
            -c "$source" -o "out/cpu/$(basename "$source").o"
    done
    rm -f out/libbbcpu.a && ar rcs out/libbbcpu.a out/cpu/*.o
    cpu=(out/libbbcpu.a "$deps/lib/libZydis.a")
fi
"$CC" "${arch[@]}" -std=c11 -O2 -g -Wall -Wextra -Werror "${warnings[@]}" -pthread "${probe_flags[@]}" "${includes[@]}" -I. -Isrc src/probe.c "${runtime[@]}" src/vulkan_smoke.c "$atrac9_lib" "${cpu[@]}" -lm "${gpu[@]}" "${libraries[@]}" -o out/bb-probe
echo "Built $PWD/out/bb-probe"
# GPU check for run.sh (live_resolution=auto) and the launcher's gamepad list (--gamepads).
"$CC" "${arch[@]}" -std=c11 -O2 -Wall -Wextra -Werror "${includes[@]}" tools/gpu_capabilities.c "${libraries[@]}" -o out/bb-gpu-capabilities
if [[ ${1:-} == --test ]]; then
    "$CC" "${arch[@]}" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread "${includes[@]}" -I. -Isrc tests/test_pad.c src/platform.c "${libraries[@]}" -o out/pad-test
    out/pad-test
    "$CC" "${arch[@]}" -std=c11 -O2 -g -Wall -Wextra -Werror "${warnings[@]}" -pthread "${includes[@]}" -I. -Isrc tests/test_runtime.c "${runtime[@]}" "$atrac9_lib" "${cpu[@]}" -lm "${gpu[@]}" "${libraries[@]}" -o out/runtime-test
    out/runtime-test
    "$CC" "${arch[@]}" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -Isrc tests/test_file_mods.c src/platform.c -o out/file-mods-test
    out/file-mods-test
    "$CC" "${arch[@]}" -std=c11 -O2 -g -Wall -Wextra -Werror "${warnings[@]}" -pthread "${includes[@]}" -I. -Isrc tests/test_sema.c "${runtime[@]}" "$atrac9_lib" "${cpu[@]}" -lm "${gpu[@]}" "${libraries[@]}" -o out/sema-test
    out/sema-test
    "$CC" "${arch[@]}" -std=c11 -D_GNU_SOURCE -O2 -g -Wall -Wextra -Werror -I. -Isrc tests/test_content.c src/runtime_content.c src/platform.c -o out/content-test
    out/content-test
    "$CC" "${arch[@]}" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -Isrc tests/test_guest_tls.c src/platform.c -o out/guest-tls-test
    out/guest-tls-test
    "${PYTHON:-python3}" tests/test_loader.py out/bb-probe
fi
