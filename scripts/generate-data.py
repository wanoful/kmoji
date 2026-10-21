#!/usr/bin/env python3
"""Build the offline catalogue from pinned Unicode 17 / CLDR 48 sources."""
import json
import re
import subprocess
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / "data/source"
CACHE.mkdir(parents=True, exist_ok=True)


def fetch(name, url):
    path = CACHE / name
    if not path.exists():
        print("Downloading", name, flush=True)
        with urllib.request.urlopen(url, timeout=60) as response:
            path.write_bytes(response.read())
    return path.read_text()


def annotations(lang, derived=False):
    folder = "annotationsDerived" if derived else "annotations"
    package = "cldr-annotations-derived-full" if derived else "cldr-annotations-full"
    url = f"https://raw.githubusercontent.com/unicode-org/cldr-json/48.0.0/cldr-json/{package}/{folder}/{lang}/annotations.json"
    return json.loads(fetch(f"{lang}-{folder}.json", url))[folder]["annotations"]


def normalize(text):
    return text.replace("\ufe0f", "")


en = {**annotations("en"), **annotations("en", True)}
zh = {**annotations("zh"), **annotations("zh", True)}
en = {normalize(k): v for k, v in en.items()}
zh = {normalize(k): v for k, v in zh.items()}
source = fetch("emoji-test.txt", "https://unicode.org/Public/17.0.0/emoji/emoji-test.txt")
groups = ["Smileys & Emotion", "People & Body", "Animals & Nature", "Food & Drink", "Travel & Places", "Activities", "Objects", "Symbols", "Flags"]
rows = []
category = 0
subgroup = ""
for line in source.splitlines():
    if line.startswith("# group:"):
        group = line.split(":", 1)[1].strip()
        category = groups.index(group) if group in groups else -1
    elif line.startswith("# subgroup:"):
        subgroup = line.split(":", 1)[1].strip()
    elif "; fully-qualified" in line and category >= 0:
        codes = line.split(";", 1)[0].split()
        text = "".join(chr(int(c, 16)) for c in codes)
        english = re.split(r" E[\d.]+ ", line.split("#", 1)[1], maxsplit=1)[1].strip()
        rows.append((text, english, category, subgroup))

tones = [chr(n) for n in range(0x1F3FB, 0x1F400)]
by_base = {}
for text, english, category, subgroup in rows:
    base = normalize("".join(c for c in text if c not in tones))
    by_base.setdefault(base, []).append(text)

entries = []
for text, english, category, subgroup in rows:
    if any(t in text for t in tones):
        continue
    key = normalize(text)
    z = zh.get(key, {})
    e = en.get(key, {})
    name = z.get("tts", [english])[0]
    keywords = list(dict.fromkeys([english, *e.get("default", []), name, *z.get("default", []), subgroup]))
    variants = [text]
    for tone in tones:
        variants.append(next((v for v in by_base[key] if tone in v and all(t not in v for t in tones if t != tone)), text))
    entries.append(dict(text=text, name=name, english=english, category=category, keywords=" ".join(keywords), aliases=keywords, tones=variants))

# ICU transliteration provides offline pinyin search without a runtime dependency.
all_aliases = [alias for e in entries for alias in e["aliases"]]
chinese = "\n".join(all_aliases)
result = subprocess.run(["uconv", "-x", "Han-Latin; Latin-ASCII; Lower()"], input=chinese, text=True, capture_output=True, check=True)
romanized = iter(result.stdout.splitlines())
for entry in entries:
    pinyin_aliases = [next(romanized) for _ in entry["aliases"]]
    entry["pinyin"] = " ".join(pinyin_aliases)
    entry["aliases"] = list(dict.fromkeys([*entry["aliases"], *(re.sub(r"\s+", "", alias) for alias in pinyin_aliases)]))

catalogue = {"unicode": "17.0", "cldr": "48.0.0", "entries": entries}
(ROOT / "data/emoji.json").write_text(json.dumps(catalogue, ensure_ascii=False, separators=(",", ":")) + "\n")
license_text = fetch("UNICODE-LICENSE.txt", "https://www.unicode.org/license.txt")
(ROOT / "data/UNICODE-LICENSE.txt").write_text(license_text)
print(f"Generated {len(entries)} base emoji with skin-tone variants")
