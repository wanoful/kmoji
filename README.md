# kMoji

An emoji and special symbols picker for KDE Plasma 6 / Wayland. Built with a Qt 6 UI that commits text directly through Fcitx5 without simulating keystrokes or overwriting the clipboard. The UI and the input method addon run in separate processes.

Its compact layout draws inspiration from the Windows emoji panel and [Smile](https://github.com/mijorus/smile): a search bar, a character grid, and a bottom category icon bar. The `Ω` button in the title bar switches between emoji and special symbols; skin tone and additional options expand on demand, and the multi-select composition bar appears only after selecting items.

![Light UI](docs/light.png)

## Features

- Unicode 17.0: 1,914 base emoji with supported skin tone variants.
- 1,600+ special symbols across 8 categories: Punctuation, Currency, Math, Arrows, Shapes, Letters & Numbers, Common, and Other.
- Symbols are inserted as plain text without emoji variation selectors, making them suitable for formulas, documents, and code comments.
- Nine emoji categories and eight symbol categories, all supporting browsing, favorites, and recent history.
- English search is always enabled; Chinese and toneless pinyin can be added or removed as additional search languages in addon settings.
- Supports pasting search keywords; symbols can also be searched by `U+` code point.
- Six skin tone options, multi-emoji composition, and continuous input.
- System, Light, or Dark appearance with a customizable accent color.
- UI text follows the system language (with built-in English source strings and Simplified Chinese translations) or can be pinned in addon settings.
- Hovering over an emoji or symbol for about half a second shows its name; display languages are configurable in addon settings (the UI language is always included), and the label hides automatically when moving away or scrolling.
- Favorites, recent items, continuous input state, and skin tone are stored in `~/.config/kmoji/kmoji.conf`; theme, accent color, search languages, and hover languages are stored in the Fcitx5 addon configuration.
- Input sessions are cancelled when focus changes; password and sensitive input fields do not trigger the picker.

Click `Ω` in the title bar or press `Ctrl + Shift + S` to open the special symbols catalogue:

![Special Symbols UI](docs/symbols.png)

## Usage

| Action | Function |
| --- | --- |
| `Win + .` / `Ctrl + Alt + .` | Open the picker in a text field |
| `Ctrl + Shift + S` / title bar `Ω` | Switch between emoji and special symbols |
| Type text | Search; pinyin can be typed directly (e.g. `xiongmao`) even when a Chinese input method is active |
| `Ctrl + V` | Paste search text |
| Arrow keys / `PageUp` / `PageDown` | Navigate emoji or symbols |
| `Tab` / `Shift + Tab` | Switch categories within the current view |
| Click / `Enter` | Insert the current item; if a composition exists, `Enter` submits the composition |
| `Shift + Click` / `Shift + Enter` | Add the current item to the composition; press `Enter` or click "Insert" to submit |
| Right-click / `Ctrl + D` | Add to / remove from Favorites |
| Top-right `···` | Expand the continuous input option |
| Continuous input | Keep the panel open after inserting |
| `Esc` | Cancel and close |
| Hand icon next to category title | Expand skin tone selector |

Emoji search runs within the current category by default. Select "All emoji" to search across all emoji.
Symbol search always spans all symbol categories and matches English names, Chinese names, pinyin, or hexadecimal code points (e.g. `arrow`, `箭头`, `jiantou`, `2192`). Recent emoji history stores the base emoji and displays/inserts it using the currently selected skin tone; symbols are inserted as-is without skin tone variants. The symbol view also tracks recent items and supports right-click favorites.

## Build & Preview

Dependencies: C++20, CMake, Ninja, Fcitx5 development files, Qt 6 Widgets/Network, Qt LinguistTools, gettext, and a color emoji font.
On Arch Linux: `base-devel cmake ninja fcitx5 qt6-base qt6-tools gettext qt6-wayland noto-fonts-emoji`.

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local" -DCMAKE_INSTALL_LIBDIR=lib
cmake --build build -j4
./build/kmoji --preview
# Preview the special symbols catalogue directly
./build/kmoji --preview --symbols
```

Offline catalogues (`data/emoji.json` and `data/symbols.json`) are bundled in the repository, so no network access is required during build or runtime.
To regenerate the symbol catalogue, run `python scripts/generate-symbols.py`; it reads local Unicode character data only and generates the pinyin index if ICU `uconv` is installed.

## Installation & Activation

### Arch Linux System Package

Download the `.pkg.tar.zst` package from [Releases](https://github.com/wanoful/kmoji/releases), then run:

```sh
sudo pacman -U ./kmoji-*.pkg.tar.zst
# Run as a normal user inside your KDE session to add window rules, free shortcuts, and restart Fcitx5
kmoji-setup
```

The setup script backs up user configuration first—do not run `kmoji-setup` with `sudo`. The system package itself does not touch user configuration files.
If you previously installed a user-local build via the script below, the old addon in `~/.local` will take precedence over the system package; restore from the corresponding installation backup first before installing the system package. See [Publishing & AUR Maintenance](docs/publishing.md) for AUR instructions.

### User-Local Install from Source

The install script additionally requires Python, PyGObject, `busctl`, and KDE's `qdbus6`. It installs only into the current user's `~/.local` directory without `sudo` and backs up existing files to `~/.local/state/kmoji/backup-*` beforehand.

```sh
# Install only, keeping current input method and shortcut settings intact
python scripts/install.py

# Install, add KWin window rules, disable the built-in I'm Emoji Picker, and restart Fcitx5
python scripts/install.py --activate
```

In Fcitx5 Configuration → Addons → kMoji, you can configure:

- **Open emoji picker**: Trigger shortcuts;
- **Theme**: Follow system, Light, or Dark;
- **Accent color**: Color picker used for selection states, category highlights, and the insert button;
- **UI language**: Auto (follow system), English, or Simplified Chinese;
- **Hover languages**: Add or remove "English" and "Chinese" (the active UI language is always shown);
- **Additional emoji search languages**: Add or remove "Chinese" and "Pinyin" (English is always enabled).

All settings take effect the next time the panel opens; saving settings terminates any idle background picker process, so restarting Fcitx5 is not required.

KWin rules prevent the panel from stealing focus and place it on the Popup layer, allowing the target text field to retain its input context. If another application in your desktop session claims the same shortcut, unbind the conflicting shortcut first.

To roll back to the state prior to an installation:

```sh
python scripts/restore.py ~/.local/state/kmoji/backup-YYYYMMDD-HHMMSS
```

Restoring reverts the Fcitx5 and KWin configurations saved in that backup; note that any manual configuration changes made after the backup was created will also be overwritten.

## Verification

```sh
python tests/integration.py
```

The integration test suite launches a private D-Bus session and Fcitx5 instance with an offscreen Qt UI without touching your active desktop input method. It covers direct text commits, ZWJ and skin tone sequences, continuous input, commit rejection after focus loss or context destruction, sensitive input fields, expired sessions, English / Chinese / pinyin search, category navigation, multi-emoji composition, favorites and recent history persistence, symbol catalogue and `U+` code point search, Simplified Chinese translation loading, and English fallback.

After installation, you can run a live Qt text field smoke test in a desktop terminal (additionally requires Qt Gui private development headers):

```sh
cmake -S . -B build -DKMOJI_BUILD_DESKTOP_SMOKE=ON
cmake --build build -j4
QT_IM_MODULE=fcitx ./build/kmoji-desktop-smoke
```

This briefly opens a test text field, sends test key events through Qt's input method interface, verifies cold start, search, and direct text commit, and preserves the field's existing text. It does not simulate global keyboard input to other applications.

To export UI preview screenshots with forced UI or search languages:

```sh
QT_QPA_PLATFORM=offscreen ./build/kmoji --screenshot /tmp/kmoji.png
QT_QPA_PLATFORM=offscreen ./build/kmoji --lang=zh_CN --theme=dark --accent='#ff3b30' --screenshot /tmp/kmoji-dark.png
QT_QPA_PLATFORM=offscreen ./build/kmoji --search-langs=en,zh,pinyin --screenshot /tmp/kmoji-search.png
```

`--lang` also accepts the `KMOJI_LANG` environment variable; `en` in `--search-langs` can be omitted as English is always enabled.

## Translations

UI source strings are written in English. Simplified Chinese translations for the Qt UI are kept in `translations/kmoji_zh_CN.ts`, compiled into `.qm` files by `lrelease` (from Qt LinguistTools) at build time, and embedded into the `kmoji` executable. They are loaded automatically at runtime based on the system locale and fall back to English when no matching translation exists, requiring no extra runtime translation files.

Fcitx5 addon configuration strings use gettext; Simplified Chinese translations live in `po/zh_CN.po` and are compiled by `msgfmt` and installed to `share/locale` during build. The addon display name and description are provided by `Name[zh_CN]` and `Comment` in `data/kmoji.conf`.

To re-extract strings after changing UI text:

```sh
cmake --build build --target update_translations
# Or run directly:
/usr/lib/qt6/bin/lupdate src -ts translations/kmoji_zh_CN.ts -no-obsolete
```

Complete the translations using Qt Linguist or any text editor, then build as usual. To add a new UI language, create `translations/kmoji_XX.ts` and add it to `TS_FILES` in `CMakeLists.txt`; to add a new addon language, add the corresponding `.po` file under `po/` and install it to the matching locale directory.
`qt6-tools` (`lupdate` / `lrelease`) and `gettext` (`msgfmt`) are build-time dependencies only and are not required at runtime.

## Architecture & Scope

- `src/addon.cpp`: Native Fcitx5 module that handles key events and `commitString()` inside the input method event loop. Validates target input contexts using trackable weak references and session IDs. Communicates with the UI over a `0600` Unix domain socket with peer UID verification; sessions are aborted on disconnect, timeout, or focus loss.
- `src/picker.cpp`: Standalone Qt 6 UI responsible for search, rendering, and user preferences. Renders grids via a custom item delegate rather than instantiating a widget per item. Launched on first trigger and reused across subsequent triggers. When spawned by the addon, it disables its own Qt input method context so it never interferes with the target application.
- `scripts/generate-data.py`: Generates categories, Chinese annotations, and skin tone mappings from Unicode 17 / CLDR 48, and builds the pinyin index via ICU `uconv`. Network access and `uconv` are needed only when regenerating data.
- `scripts/generate-symbols.py`: Generates `data/symbols.json` offline from Python's local `unicodedata` and curated Unicode blocks, adding Chinese names, English names, pinyin, and code point indices.
- `data/symbols.json`: Special symbols catalogue sharing commit, favorites, and search infrastructure with emoji.
- `translations/`: Qt translation source files; `.ts` files are tracked in git, while `.qm` files are generated and embedded at build time.
- `po/`: Gettext translation source files for the Fcitx5 addon.

The initial release targets KDE Plasma 6 on Wayland; other desktop environments are untested. Target applications must integrate properly with Fcitx5.
While the picker is open, keyboard input is routed to search and selection rather than Chinese IME composition; you can search directly in pinyin or English, or paste Chinese text.
Kaomoji, custom tags, mixed multi-skin-tone editing, and cursor-relative positioning are currently out of scope (the panel is centered by KWin).
The special symbols catalogue covers common Unicode punctuation, symbol, and number blocks rather than every obscure block.
Emoji rendering depends on the system color emoji font, while special symbol rendering depends on glyph coverage in the system text fonts.

Code is licensed under the MIT License; Unicode / CLDR data is distributed under the [Unicode License](data/UNICODE-LICENSE.txt).
