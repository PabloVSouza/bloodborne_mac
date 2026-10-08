#!/usr/bin/env bash
# macOS 26+ on Apple Silicon: builds Mesa's KosmicKrisp Vulkan driver (Vulkan 1.4 on Metal 4,
# conformant) into deps/macos-arm64-kk, as an alternative to MoltenVK for the native build
# (run.sh: BB_VK_DRIVER=kosmickrisp). Its OpenCL C kernels are compiled at build time (mesa_clc):
# that needs Homebrew's llvm, spirv-tools and spirv-headers, plus a SPIRV-LLVM-Translator for that
# LLVM, built here. meson, mako and PyYAML go into a virtualenv under deps/build-arm64.
#   bash scripts/macos/build-kosmickrisp.sh
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
root=$PWD
prefix=$root/deps/macos-arm64-kk
work=$root/deps/build-arm64
MESA_VERSION=26.2.4
for formula in llvm spirv-tools spirv-headers; do
    brew --prefix --installed "$formula" >/dev/null 2>&1 || { echo "Missing: brew install $formula" >&2; exit 1; }
done
llvm=$(brew --prefix llvm)
llvm_version=$("$llvm/bin/llvm-config" --version)
translator_version=v$llvm_version
mkdir -p "$work/src" "$prefix"

fetch() { # fetch <name> <url>
    local name=$1 url=$2 archive=$work/$1.archive
    [[ -d $work/src/$name ]] && return
    mkdir -p "$work/src/$name"
    [[ -f $archive ]] || curl -fsSL --retry 3 -o "$archive" "$url"
    tar -xf "$archive" -C "$work/src/$name" --strip-components 1
}

# meson and the Python modules Mesa's generators use.
if [[ ! -x $work/venv/bin/meson ]]; then
    python3 -m venv "$work/venv"
    "$work/venv/bin/pip" install -q meson mako pyyaml packaging
fi
export PATH=$work/venv/bin:$PATH

# SPIRV-LLVM-Translator matching Homebrew's LLVM.
translator=$work/translator-$llvm_version
if [[ ! -f $translator/lib/pkgconfig/LLVMSPIRVLib.pc ]]; then
    fetch "spirv-llvm-translator-$llvm_version" \
        "https://github.com/KhronosGroup/SPIRV-LLVM-Translator/archive/refs/tags/$translator_version.tar.gz"
    cmake -S "$work/src/spirv-llvm-translator-$llvm_version" -B "$work/build/spirv-llvm-translator-$llvm_version" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$translator" -DLLVM_DIR="$llvm/lib/cmake/llvm" \
        -DLLVM_EXTERNAL_SPIRV_HEADERS_SOURCE_DIR="$(brew --prefix spirv-headers)" \
        -DLLVM_SPIRV_INCLUDE_TESTS=OFF -DBUILD_SHARED_LIBS=OFF
    cmake --build "$work/build/spirv-llvm-translator-$llvm_version"
    cmake --install "$work/build/spirv-llvm-translator-$llvm_version"
fi

# Mesa with only the KosmicKrisp Vulkan driver.
fetch "mesa-$MESA_VERSION" "https://archive.mesa3d.org/mesa-$MESA_VERSION.tar.xz"
cat > "$work/mesa-native.ini" <<EOF
[binaries]
llvm-config = '$llvm/bin/llvm-config'
EOF
export PKG_CONFIG_PATH=$translator/lib/pkgconfig:$(brew --prefix spirv-tools)/lib/pkgconfig:${PKG_CONFIG_PATH:-}
build=$work/build/mesa-$MESA_VERSION
if [[ ! -f $build/build.ninja ]]; then
    meson setup "$build" "$work/src/mesa-$MESA_VERSION" --native-file "$work/mesa-native.ini" \
        --prefix="$prefix" --libdir=lib -Dbuildtype=release -Db_ndebug=true \
        -Dvulkan-drivers=kosmickrisp -Dgallium-drivers= -Dplatforms=macos -Dopengl=false \
        -Dgles1=disabled -Dgles2=disabled -Dglx=disabled -Degl=disabled -Dgbm=disabled \
        -Dllvm=enabled -Dshared-llvm=enabled -Dvideo-codecs= -Dtools= -Dbuild-tests=false \
        -Dxmlconfig=disabled -Dzstd=disabled -Dvalgrind=disabled -Dlibunwind=disabled
fi
ninja -C "$build" install
ls "$prefix"/share/vulkan/icd.d/
echo "KosmicKrisp: $prefix (run.sh: BB_VK_DRIVER=kosmickrisp)"
