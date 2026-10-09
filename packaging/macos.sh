#!/usr/bin/env bash
# macOS: packages the current build (bash build.sh first) as Bloodborne.app in
# dist/bloodborne_mac-<version>-<arch>.dmg. No game files are included. The app needs nothing
# installed: it carries the launcher (launcher/macos/Launcher.swift), bash, Python and the
# libraries.
#   Contents/MacOS/             Bloodborne (launcher), bb-probe, bb-gpu-capabilities, bash
#   Contents/Frameworks/        libbbgpu, MoltenVK, Vulkan loader, SDL3, FFmpeg
#   Contents/Resources/game/    run.sh, scripts/, patches/, share/vulkan/icd.d/
#   Contents/Resources/python/  standalone Python 3
# Library references become @rpath ones and everything is signed again ad hoc (no Developer
# ID: macOS asks the user to allow the app once). Version: $BB_VERSION, else `git describe`.
#   bash packaging/macos.sh
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
[[ $(uname -s) == Darwin ]] || { echo 'macOS only' >&2; exit 1; }
arch=${BB_ARCH:-$( [[ $(sysctl -n hw.optional.arm64 2>/dev/null) == 1 ]] && echo arm64 || echo x86_64 )}
[[ $arch == arm64 ]] || { echo 'The app is built for Apple Silicon (arm64) only' >&2; exit 1; }
deps=$PWD/deps/macos-$arch
gpudir=out/gpu-arm64
# The app's own bash and Python (scripts/macos/build-deps.sh builds what is missing).
[[ -x $deps/app-runtime/bin/bash && -x $deps/app-runtime/python/bin/python3 ]] ||
    BB_ARCH=$arch bash scripts/macos/build-deps.sh
for f in out/bb-probe out/bb-gpu-capabilities "$gpudir/libbbgpu.dylib" "$deps/lib/libMoltenVK.dylib"; do
    [[ -f $f ]] || { echo "Missing $f: build first (BB_ARCH=$arch bash build.sh)" >&2; exit 1; }
done
version=${BB_VERSION:-$(git describe --tags --always --dirty 2>/dev/null || echo dev)}
short_version=$(sed -E 's/^v//; s/[^0-9.].*$//' <<< "$version"); [[ -n $short_version ]] || short_version=0.0.0
name=bloodborne_mac-$version-$arch
work=dist/$name
app=$work/Bloodborne.app
rm -rf "$work" "dist/$name.dmg"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Frameworks" "$app/Contents/Resources/game/share/vulkan/icd.d"
macos=$app/Contents/MacOS
frameworks=$app/Contents/Frameworks
game=$app/Contents/Resources/game

# The launcher: Xcode's toolchain (the Command Line Tools have no SwiftUI macro plugins).
developer=${DEVELOPER_DIR:-$(ls -d /Applications/Xcode*.app 2>/dev/null | sort -V | tail -1)/Contents/Developer}
DEVELOPER_DIR=$developer xcrun swiftc -parse-as-library -O -target arm64-apple-macos13.0 \
    launcher/macos/Launcher.swift -o "$macos/Bloodborne"
