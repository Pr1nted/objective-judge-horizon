#!/usr/bin/env python3
import argparse
import asyncio
import contextlib
import json
import os
import platform
import subprocess
import sys
import time


async def play_turn(game_map):
    from map_logic.ai import ai_handler
    from map_logic.turn_processing import turn_manager

    pending = []

    def hold(fn, *args, **kwargs):
        pending.append((fn, args, kwargs))

    async def run_pending():
        while pending:
            fn, args, kwargs = pending.pop(0)
            result = fn(*args, **kwargs)
            if asyncio.iscoroutine(result):
                await result

    background = turn_manager.run_background
    turn_manager.run_background = hold
    try:
        started = time.perf_counter()
        turn_manager.advance_time(game_map)
        game_map.force_skip_llm = True
        ai_handler.FORCE_SKIP = True
        await run_pending()
        game_map.update()
        await run_pending()
        seconds = time.perf_counter() - started
    finally:
        turn_manager.run_background = background
    if game_map.thread_error:
        raise RuntimeError(game_map.thread_error)
    return seconds


def boot(gd5, scenario):
    os.chdir(gd5)
    sys.path.insert(0, gd5)
    from tests import app_harness
    app_harness.SCENARIO_PATH = scenario
    controller, surface = app_harness.boot()
    game_map = app_harness.boot_map()
    game_map.player_country = "Spectator"
    game_map.active_players = []
    game_map.selection_mode = False
    game_map.skip_ai_view = True
    return controller, surface, game_map


def main():
    ap = argparse.ArgumentParser(description="OJH driver for Greater Diplomacy 5 without its own ojh_benchmark.py: turns per minute, every nation AI, no window.")
    ap.add_argument("--gd5", required=True, help="the Greater Diplomacy 5 checkout")
    ap.add_argument("--scenario", default="scenarios/historical/1939")
    ap.add_argument("--turns", type=int, default=50)
    args = ap.parse_args()

    gd5 = os.path.abspath(args.gd5)
    out = sys.stdout
    t_boot = time.perf_counter()
    with contextlib.redirect_stdout(sys.stderr):
        _, _, game_map = boot(gd5, args.scenario)
        from data import queries
        import pygame
        nations = len(queries.get_active_ai_nations(game_map))
    boot_seconds = time.perf_counter() - t_boot

    turns = []

    async def play():
        for i in range(args.turns):
            with contextlib.redirect_stdout(sys.stderr):
                seconds = await play_turn(game_map)
            row = {"turn": i + 1, "total_seconds": round(seconds, 6),
                   "living_nations": len(queries.get_living_nations(game_map.map_data)),
                   "date": game_map.time_manager.get_date_string()}
            turns.append(row)
            print(json.dumps(row), file=out, flush=True)

    asyncio.run(play())

    total = sum(t["total_seconds"] for t in turns)
    try:
        commit = subprocess.run(["git", "-C", gd5, "rev-parse", "--short", "HEAD"],
                                capture_output=True, text=True).stdout.strip()
    except OSError:
        commit = ""
    summary = {
        "game": "Greater Diplomacy 5",
        "commit": commit,
        "scenario": args.scenario,
        "turns": len(turns),
        "regions": len(game_map.map_data),
        "nations_at_start": nations,
        "boot_seconds": round(boot_seconds, 3),
        "total_seconds": round(total, 3),
        "tpm": round(len(turns) / (total / 60.0), 2) if total > 0 else None,
        "llm": "skipped every turn, as Force Skip",
        "python": platform.python_version(),
        "pygame": pygame.version.ver,
    }
    print(json.dumps({"summary": summary}), file=out, flush=True)


if __name__ == "__main__":
    main()
