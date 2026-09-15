#!/usr/bin/env python3
import argparse
import collections
import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

TREASURY_PER_PROVINCE = 2.5


def convert(dragoman, source, target, to):
    result = subprocess.run([dragoman, "convert", source, target, "--to", to, "--quiet"], capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f"dragoman could not convert {source}: {result.stderr.strip() or result.stdout.strip()}")
    return result.stderr.strip()


def endow(odmap):
    with zipfile.ZipFile(odmap) as archive:
        provinces = json.loads(archive.read("provinces.json"))
        countries = json.loads(archive.read("countries.json"))
        entries = [(info, archive.read(info.filename)) for info in archive.infolist()]
    owned = collections.Counter(p.get("country_id") for p in provinces.values())
    endowed = 0
    for country in countries.values():
        count = owned.get(country.get("id"), 0)
        if count and not country.get("treasury"):
            country["treasury"] = round(TREASURY_PER_PROVINCE * count, 2)
            endowed += 1
    handle, temporary = tempfile.mkstemp(suffix=".odmap", dir=os.path.dirname(os.path.abspath(odmap)))
    os.close(handle)
    with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED) as archive:
        for info, data in entries:
            if info.filename == "countries.json":
                data = json.dumps(countries, ensure_ascii=False, indent=1).encode("utf-8")
            archive.writestr(info, data)
    os.replace(temporary, odmap)
    return endowed


def main():
    ap = argparse.ArgumentParser(description="Put Open Doctrines and Greater Diplomacy 5 on the same world with Dragoman.")
    ap.add_argument("--dragoman", default=shutil.which("dragoman") or "dragoman")
    ap.add_argument("--gd5-scenario", required=True, help="a Greater Diplomacy 5 scenario or map directory")
    ap.add_argument("--odmap", required=True, help="where to write the Open Doctrines copy of that world")
    ap.add_argument("--od-source", help="an Open Doctrines .odmap to convert the other way")
    ap.add_argument("--gd5-map", help="where to write the Greater Diplomacy 5 copy of --od-source")
    args = ap.parse_args()

    os.makedirs(os.path.dirname(os.path.abspath(args.odmap)), exist_ok=True)
    if os.path.exists(args.odmap):
        os.remove(args.odmap)
    convert(args.dragoman, args.gd5_scenario, args.odmap, "odmap")
    endowed = endow(args.odmap)
    report = {"gd5_to_od": {"source": args.gd5_scenario, "odmap": args.odmap, "treasuries_set": endowed,
                            "treasury_per_province": TREASURY_PER_PROVINCE}}

    if args.od_source and args.gd5_map:
        if os.path.exists(args.gd5_map):
            shutil.rmtree(args.gd5_map)
        convert(args.dragoman, args.od_source, args.gd5_map, "gd5")
        report["od_to_gd5"] = {"source": args.od_source, "gd5_map": args.gd5_map}

    print(json.dumps(report, indent=1))


if __name__ == "__main__":
    main()
