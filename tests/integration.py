#!/usr/bin/env python3
"""Exercise the real addon on a private D-Bus, never the user's input method."""
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if "--session" not in sys.argv:
    sys.exit(subprocess.call(["dbus-run-session", "--", sys.executable, __file__, "--session"]))

from gi.repository import Gio, GLib


def pump(seconds=0.08):
    loop = GLib.MainLoop()
    GLib.timeout_add(max(1, int(seconds * 1000)), lambda: (loop.quit(), False)[1])
    loop.run()


with tempfile.TemporaryDirectory(prefix="kmoji-test-") as tmp:
    root = Path(tmp)
    for folder in ["config", "data/fcitx5/addon", "runtime", "cache"]:
        (root / folder).mkdir(parents=True, exist_ok=True)
    (root / "runtime").chmod(0o700)
    shutil.copy(ROOT / "data/kmoji.conf", root / "data/fcitx5/addon")
    env = {**os.environ, "XDG_CONFIG_HOME": str(root / "config"), "XDG_DATA_HOME": str(root / "data"),
           "XDG_CACHE_HOME": str(root / "cache"), "XDG_RUNTIME_DIR": str(root / "runtime"),
           "FCITX_ADDON_DIRS": f"{ROOT}/build:/usr/lib/fcitx5", "QT_QPA_PLATFORM": "offscreen",
           "QT_QPA_PLATFORMTHEME": "", "DISPLAY": "", "WAYLAND_DISPLAY": "", "KMOJI_UI": "/bin/true"}
    os.environ.update({key: env[key] for key in ["XDG_RUNTIME_DIR"]})
    log = (root / "fcitx.log").open("w+")
    daemon = subprocess.Popen(["fcitx5", "--disable", "all", "--enable", "keyboard,dbus,dbusfrontend,kmoji"], env=env, stdout=log, stderr=log)
    ui = None
    bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
    service = "org.fcitx.Fcitx5"
    iface = "org.fcitx.Fcitx.InputContext1"

    def call(path, interface, method, args=None):
        return bus.call_sync(service, path, interface, method, args, None, Gio.DBusCallFlags.NO_AUTO_START, 3000, None)

    def create():
        result = call("/org/freedesktop/portal/inputmethod", "org.fcitx.Fcitx.InputMethod1", "CreateInputContext", GLib.Variant("(a(ss))", ([("program", "kmoji-isolated-test")],)))
        path = result.unpack()[0]
        call(path, iface, "FocusIn")
        return path

    def key(path, sym, code, states=0):
        handled = call(path, iface, "ProcessKeyEvent", GLib.Variant("(uuubu)", (sym, code, states, False, 0))).unpack()[0]
        call(path, iface, "ProcessKeyEvent", GLib.Variant("(uuubu)", (sym, code, states, True, 0)))
        return handled

    commits = []
    bus.signal_subscribe(service, iface, "CommitString", None, None, Gio.DBusSignalFlags.NONE,
                         lambda conn, sender, path, interface, signal, params: commits.append((path, params.unpack()[0])))
    try:
        for _ in range(60):
            try:
                call("/controller", "org.fcitx.Fcitx.Controller1", "State")
                break
            except GLib.Error:
                if daemon.poll() is not None: raise RuntimeError("Fcitx exited")
                time.sleep(0.05)
        a = create()
        assert key(a, ord("."), 60, 64), "Addon did not handle Meta+."
        sockpath = next((root / "runtime").glob("kmoji-*.sock"))
        client = socket.socket(socket.AF_UNIX)
        client.settimeout(2)
        client.connect(str(sockpath))
        client.sendall(b"READY\n")
        reply = client.recv(8192).decode()
        token = next(line.split("\t")[1] for line in reply.splitlines() if line.startswith("SHOW\t"))
        client.sendall(f"COMMIT\t{token}\t{'😄'.encode().hex()}\tkeep\n".encode())
        pump()
        assert commits == [(a, "😄")], commits
        client.sendall(f"COMMIT\t{token}\t{'👨‍👩‍👧‍👦👍🏽'.encode().hex()}\tkeep\n".encode())
        pump()
        assert commits[-1] == (a, "👨‍👩‍👧‍👦👍🏽")
        print("PASS: direct input, ZWJ sequences, skin tones, continuous input")

        call(a, iface, "FocusOut")
        b = create()
        count = len(commits)
        client.sendall(f"COMMIT\t{token}\t{'❌'.encode().hex()}\tclose\n".encode())
        pump()
        assert len(commits) == count, "A stale session committed after focus changed"
        call(a, iface, "DestroyIC")
        print("PASS: focus changes and destroyed contexts cannot receive stale input")

        for capability in [1 << 3, 1 << 36]:
            call(b, iface, "SetCapability", GLib.Variant("(t)", (capability,)))
            assert not key(b, ord("."), 60, 64), "Picker opened in a sensitive field"
        call(b, iface, "SetCapability", GLib.Variant("(t)", (0,)))
        print("PASS: password and sensitive fields are excluded")

        assert key(b, ord("."), 60, 64)
        pump()
        reply = client.recv(8192).decode()
        newer = [line.split("\t")[1] for line in reply.splitlines() if line.startswith("SHOW\t")][-1]
        assert newer != token
        client.sendall(f"COMMIT\t{token}\t61\tclose\n".encode())
        pump()
        assert len(commits) == count
        assert key(b, 0xff1b, 9)
        client.close()
        pump(0.15)
        print("PASS: stale tokens and cancellation are rejected")

        # Run the actual Qt frontend against the same native addon, offscreen.
        assert key(b, ord("."), 60, 64)
        ui = subprocess.Popen([str(ROOT / "build/kmoji"), "--socket", str(sockpath), "--data", str(ROOT / "data/emoji.json")], env=env, stdout=log, stderr=log)
        pump(0.7)
        for letter, code in [("c", 54), ("a", 38), ("t", 28)]:
            assert key(b, ord(letter), code)
        pump(0.15)
        key(b, 0xff0d, 36)
        pump(0.2)
        assert len(commits) == count + 1, commits
        assert commits[-1] == (b, "🐈"), commits[-1]
        print("PASS: real Qt frontend searches and commits through Fcitx5:", commits[-1][1])

        # Default all-emoji view and category browsing work without a search.
        assert key(b, ord("."), 60, 64)
        pump(0.15)
        key(b, 0xff09, 23)  # first category: smileys
        key(b, 0xff53, 114)
        key(b, 0xff0d, 36)
        pump(0.2)
        assert len(commits) == count + 2
        print("PASS: browse category and insert without searching")

        assert key(b, ord("."), 60, 64)
        pump(0.1)
        key(b, 0x0100732b, 0)  # 猫, encoded as a Unicode XKB keysym
        pump(0.1)
        key(b, 0xff0d, 36, 1)  # Shift+Enter collects without committing
        pump(0.1)
        assert len(commits) == count + 2
        key(b, ord("a"), 38, 4)  # Ctrl+A replaces the query
        for letter, code in [("d", 40), ("o", 32), ("g", 42)]: key(b, ord(letter), code)
        pump(0.1)
        key(b, 0xff0d, 36, 1)
        pump(0.1)
        key(b, 0xff0d, 36)
        pump(0.2)
        assert commits[-1] == (b, "🐈🐕"), commits[-1]
        print("PASS: Chinese search and multi-emoji collection")

        assert key(b, ord("."), 60, 64)
        pump(0.1)
        for letter in "xiongmao": key(b, ord(letter), 0)
        pump(0.12)
        key(b, ord("d"), 40, 4)  # Ctrl+D adds panda to favorites
        key(b, 0xff0d, 36)
        pump(0.15)
        assert commits[-1] == (b, "🐼"), commits[-1]
        print("PASS: offline pinyin search")
        assert key(b, ord("."), 60, 64)
        pump(0.1)
        key(b, 0xff09, 23, 1)  # Shift+Tab -> favorites
        key(b, 0xff0d, 36)
        pump(0.15)
        assert commits[-1] == (b, "🐼"), commits[-1]
        saved = (root / "config/kmoji/kmoji.conf").read_text()
        assert "favorites=" in saved and "recents=" in saved
        print("PASS: favorite browsing and persistent history")
        call(b, iface, "DestroyIC")
    except BaseException:
        log.flush(); log.seek(0)
        print(log.read(), file=sys.stderr)
        raise
    finally:
        if ui is not None:
            ui.terminate()
            try: ui.wait(timeout=3)
            except subprocess.TimeoutExpired: ui.kill(); ui.wait()
        daemon.terminate()
        try: daemon.wait(timeout=3)
        except subprocess.TimeoutExpired: daemon.kill(); daemon.wait()
        log.close()
