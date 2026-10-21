#!/usr/bin/env python3
"""Generate a release's PKGBUILD and .SRCINFO in a separate AUR checkout."""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import urllib.request

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("version", help="Released version, e.g. 0.1.0")
parser.add_argument("output", type=Path, help="AUR checkout directory")
args = parser.parse_args()
if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", args.version):
    parser.error("version must have the form X.Y.Z")
args.output.mkdir(parents=True, exist_ok=True)
url = f"https://github.com/wanoful/kmoji/archive/refs/tags/v{args.version}.tar.gz"
with urllib.request.urlopen(url, timeout=60) as response:
    digest = hashlib.file_digest(response, "sha256").hexdigest()
template = (Path(__file__).parent / "aur/PKGBUILD.in").read_text()
(args.output / "PKGBUILD").write_text(template.replace("@VERSION@", args.version).replace("@SHA256@", digest))
result = subprocess.run(["makepkg", "--printsrcinfo"], cwd=args.output, check=True, capture_output=True, text=True)
(args.output / ".SRCINFO").write_text(result.stdout)
print(f"Prepared {args.output.resolve()} for kmoji {args.version}")
