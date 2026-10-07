#!/usr/bin/env bash
# Starts the Bloodborne launcher (GTK4). With Python, PyGObject, GTK 4 and libadwaita installed
# it runs directly; otherwise through nix-shell (launcher/shell.nix).
# macOS: brew install gtk4 libadwaita pygobject3 (Homebrew's Python has the bindings; the
# system python3 does not).
here=$(cd -- "$(dirname -- "$0")" && pwd)
for python in python3 /opt/homebrew/bin/python3 /usr/local/bin/python3; do
    if "$python" -c 'import gi; gi.require_version("Gtk", "4.0"); gi.require_version("Adw", "1")' 2>/dev/null; then
        exec "$python" "$here/bbport_launcher.py" "$@"
    fi
done
if ! command -v nix-shell >/dev/null; then
    echo 'The launcher needs Python with PyGObject, GTK 4 and libadwaita (macOS: brew install gtk4 libadwaita pygobject3).' >&2
    exit 1
fi
exec nix-shell "$here/shell.nix" --run "python3 '$here/bbport_launcher.py'"
