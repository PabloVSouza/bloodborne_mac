#!/usr/bin/env bash
# macOS: packages the current build (bash build.sh first) as a self-contained folder and archive,
# dist/bloodborne_mac-<version>-<arch>.tar.gz. No game files are included.
#   bin/       bb-probe, bb-gpu-capabilities
#   lib/       libbbgpu and the libraries it needs (Vulkan loader, MoltenVK, SDL3, FFmpeg)
#   share/vulkan/icd.d/MoltenVK_icd.json
#   run.sh, play.command, scripts/, patches/, launcher/, LICENSE, README.md
# Library references are rewritten to @rpath (the binaries look in ../lib) and everything is
# signed again ad hoc. Version: $BB_VERSION, else `git describe`.
#   bash packaging/macos.sh
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
[[ $(uname -s) == Darwin ]] || { echo 'macOS only' >&2; exit 1; }
arch=${BB_ARCH:-$( [[ $(sysctl -n hw.optional.arm64 2>/dev/null) == 1 ]] && echo arm64 || echo x86_64 )}
deps=$PWD/deps/macos-$arch
gpudir=out/gpu$([[ $arch == arm64 ]] && echo -arm64)
for f in out/bb-probe out/bb-gpu-capabilities "$gpudir/libbbgpu.dylib" "$deps/lib/libMoltenVK.dylib"; do
    [[ -f $f ]] || { echo "Missing $f: build first (BB_ARCH=$arch bash build.sh)" >&2; exit 1; }
done
version=${BB_VERSION:-$(git describe --tags --always --dirty 2>/dev/null || echo dev)}
name=bloodborne_mac-$version-$arch
stage=dist/$name
rm -rf "$stage" "dist/$name.tar.gz"
mkdir -p "$stage/bin" "$stage/lib" "$stage/share/vulkan/icd.d"

install -m755 out/bb-probe out/bb-gpu-capabilities "$stage/bin/"
install -m755 "$gpudir/libbbgpu.dylib" "$stage/lib/"
# The libraries the binaries name by absolute path, with their own dependencies, and MoltenVK
# (loaded by the Vulkan loader through the ICD manifest).
copy_deps() { # copy_deps FILE: the non-system libraries FILE links against, recursively
    local dep
    for dep in $(otool -L "$1" | awk 'NR > 1 {print $1}' | grep "^$deps/lib/"); do
        local base=${dep##*/}
        if [[ ! -f $stage/lib/$base ]]; then
            install -m755 "$(realpath "$dep")" "$stage/lib/$base"
            copy_deps "$stage/lib/$base"
        fi
    done
}
for f in "$stage/bin/"* "$stage/lib/libbbgpu.dylib"; do copy_deps "$f"; done
install -m755 "$(realpath "$deps/lib/libMoltenVK.dylib")" "$stage/lib/libMoltenVK.dylib"
copy_deps "$stage/lib/libMoltenVK.dylib"
cat > "$stage/share/vulkan/icd.d/MoltenVK_icd.json" <<'EOF'
{
    "file_format_version": "1.0.0",
    "ICD": {
        "library_path": "../../../lib/libMoltenVK.dylib",
        "api_version": "1.4.0",
        "is_portability_driver": true
    }
}
EOF

# @rpath references and a relative search path; the build tree's search paths removed.
relink() { # relink FILE RPATH
    local file=$1 dep
    for dep in $(otool -L "$file" | awk 'NR > 1 {print $1}' | grep "^$deps/lib/"); do
        install_name_tool -change "$dep" "@rpath/${dep##*/}" "$file" 2>/dev/null
    done
    if [[ $file == *.dylib ]]; then
        install_name_tool -id "@rpath/${file##*/}" "$file" 2>/dev/null
    fi
    local old
    for old in $(otool -l "$file" | awk '/LC_RPATH/ {getline; getline; print $2}'); do
        install_name_tool -delete_rpath "$old" "$file" 2>/dev/null
    done
    install_name_tool -add_rpath "$2" "$file" 2>/dev/null
}
for f in "$stage/bin/"*; do relink "$f" @executable_path/../lib; done
for f in "$stage/lib/"*.dylib; do relink "$f" @loader_path; done
# Anything still pointing into the build tree would fail on another Mac.
if leftovers=$(for f in "$stage/bin/"* "$stage/lib/"*.dylib; do otool -L "$f" | awk 'NR > 1 {print $1}'; done |
               grep "^$PWD" | sort -u) && [[ -n $leftovers ]]; then
    echo "References into the build tree remain:" >&2; echo "$leftovers" >&2; exit 1
fi
strip -x "$stage/bin/"* "$stage/lib/"*.dylib 2>/dev/null || true
codesign --force --sign - "$stage/lib/"*.dylib "$stage/bin/"*

cp run.sh LICENSE README.md "$stage/"
cp -R scripts patches launcher "$stage/"
find "$stage" -name __pycache__ -type d -prune -exec rm -rf {} +
# Double-click start (Finder opens .command files in Terminal): the game folder from
# BB_GAME_DIR, else a CUSA03173 folder next to this one, else asked once and remembered.
cat > "$stage/play.command" <<'EOF'
#!/bin/bash
cd -- "$(dirname -- "$0")"
if [[ -z ${BB_GAME_DIR:-} ]]; then
    if [[ -f game-dir.txt ]]; then BB_GAME_DIR=$(cat game-dir.txt)
    elif [[ -f ../CUSA03173/eboot.bin ]]; then BB_GAME_DIR=../CUSA03173
    else
        read -r -p 'Path of the game folder (CUSA03173, drag it here): ' BB_GAME_DIR
        BB_GAME_DIR=${BB_GAME_DIR%/}; BB_GAME_DIR=${BB_GAME_DIR//\\ / }
        echo "$BB_GAME_DIR" > game-dir.txt
    fi
fi
export BB_GAME_DIR BB_PREBUILT=1
exec ./run.sh "$@"
EOF
chmod 755 "$stage/play.command" "$stage/run.sh"
cat > "$stage/RELEASE.txt" <<EOF
bloodborne_mac $version ($arch)

Needs: macOS on Apple Silicon (15 or newer recommended), Homebrew's bash and Python 3
(brew install bash python), and your own dump of Bloodborne CUSA03173 v1.09.

Start: double-click play.command (or: BB_GAME_DIR=/path/to/CUSA03173 BB_PREBUILT=1 ./run.sh).
The first start compiles the game's shaders and takes longer. Saves, the shader cache and
bbport.ini are kept in this folder.

If macOS refuses to open it (downloaded, unsigned), run once in this folder:
    xattr -dr com.apple.quarantine .

https://github.com/PabloVSouza/bloodborne_mac
EOF
tar -C dist -czf "dist/$name.tar.gz" "$name"
ls -lh "dist/$name.tar.gz"
