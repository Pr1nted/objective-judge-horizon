#!/usr/bin/env python3
import argparse
import asyncio
import os
import shutil
import sys
import time


def folder_bytes(path):
    total = 0
    for root, _, files in os.walk(path):
        for name in files:
            total += os.path.getsize(os.path.join(root, name))
    return total


def main():
    ap = argparse.ArgumentParser(description="OJH footprint driver for Greater Diplomacy 5: save size, save time and load time.")
    ap.add_argument("--gd5", required=True, help="the Greater Diplomacy 5 checkout")
    ap.add_argument("--scenario", default="scenarios/historical/1939")
    ap.add_argument("--turns", type=int, default=10)
    args = ap.parse_args()

    gd5 = os.path.abspath(args.gd5)
    os.chdir(gd5)
    sys.path.insert(0, gd5)

    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import gd5_tpm
    _, _, game_map = gd5_tpm.boot(gd5, args.scenario)

    import data.constants as c
    from data.map.save_map import save_map_data

    async def play():
        for _ in range(args.turns):
            await gd5_tpm.play_turn(game_map)

    asyncio.run(play())

    name = "ojh-footprint"
    path = os.path.join(c.SAVES_DIR, name)
    shutil.rmtree(path, ignore_errors=True)
    t0 = time.perf_counter()
    asyncio.run(save_map_data(game_map, name))
    save_seconds = time.perf_counter() - t0
    if not os.path.isdir(path) or folder_bytes(path) == 0:
        print(f"the save was not written to {path}", file=sys.stderr, flush=True)
        sys.exit(1)
    print(f"OJH save {folder_bytes(path)} {save_seconds:.6f}", flush=True)

    import main as gd5_main
    t1 = time.perf_counter()
    gd5_main.Map(load_path=path, is_scenario=False, num_players=1)
    load_seconds = time.perf_counter() - t1
    print(f"OJH load {load_seconds:.6f}", flush=True)

    shutil.rmtree(path, ignore_errors=True)


if __name__ == "__main__":
    main()