install -m755 out/bb-probe out/bb-gpu-capabilities "$deps/app-runtime/bin/bash" "$macos/"
install -m755 "$gpudir/libbbgpu.dylib" "$frameworks/"
copy_deps() { # copy_deps FILE: the non-system libraries FILE links against, recursively
    local dep
    for dep in $(otool -L "$1" | awk 'NR > 1 {print $1}' | grep "^$deps/lib/"); do
        local base=${dep##*/}
        if [[ ! -f $frameworks/$base ]]; then
            install -m755 "$(realpath "$dep")" "$frameworks/$base"
            copy_deps "$frameworks/$base"
        fi
    done
}
for f in "$macos/bb-probe" "$macos/bb-gpu-capabilities" "$frameworks/libbbgpu.dylib"; do copy_deps "$f"; done
install -m755 "$(realpath "$deps/lib/libMoltenVK.dylib")" "$frameworks/libMoltenVK.dylib"
cat > "$game/share/vulkan/icd.d/MoltenVK_icd.json" <<'EOF'
{
    "file_format_version": "1.0.0",
    "ICD": {
        "library_path": "../../../../../Frameworks/libMoltenVK.dylib",
        "api_version": "1.4.0",
        "is_portability_driver": true
    }
}
EOF

relink() { # relink FILE RPATH: @rpath references, the build tree's search paths replaced
    local file=$1 dep old
    for dep in $(otool -L "$file" | awk 'NR > 1 {print $1}' | grep "^$deps/lib/"); do
        install_name_tool -change "$dep" "@rpath/${dep##*/}" "$file" 2>/dev/null
    done
    if [[ $file == *.dylib ]]; then install_name_tool -id "@rpath/${file##*/}" "$file" 2>/dev/null; fi
    for old in $(otool -l "$file" | awk '/LC_RPATH/ {getline; getline; print $2}'); do
        install_name_tool -delete_rpath "$old" "$file" 2>/dev/null
    done
    install_name_tool -add_rpath "$2" "$file" 2>/dev/null
}
relink "$macos/bb-probe" @executable_path/../Frameworks
relink "$macos/bb-gpu-capabilities" @executable_path/../Frameworks
for f in "$frameworks/"*.dylib; do relink "$f" @loader_path; done
if leftovers=$(for f in "$macos/"* "$frameworks/"*.dylib; do otool -L "$f" | awk 'NR > 1 {print $1}'; done |
               grep "^$PWD" | sort -u) && [[ -n $leftovers ]]; then
    echo "References into the build tree remain:" >&2; echo "$leftovers" >&2; exit 1
fi
strip -x "$macos/bb-probe" "$macos/bb-gpu-capabilities" "$frameworks/"*.dylib 2>/dev/null || true

cp run.sh LICENSE README.md "$game/"
cp -R scripts patches "$game/"
cp -R "$deps/app-runtime/python" "$app/Contents/Resources/python"
find "$app" -name __pycache__ -type d -prune -exec rm -rf {} +
iconset=$(mktemp -d)/AppIcon.iconset
DEVELOPER_DIR=$developer xcrun swift launcher/macos/make_icon.swift "$iconset" >/dev/null
iconutil -c icns "$iconset" -o "$app/Contents/Resources/AppIcon.icns"
cat > "$app/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>Bloodborne</string>
    <key>CFBundleDisplayName</key><string>Bloodborne</string>
    <key>CFBundleIdentifier</key><string>io.github.pablovsouza.bloodborne-mac</string>
    <key>CFBundleExecutable</key><string>Bloodborne</string>
    <key>CFBundleIconFile</key><string>AppIcon</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>$short_version</string>
    <key>CFBundleVersion</key><string>$version</string>
    <key>LSMinimumSystemVersion</key><string>13.0</string>
    <key>LSApplicationCategoryType</key><string>public.app-category.role-playing-games</string>
    <key>NSHighResolutionCapable</key><true/>
    <key>NSHumanReadableCopyright</key><string>GPL-2.0-or-later. Based on bbport (deadinside28/bloodborne_pc) and shadPS4. No game files included.</string>
</dict>
</plist>
EOF

# Ad-hoc signatures, inside out (libraries, helpers, Python's binaries, then the bundle).
codesign --force --sign - "$frameworks/"*.dylib
find "$app/Contents/Resources/python" -type f \( -perm -u+x -o -name '*.so' -o -name '*.dylib' \) \
    -exec sh -c 'file -b "$1" | grep -q Mach-O && codesign --force --sign - "$1"' _ {} \;
codesign --force --sign - "$macos/bb-probe" "$macos/bb-gpu-capabilities" "$macos/bash"
codesign --force --sign - "$app"
codesign --verify --deep --strict "$app"

ln -s /Applications "$work/Applications"
cat > "$work/First start.txt" <<EOF
Bloodborne for Apple Silicon ($version)

1. Drag Bloodborne to Applications.
2. The first time, right-click Bloodborne and choose Open, then Open again (the app is not
   signed with an Apple Developer ID). If macOS still refuses: System Settings → Privacy &
   Security → Open Anyway. Or, in Terminal: xattr -dr com.apple.quarantine /Applications/Bloodborne.app
3. Choose your Bloodborne dump (the CUSA03173 folder, version 1.09) and press Play.
   The first start compiles the game's shaders and takes a few minutes.

No game files are included. Saves, settings and logs:
~/Library/Application Support/bloodborne_mac

https://github.com/PabloVSouza/bloodborne_mac
EOF
hdiutil create -quiet -volname "Bloodborne" -srcfolder "$work" -ov -format UDZO "dist/$name.dmg"
ls -lh "dist/$name.dmg"
