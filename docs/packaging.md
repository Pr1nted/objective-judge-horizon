# Packaging and releases

OJH ships the same files to every package manager: the `ojh` binary, its game drivers,
the example specs, `tools/run-all.sh` and the documentation. An installed `ojh` finds its
drivers at `../share/ojh/drivers` next to the binary, so it works from any folder.

## Making a release

1. Set the version in `CMakeLists.txt` (`OJH_RELEASE_VERSION`).
2. Tag it and push the tag:

   ```bash
   git tag v0.1.0
   ```

   ```bash
   git push origin v0.1.0
   ```

3. `.github/workflows/release.yml` builds and self-tests OJH on Linux x86_64, Linux
   aarch64, macOS (one universal binary for Apple silicon and Intel) and Windows x64.
   It packages each one, and adds the source archive and `SHA256SUMS`.
4. The same workflow fills every package manager manifest in with that release's real
   download URLs and checksums (`packaging/render.py`). It attaches them as
   `ojh-<version>-manifests.tar.gz` to the GitHub release.

To fill the manifests in by hand for files you already have:

```bash
python3 packaging/render.py --version 0.1.0 --dist dist --base-url https://github.com/Pr1nted/objective-judge-horizon/releases/download/v0.1.0 --out manifests
```

`render.py` refuses to write a manifest with a value it could not fill in, and says which
release file was missing.

## What a release contains

| File | For |
|---|---|
| `ojh-<version>-linux-x86_64.tar.gz`, `ojh-<version>-linux-aarch64.tar.gz` | any Linux, and the AUR `ojh-bin` package |
| `ojh_<version>_amd64.deb`, `ojh_<version>_arm64.deb` | Debian, Ubuntu and derivatives |
| `ojh-<version>-1.x86_64.rpm`, `ojh-<version>-1.aarch64.rpm` | Fedora, RHEL, openSUSE |
| `ojh-<version>-macos-universal.tar.gz` | macOS, and the Homebrew cask |
| `ojh-<version>-windows-x64.zip` | Windows, winget, Scoop and Chocolatey |
| `ojh-<version>-source.tar.gz` | Homebrew formula, AUR `ojh`, Fedora COPR, Nix, MacPorts, Alpine |
| `ojh-<version>-manifests.tar.gz` | every manifest below, filled in |
| `SHA256SUMS` | checksums of all of the above |

## The manifests

| Package manager | Manifest | Installs | Where it is published |
|---|---|---|---|
| Homebrew formula | `homebrew/Formula/ojh.rb` | builds from source | a tap repository, e.g. `Pr1nted/homebrew-ojh` |
| Homebrew cask | `homebrew/Casks/ojh.rb` | the prebuilt macOS binary | the same tap |
| winget | `winget/manifests/p/Pr1nted/ObjectiveJudgeHorizon/<version>/` | the Windows zip, as a portable command | a pull request to `microsoft/winget-pkgs` |
| Scoop | `scoop/ojh.json` | the Windows zip | a bucket repository, e.g. `Pr1nted/scoop-ojh` |
| Chocolatey | `chocolatey/ojh.nuspec` and `tools/` | the Windows zip | `choco pack`, then `choco push` to community.chocolatey.org |
| pacman (AUR) | `aur/ojh/PKGBUILD`, `aur/ojh-bin/PKGBUILD` and their `.SRCINFO` | from source, or the prebuilt binary | `ssh://aur@aur.archlinux.org/ojh.git` and `ojh-bin.git` |
| apt | the `.deb` files | the prebuilt binary | the release itself, or an apt repository |
| dnf and yum | the `.rpm` files, or `rpm/ojh.spec` | prebuilt, or built by Fedora COPR | the release, or a COPR project |
| Nix | `nix/default.nix` | from source | a pull request to `NixOS/nixpkgs` |
| MacPorts | `macports/devel/ojh/Portfile` | from source | a pull request to `macports/macports-ports` |
| apk (Alpine) | `alpine/ojh/APKBUILD` | from source | a merge request to `alpine/aports` |

## Before publishing anywhere

- **The repository must be public.** Every manifest downloads from the GitHub release, and
  a private repository's release files cannot be downloaded without signing in.
- **OJH needs a license.** Until one is chosen, the manifests say
  `LicenseRef-OJH-Pending`. Once it is chosen, add a `LICENSE` file and set the
  `OJH_LICENSE` repository variable to its SPDX identifier (for example `MIT`). The
  release workflow writes that identifier into every manifest. winget, the AUR, nixpkgs,
  MacPorts and Alpine will not accept a package without a real license.
- **The Homebrew cask needs a binary signed and notarized with an Apple Developer ID.**
  Without it, macOS refuses to open the downloaded `ojh` ("Apple could not verify ojh is
  free of malware"). This was tested: the unsigned cask installs, then is killed on launch.
  The Homebrew formula builds from source and needs no signing, so it is the one to
  publish first.
- **winget and Chocolatey** review every new package by hand before it appears.

## Installing

After publishing:

```bash
brew install Pr1nted/ojh/ojh
```

```powershell
winget install Pr1nted.ObjectiveJudgeHorizon
```

```powershell
scoop bucket add ojh https://github.com/Pr1nted/scoop-ojh
```

```powershell
scoop install ojh
```

```powershell
choco install ojh
```

```bash
yay -S ojh
```

```bash
sudo apt install ./ojh_0.1.0_amd64.deb
```

```bash
sudo dnf install ./ojh-0.1.0-1.x86_64.rpm
```

From source anywhere:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build
```

```bash
cmake --install build
```
