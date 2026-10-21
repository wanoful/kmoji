#!/usr/bin/env python3
"""Build and atomically install for this user; --activate switches the picker."""
import argparse
import configparser
from datetime import datetime
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
HOME = Path.home()
PREFIX = HOME / ".local"
CONFIG = Path(os.environ.get("XDG_CONFIG_HOME", HOME / ".config"))
STATE = Path(os.environ.get("XDG_STATE_HOME", HOME / ".local/state")) / "kmoji"


def run(*args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def backup(paths):
    target = STATE / ("backup-" + datetime.now().strftime("%Y%m%d-%H%M%S"))
    target.mkdir(parents=True)
    manifest = {}
    for i, path in enumerate(paths):
        manifest[str(path)] = str(i) if path.exists() else None
        if path.exists():
            shutil.copy2(path, target / str(i))
    (target / "manifest.json").write_text(json.dumps(manifest, indent=2))
    print("Backup:", target, flush=True)
    return target


def rule():
    path = CONFIG / "kwinrulesrc"
    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    config.read(path)
    if not config.has_section("General"):
        config["General"] = {}
    rules = list(filter(None, config["General"].get("rules", "").split(",")))
    name = "kmoji-native-picker"
    if name not in rules:
        rules.append(name)
    config["General"]["rules"] = ",".join(rules)
    config["General"]["count"] = str(len(rules))
    config[name] = {"Description": "kMoji native emoji picker", "title": "kMoji Emoji Picker", "titlematch": "2",
                    "acceptfocus": "false", "acceptfocusrule": "2", "layer": "popup", "layerrule": "2",
                    "skiptaskbar": "true", "skiptaskbarrule": "2", "skippager": "true", "skippagerrule": "2",
                    "skipswitcher": "true", "skipswitcherrule": "2", "placement": "5", "placementrule": "2"}
    with path.open("w") as output:
        config.write(output, space_around_delimiters=False)
    run("qdbus6", "org.kde.KWin", "/KWin", "org.kde.KWin.reconfigure")


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--activate", action="store_true", help="Configure KWin, disable the previous picker, and restart Fcitx5")
args = parser.parse_args()
if not (ROOT / "data/emoji.json").exists():
    run("python", str(ROOT / "scripts/generate-data.py"))
run("cmake", "-S", str(ROOT), "-B", str(ROOT / "build"), "-G", "Ninja", "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
    f"-DCMAKE_INSTALL_PREFIX={PREFIX}", "-DCMAKE_INSTALL_LIBDIR=lib")
run("cmake", "--build", str(ROOT / "build"), "-j", "4")
files = [PREFIX / "bin/kmoji", PREFIX / "lib/fcitx5/libkmoji.so", PREFIX / "share/fcitx5/addon/kmoji.conf",
         PREFIX / "share/kmoji/emoji.json", PREFIX / "share/kmoji/UNICODE-LICENSE.txt"]
paths = files + [CONFIG / "kwinrulesrc", CONFIG / "fcitx5/config", CONFIG / "fcitx5/conf/kmoji.conf"]
if args.activate:
    run("qdbus6", "org.fcitx.Fcitx5", "/controller", "org.fcitx.Fcitx.Controller1.Save")
backup_dir = backup(paths)
with tempfile.TemporaryDirectory(prefix="kmoji-install-") as stage:
    run("cmake", "--install", str(ROOT / "build"), env={**os.environ, "DESTDIR": stage})
    for path in files:
        source = Path(stage) / str(path).lstrip("/")
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = path.with_name(path.name + ".new")
        shutil.copy2(source, temporary)
        os.replace(temporary, path)

if args.activate:
    rule()
    run("busctl", "--user", "call", "org.fcitx.Fcitx5", "/controller", "org.fcitx.Fcitx.Controller1", "SetAddonsState", "a(sb)",
        "2", "fcitx5imemojipicker", "false", "kmoji", "true")
    # Release Meta+. from Plasma's default picker while retaining other shortcuts.
    from gi.repository import Gio, GLib
    bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
    action = ["org.kde.plasma.emojier.desktop", "_launch", "Emoji Selector", "Emoji Selector"]
    current = bus.call_sync("org.kde.kglobalaccel", "/kglobalaccel", "org.kde.KGlobalAccel", "shortcut",
                            GLib.Variant("(as)", (action,)), None, Gio.DBusCallFlags.NONE, 3000, None).unpack()[0]
    (backup_dir / "plasma-shortcut.json").write_text(json.dumps({"action": action, "keys": current}))
    bus.call_sync("org.kde.kglobalaccel", "/kglobalaccel", "org.kde.KGlobalAccel", "setShortcut",
                  GLib.Variant("(asaiu)", (action, [k for k in current if k != 268435502], 4)), None, Gio.DBusCallFlags.NONE, 3000, None)
    def owner_pid():
        return bus.call_sync("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetConnectionUnixProcessID",
                             GLib.Variant("(s)", ("org.fcitx.Fcitx5",)), None, Gio.DBusCallFlags.NONE, 1000, None).unpack()[0]
    previous_pid = owner_pid()
    run("qdbus6", "org.fcitx.Fcitx5", "/controller", "org.fcitx.Fcitx.Controller1.Restart")
    for _ in range(50):
        try:
            if owner_pid() != previous_pid:
                bus.call_sync("org.fcitx.Fcitx5", "/controller", "org.fcitx.Fcitx.Controller1", "GetConfig",
                              GLib.Variant("(s)", ("fcitx://config/addon/kmoji",)), None, Gio.DBusCallFlags.NONE, 1000, None)
                print("Activated kMoji. Open a text field and press Win+.")
                break
        except GLib.Error:
            pass
        time.sleep(0.1)
    else:
        raise RuntimeError("Fcitx5 did not report the new addon. Restore using: python scripts/restore.py " + str(backup_dir))
else:
    print("Installed. Preview:", PREFIX / "bin/kmoji", "--preview")
    print("To activate: python scripts/install.py --activate")
