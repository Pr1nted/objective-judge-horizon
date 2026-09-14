#!/usr/bin/env python3
import argparse
import asyncio
import os
import sys
import time


def say(text):
    print(text, flush=True)


def summary(name, times):
    times = sorted(times)
    n = len(times)
    total = sum(times)

    def percentile(p):
        return times[min(n - 1, int(p * (n - 1)))] * 1000.0

    slowest = times[-max(1, n // 100):]
    low = len(slowest) / sum(slowest) if sum(slowest) > 0 else 0.0
    return f"OJH scene {name} {n} {total:.5f} {percentile(0.5):.3f} {percentile(0.95):.3f} {percentile(0.99):.3f} {low:.2f}"


def measure(name, screen, surface, seconds, before=None, warmup=30):
    import pygame
    for _ in range(warmup):
        if before:
            before()
        pygame.event.pump()
        screen.update()
        screen.draw(surface)
        pygame.display.flip()
    times = []
    end = time.perf_counter() + seconds
    while time.perf_counter() < end:
        start = time.perf_counter()
        if before:
            before()
        pygame.event.pump()
        screen.update()
        screen.draw(surface)
        pygame.display.flip()
        times.append(time.perf_counter() - start)
    say(summary(name, times))


def main():
    ap = argparse.ArgumentParser(description="OJH frame-rate driver for Greater Diplomacy 5: the same scenes as every other game.")
    ap.add_argument("--gd5", required=True)
    ap.add_argument("--seconds", type=float, default=5.0)
    ap.add_argument("--turns", type=int, default=20)
    args = ap.parse_args()

    gd5 = os.path.abspath(args.gd5)
    os.chdir(gd5)
    sys.path.insert(0, gd5)

    from tests import app_harness
    controller, surface = app_harness.boot()
    game_map = app_harness.boot_map()
    game_map.selection_mode = False
    game_map.force_skip_llm = True

    import pygame
    import data.constants as c
    from map_logic.turn_processing import turn_manager

    say(f"OJH renderer SDL dummy video driver, pygame software drawing (pygame {pygame.version.ver}, SDL {'.'.join(map(str, pygame.get_sdl_version()))})")
    say(f"OJH resolution {c.SCREEN_WIDTH}x{c.SCREEN_HEIGHT}")
    say("OJH vsync off")

    measure("menu", controller.states["MENU"], surface, args.seconds)

    camera = game_map.camera
    start_zoom = camera.zoom
    start_x, start_y = camera.pos.x, camera.pos.y

    def set_zoom(zoom):
        camera.zoom = zoom
        camera.target_zoom = zoom
        camera.pos.x, camera.pos.y = start_x, start_y
        if hasattr(camera, "target_pos"):
            camera.target_pos.x, camera.target_pos.y = start_x, start_y

    set_zoom(start_zoom)
    measure("map-start", game_map, surface, args.seconds)
    set_zoom(game_map.min_zoom)
    measure("map-out", game_map, surface, args.seconds)
    set_zoom(c.MAX_CAMERA_ZOOM)
    measure("map-in", game_map, surface, args.seconds)

    set_zoom(max(start_zoom, game_map.min_zoom * 3))

    def pan():
        camera.pos.x += 12.0 / camera.zoom
        if hasattr(camera, "target_pos"):
            camera.target_pos.x = camera.pos.x

    measure("map-pan", game_map, surface, args.seconds, before=pan)
    set_zoom(start_zoom)

    economy = controller.states.get("ECONOMY")
    if economy is not None:
        measure("panel", economy, surface, args.seconds)
    else:
        say("OJH noscene panel GD5 has no economy screen in this build")

    async def play():
        for _ in range(args.turns):
            turn_manager.advance_time(game_map)
            await turn_manager._resolve_turn_and_refresh(game_map)

    asyncio.run(play())
    game_map.refresh_ui()
    set_zoom(start_zoom)
    measure("map-late", game_map, surface, args.seconds)
    say("OJH noscene end-turn GD5 resolves a turn without drawing frames while it works")


if __name__ == "__main__":
    main()
