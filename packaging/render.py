#!/usr/bin/env python3
import argparse
import hashlib
import os
import pathlib
import re
import sys


def digest(path, algorithm):
    h = hashlib.new(algorithm)
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description="Fills OJH's package manager manifests in for one release.")
    ap.add_argument("--version", required=True)
    ap.add_argument("--dist", required=True, help="folder holding the release files")
    ap.add_argument("--base-url", required=True, help="where the release files are downloaded from")
    ap.add_argument("--out", required=True)
    ap.add_argument("--license", default=os.environ.get("OJH_LICENSE") or "MIT")
    ap.add_argument("--homepage", default="https://github.com/Pr1nted/objective-judge-horizon")
    args = ap.parse_args()

    version = args.version
    files = {
        "source": f"ojh-{version}-source.tar.gz",
        "macos": f"ojh-{version}-macos-universal.tar.gz",
        "linux_x86_64": f"ojh-{version}-linux-x86_64.tar.gz",
        "linux_aarch64": f"ojh-{version}-linux-aarch64.tar.gz",
        "windows_x64": f"ojh-{version}-windows-x64.zip",
    }
    spdx = not args.license.startswith("LicenseRef-")
    values = {
        "version": version,
        "homepage": args.homepage,
        "base_url": args.base_url.rstrip("/"),
        "license": args.license,
        "license_ruby": f'"{args.license}"' if spdx else ":cannot_represent",
        "license_macports": args.license if spdx else "Restrictive",
        "summary": "Benchmark for turn-based strategy games",
        "description": "Objective Judge Horizon measures turn-based strategy games the same way on the same machine: "
                       "turn speed, frame rate, network, footprint and an independent score for every game.",
    }
    missing = []
    for key, name in files.items():
        path = os.path.join(args.dist, name)
        values[f"url_{key}"] = f"{values['base_url']}/{name}"
        values[f"file_{key}"] = name
        if os.path.isfile(path):
            sha256 = digest(path, "sha256")
            values[f"sha256_{key}"] = sha256
            values[f"sha256_{key}_upper"] = sha256.upper()
            values[f"sha512_{key}"] = digest(path, "sha512")
            values[f"size_{key}"] = str(os.path.getsize(path))
        else:
            missing.append(name)

    templates = pathlib.Path(__file__).resolve().parent / "templates"
    out = pathlib.Path(args.out)
    written, skipped = 0, 0
    for template in sorted(templates.rglob("*")):
        if not template.is_file():
            continue
        text = template.read_text(encoding="utf-8")
        used = sorted(set(re.findall(r"\{\{(\w+)\}\}", text + str(template.relative_to(templates)))))
        absent = [u for u in used if u not in values]
        relative = template.relative_to(templates)
        if absent:
            print(f"skipped {relative}: it needs {', '.join(absent)}", file=sys.stderr)
            skipped += 1
            continue
        for u in used:
            text = text.replace("{{" + u + "}}", values[u])
        target_name = str(relative)
        for key, value in values.items():
            target_name = target_name.replace("{{" + key + "}}", value)
        target = out / target_name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8")
        written += 1
    if missing:
        print(f"not in {args.dist}: {', '.join(missing)}", file=sys.stderr)
    if not spdx:
        print(f"license is {args.license}: set --license to OJH's SPDX identifier before publishing", file=sys.stderr)
    print(f"wrote {written} manifests to {out}, skipped {skipped}")
    return 0 if written else 1


if __name__ == "__main__":
    sys.exit(main())
