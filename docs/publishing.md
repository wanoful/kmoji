# Publishing & AUR Maintenance

Source repository: https://github.com/wanoful/kmoji

## Releasing a New Version

1. Update the version number in both `CMakeLists.txt` and `data/kmoji.conf`.
2. Build and run `python tests/integration.py`, then commit the changes.
3. Create a version tag (e.g. `v0.1.0`), push it to GitHub, and create the corresponding GitHub Release.
4. Generate the AUR files and build the package using the script below, then upload the package and checksum files to the Release.

Do not move a version tag once published, as doing so changes the checksum of the source archive.
Initial release binaries are built on Arch Linux `x86_64`; other architectures have not been tested yet.

## Initial AUR Submission

The AUR repository stores only `PKGBUILD` and `.SRCINFO`—do not upload source archives or binary packages.
First, register an [AUR account](https://aur.archlinux.org/register) and add your SSH public key in account settings.
See the [ArchWiki AUR submission guidelines](https://wiki.archlinux.org/title/AUR_submission_guidelines) for authentication and packaging rules.

```sh
# Run from the kMoji source directory; the AUR repo is kept separate from the source repo
git -c init.defaultBranch=master clone ssh://aur@aur.archlinux.org/kmoji.git ../kmoji-aur
python packaging/prepare-aur.py 0.1.0 ../kmoji-aur
cd ../kmoji-aur
makepkg -s
# Optional but recommended: check PKGBUILD and the built package with namcap
namcap PKGBUILD kmoji-0.1.0-1-x86_64.pkg.tar.zst
git add PKGBUILD .SRCINFO
git commit -m 'Initial release: kmoji 0.1.0'
git push -u origin master
```

Before pushing for the first time, verify that the package name is available and inspect the generated files. Never commit `src/`, `pkg/`, source tarballs, or `.pkg.tar.zst` archives to the AUR. After installing the package, users run `kmoji-setup` inside their KDE session to enable it.

## Updating the AUR Package

Publish the new GitHub tag first, pull the AUR repository, re-run `prepare-aur.py`, build and verify the package, and then commit `PKGBUILD` and `.SRCINFO`. If you are only updating packaging without a new upstream release, increment `pkgrel` instead.
Note that `prepare-aur.py` resets `pkgrel` to `1` when generating files; adjust `pkgrel` manually when making packaging-only revisions to an existing upstream version.

If you have a release ready but have not set up your AUR SSH access yet, you can generate the files into a regular directory first:

```sh
python packaging/prepare-aur.py 0.1.0 ../kmoji-aur
```

Once SSH access is configured, copy `PKGBUILD` and `.SRCINFO` into your official AUR checkout and commit them.
