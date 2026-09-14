#!/usr/bin/env python3
"""OJH driver for Greater Diplomacy 5: turns per minute, every nation AI, no window.

Run with GD5's own Python (its requirements installed). It boots the real game the way
GD5's own test suite does (tests/app_harness: SDL's dummy video and audio drivers, no
mods), loads the harness's historical scenario, switches the LLM diplomacy off so no
turn waits on the network, and plays turns through GD5's own turn manager.

Every turn is timed twice: the game logic alone (advancing the date and resolving the
turn), and the logic plus refreshing and redrawing the map, which GD5 does every turn
in normal play. One JSON line per turn, then one summary line, on stdout.

    gd5-venv/bin/python drivers/gd5_tpm.py --gd5 path/to/Greater-Diplomacy-5 --turns 50
"""
import argparse
import asyncio
import json
import os
import platform
import subprocess
import sys
import time


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--gd5", required=True, help="the Greater Diplomacy 5 checkout")
    ap.add_argument("--turns", type=int, default=50)
    ap.add_argument("--no-draw", action="store_true", help="skip the per-turn map redraw")
    args = ap.parse_args()

    gd5 = os.path.abspath(args.gd5)
    os.chdir(gd5)  # GD5 loads assets and scenarios by relative path
    sys.path.insert(0, gd5)

    t_boot = time.perf_counter()
    from tests import app_harness
    controller, surface = app_harness.boot()
    game_map = app_harness.boot_map()
    game_map.selection_mode = False
    game_map.force_skip_llm = True
    boot_seconds = time.perf_counter() - t_boot

    from data import queries
    from map_logic.turn_processing import turn_manager
    import pygame

    turns = []

    async def play():
        for i in range(args.turns):
            t0 = time.perf_counter()
            turn_manager.advance_time(game_map)
            await turn_manager._resolve_turn_and_refresh(game_map)
            t1 = time.perf_counter()
            if not args.no_draw:
                game_map.refresh_ui()
                game_map.draw(surface)
            t2 = time.perf_counter()
            living = len(queries.get_living_nations(game_map.map_data))
            row = {"turn": i + 1, "logic_seconds": round(t1 - t0, 6), "total_seconds": round(t2 - t0, 6),
                   "living_nations": living, "date": game_map.time_manager.get_date_string()}
            turns.append(row)
            print(json.dumps(row), flush=True)

    asyncio.run(play())

    logic = sum(t["logic_seconds"] for t in turns)
    total = sum(t["total_seconds"] for t in turns)
    try:
        commit = subprocess.run(["git", "-C", gd5, "rev-parse", "--short", "HEAD"],
                                capture_output=True, text=True).stdout.strip()
    except OSError:
        commit = ""
    summary = {
        "game": "Greater Diplomacy 5",
        "commit": commit,
        "scenario": app_harness.SCENARIO_PATH,
        "turns": len(turns),
        "regions": len(game_map.map_data),
        "nations_at_start": turns[0]["living_nations"] if turns else None,
        "boot_seconds": round(boot_seconds, 3),
        "logic_seconds": round(logic, 3),
        "total_seconds": round(total, 3),
        "tpm_logic": round(len(turns) / (logic / 60.0), 2) if logic > 0 else None,
        "tpm_with_redraw": round(len(turns) / (total / 60.0), 2) if total > 0 and not args.no_draw else None,
        "llm": "off (force_skip_llm)",
        "python": platform.python_version(),
        "pygame": pygame.version.ver,
    }
    print(json.dumps({"summary": summary}), flush=True)


if __name__ == "__main__":
    main()
