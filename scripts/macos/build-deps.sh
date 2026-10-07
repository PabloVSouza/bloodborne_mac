#!/usr/bin/env bash
# macOS ships bash 3.2; this script needs 4.4 or newer (brew install bash).
if (( BASH_VERSINFO[0] < 4 || (BASH_VERSINFO[0] == 4 && BASH_VERSINFO[1] < 4) )); then
    for candidate in /opt/homebrew/bin/bash /usr/local/bin/bash; do
        if [[ -x $candidate ]]; then exec "$candidate" "$0" "$@"; fi
    done
    echo 'bash 4.4 or newer is needed (macOS: brew install bash).' >&2; exit 1
fi
# macOS (Apple Silicon or Intel): builds the x86-64 libraries bbport links against into
# deps/macos-x86_64. The game's code is x86-64, so the whole process is an x86-64 program run by
# Rosetta 2 on Apple Silicon; Homebrew's arm64 libraries cannot be linked into it.
# Build tools that only run on the host (pkg-config, glslang, nasm) come from Homebrew.
#   bash scripts/macos/build-deps.sh          # everything not built yet
#   BB_DEPS_REBUILD=sdl3 bash scripts/...     # rebuild one package
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
root=$PWD
prefix=$root/deps/macos-x86_64
work=$root/deps/build
mkdir -p "$prefix" "$work/src"

for tool in pkg-config glslangValidator nasm cmake ninja; do
    if ! command -v $tool >/dev/null; then
        echo "Missing $tool: brew install pkgconf glslang nasm cmake ninja bash" >&2; exit 1
    fi
done

VULKAN_VERSION=v1.4.365
MOLTENVK_VERSION=v1.4.2
SDL_VERSION=release-3.4.18
FMT_VERSION=12.0.0
MAGIC_ENUM_VERSION=v0.9.7
ROBIN_MAP_VERSION=v1.4.0
VMA_VERSION=v3.3.0
MINIZ_VERSION=3.1.2
XBYAK_VERSION=v7.43
ZYDIS_VERSION=v4.1.1
XXHASH_VERSION=v0.8.3
BOOST_VERSION=1.89.0
FFMPEG_VERSION=n7.1.2

jobs=$(sysctl -n hw.ncpu)
export MACOSX_DEPLOYMENT_TARGET=13.0
cmake_x86=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
    -DCMAKE_OSX_DEPLOYMENT_TARGET=$MACOSX_DEPLOYMENT_TARGET -DCMAKE_INSTALL_PREFIX="$prefix"
    -DCMAKE_PREFIX_PATH="$prefix" -DCMAKE_INSTALL_NAME_DIR="$prefix/lib" -DBUILD_TESTING=OFF
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5)

