#!/usr/bin/env python3
"""Restore one installation backup, including the prior input-method settings."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

if len(sys.argv) != 2:
    sys.exit("Usage: python scripts/restore.py ~/.local/state/kmoji/backup-YYYYMMDD-HHMMSS")
backup = Path(sys.argv[1]).resolve()
manifest = json.loads((backup / "manifest.json").read_text())
# Atomic replacement leaves a loaded library's old inode intact until restart.
# Keep the bus owner alive while restoring, so clients cannot activate another
# daemon halfway through the operation.
for original, name in manifest.items():
    path = Path(original)
    if name is None:
        path.unlink(missing_ok=True)
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = path.with_name(path.name + ".restore")
        shutil.copy2(backup / name, temporary)
        os.replace(temporary, path)
if (backup / "plasma-shortcut.json").exists():
    from gi.repository import Gio, GLib
    bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
    old = json.loads((backup / "plasma-shortcut.json").read_text())
    bus.call_sync("org.kde.kglobalaccel", "/kglobalaccel", "org.kde.KGlobalAccel", "setShortcut",
                  GLib.Variant("(asaiu)", (old["action"], old["keys"], 4)), None, Gio.DBusCallFlags.NONE, 3000, None)
subprocess.run(["qdbus6", "org.kde.KWin", "/KWin", "org.kde.KWin.reconfigure"], check=True)
subprocess.run(["qdbus6", "org.fcitx.Fcitx5", "/controller", "org.fcitx.Fcitx.Controller1.ReloadConfig"], check=True)
subprocess.run(["qdbus6", "org.fcitx.Fcitx5", "/controller", "org.fcitx.Fcitx.Controller1.Restart"], check=True)
print("Restored", backup)