done_mark() { echo "$prefix/.built-$1"; }
needed() {
    [[ ${BB_DEPS_REBUILD:-} == "$1" || ${BB_DEPS_REBUILD:-} == all ]] && return 0
    [[ ! -f $(done_mark "$1") ]]
}
mark() { touch "$(done_mark "$1")"; echo "== $1 installed"; }
# fetch <name> <url>: downloads and unpacks a source archive into $work/src/<name>;
# <repository>.git#<tag> clones the tag with its submodules.
fetch() {
    local name=$1 url=$2 archive=$work/$1.archive
    rm -rf "${work:?}/src/$name"; mkdir -p "$work/src/$name"
    if [[ $url == *.git#* ]]; then
        git clone -q --depth 1 --recurse-submodules --shallow-submodules -b "${url#*#}" "${url%#*}" "$work/src/$name"
        return
    fi
    [[ -f $archive ]] || curl -fsSL --retry 3 -o "$archive" "$url"
    tar -xf "$archive" -C "$work/src/$name" --strip-components 1
}
# cmake_package <name> <url> [cmake options...]
cmake_package() {
    local name=$1 url=$2; shift 2
    needed "$name" || return 0
    echo "== $name"
    fetch "$name" "$url"
    cmake -S "$work/src/$name" -B "$work/$name" "${cmake_x86[@]}" "$@" >/dev/null
    cmake --build "$work/$name" -j "$jobs" >/dev/null
    cmake --install "$work/$name" >/dev/null
    mark "$name"
}
gh() { echo "https://github.com/$1/archive/refs/tags/$2.tar.gz"; }

cmake_package vulkan-headers "$(gh KhronosGroup/Vulkan-Headers $VULKAN_VERSION)"
cmake_package vulkan-loader "$(gh KhronosGroup/Vulkan-Loader $VULKAN_VERSION)" \
    -DUPDATE_DEPS=OFF -DBUILD_WSI_XCB_SUPPORT=OFF -DBUILD_WSI_XLIB_SUPPORT=OFF -DBUILD_WSI_WAYLAND_SUPPORT=OFF

# MoltenVK: the Khronos release is a universal dylib (building it needs Xcode). The ICD manifest
# points the loader at it; run.sh sets VK_DRIVER_FILES to it.
if needed moltenvk; then
    echo "== moltenvk"
    archive=$work/moltenvk.tar
    [[ -f $archive ]] || curl -fsSL --retry 3 -o "$archive" \
        "https://github.com/KhronosGroup/MoltenVK/releases/download/$MOLTENVK_VERSION/MoltenVK-macos.tar"
    rm -rf "$work/src/moltenvk"; mkdir -p "$work/src/moltenvk"
    tar -xf "$archive" -C "$work/src/moltenvk"
    dylib=$(find "$work/src/moltenvk" -path '*dynamic*' -name libMoltenVK.dylib | head -1)
    [[ -n $dylib ]] || { echo 'libMoltenVK.dylib not found in the MoltenVK release' >&2; exit 1; }
    lipo -info "$dylib" | grep -q x86_64 || { echo 'MoltenVK release has no x86_64 slice' >&2; exit 1; }
    mkdir -p "$prefix/lib" "$prefix/share/vulkan/icd.d"
    cp "$dylib" "$prefix/lib/libMoltenVK.dylib"
    cat > "$prefix/share/vulkan/icd.d/MoltenVK_icd.json" <<'EOF'
{
    "file_format_version": "1.0.0",
    "ICD": {
        "library_path": "../../../lib/libMoltenVK.dylib",
        "api_version": "1.4.0",
        "is_portability_driver": true
    }
}
EOF
    mark moltenvk
fi

cmake_package sdl3 "$(gh libsdl-org/SDL $SDL_VERSION)" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
cmake_package fmt "$(gh fmtlib/fmt $FMT_VERSION)" -DFMT_TEST=OFF -DFMT_DOC=OFF
cmake_package magic-enum "$(gh Neargye/magic_enum $MAGIC_ENUM_VERSION)" \
    -DMAGIC_ENUM_OPT_BUILD_EXAMPLES=OFF -DMAGIC_ENUM_OPT_BUILD_TESTS=OFF
cmake_package xbyak "$(gh herumi/xbyak $XBYAK_VERSION)"
cmake_package robin-map "$(gh Tessil/robin-map $ROBIN_MAP_VERSION)"
cmake_package vma "$(gh GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator $VMA_VERSION)"
cmake_package miniz "$(gh richgel999/miniz $MINIZ_VERSION)" -DBUILD_EXAMPLES=OFF -DBUILD_FUZZERS=OFF -DBUILD_TESTS=OFF
# Zydis builds its bundled Zycore without install rules; Zycore is installed first and used from
# there, so Zydis's package files find it.
if needed zydis; then
    echo "== zydis"
    fetch zydis "https://github.com/zyantific/zydis.git#$ZYDIS_VERSION"
    cmake -S "$work/src/zydis/dependencies/zycore" -B "$work/zycore" "${cmake_x86[@]}" >/dev/null
    cmake --build "$work/zycore" -j "$jobs" >/dev/null
    cmake --install "$work/zycore" >/dev/null
    cmake -S "$work/src/zydis" -B "$work/zydis" "${cmake_x86[@]}" -DZYAN_SYSTEM_ZYCORE=ON \
        -DZYDIS_BUILD_TOOLS=OFF -DZYDIS_BUILD_EXAMPLES=OFF -DZYDIS_BUILD_DOXYGEN=OFF >/dev/null
    cmake --build "$work/zydis" -j "$jobs" >/dev/null
    cmake --install "$work/zydis" >/dev/null
    mark zydis
fi
# xxHash's CMake project lives in cmake_unofficial.
if needed xxhash; then
    echo "== xxhash"
    fetch xxhash "$(gh Cyan4973/xxHash $XXHASH_VERSION)"
    cmake -S "$work/src/xxhash/cmake_unofficial" -B "$work/xxhash" "${cmake_x86[@]}" -DXXHASH_BUILD_XXHSUM=OFF -DBUILD_SHARED_LIBS=OFF >/dev/null
    cmake --build "$work/xxhash" -j "$jobs" >/dev/null
    cmake --install "$work/xxhash" >/dev/null
    mark xxhash
fi
# Boost: header-only libraries (CMake package files included).
if needed boost; then
    echo "== boost"
    fetch boost "https://github.com/boostorg/boost/releases/download/boost-$BOOST_VERSION/boost-$BOOST_VERSION-cmake.tar.xz"
    cmake -S "$work/src/boost" -B "$work/boost" "${cmake_x86[@]}" \
        -DBOOST_INCLUDE_LIBRARIES='asio;container;container_hash;icl;intrusive;pool;preprocessor' \
        -DBOOST_SKIP_INSTALL_RULES=OFF -DBOOST_CONTEXT_ARCHITECTURE=x86_64 -DBOOST_CONTEXT_ABI=sysv >/dev/null
    cmake --build "$work/boost" -j "$jobs" >/dev/null
    cmake --install "$work/boost" >/dev/null
    mark boost
fi
# FFmpeg: the decoders the game's movies need (AvPlayer), built as x86-64 shared libraries.
if needed ffmpeg; then
    echo "== ffmpeg"
    fetch ffmpeg "$(gh FFmpeg/FFmpeg $FFMPEG_VERSION)"
    (cd "$work/src/ffmpeg" && ./configure --prefix="$prefix" --enable-cross-compile --arch=x86_64 --target-os=darwin \
        --cc="clang -arch x86_64" --cxx="clang++ -arch x86_64" --x86asmexe=nasm \
        --extra-cflags="-mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET" \
        --extra-ldflags="-arch x86_64 -mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET" \
        --enable-shared --disable-static --disable-programs --disable-doc --disable-avdevice \
        --disable-avfilter --disable-network --disable-autodetect --enable-videotoolbox \
        --install-name-dir="$prefix/lib" >/dev/null &&
        make -j "$jobs" >/dev/null && make install >/dev/null)
    mark ffmpeg
fi
echo "Dependencies are in $prefix"
