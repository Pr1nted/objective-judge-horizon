#!/usr/bin/env python3
import argparse
import os
import sys
import threading
import time


def say(text):
    print(text, flush=True)


def host(args):
    gd5 = os.path.abspath(args.gd5)
    os.chdir(gd5)
    sys.path.insert(0, gd5)
    from tests import app_harness
    app_harness.boot()
    game_map = app_harness.boot_map()
    game_map.selection_mode = False
    game_map.force_skip_llm = True

    from data import queries
    from data.io.realtime_multiplayer import (MapRealtimeDriver, RealtimeConfig, RealtimeServer, RealtimeSession,
                                              create_match_certificate, encode_invite)

    countries = sorted(queries.get_living_nations(game_map.map_data))
    config = RealtimeConfig(app_harness.SCENARIO_PATH, {}, max_players=args.clients + 1, max_turns=args.turns,
                            turn_minutes=240, advertised_address="127.0.0.1", port=args.port)
    session = RealtimeSession(config, countries, "OJH Host", MapRealtimeDriver(game_map))
    certificate, key, fingerprint = create_match_certificate(os.path.join(args.work, "gd5-certificate"))
    server = RealtimeServer(session, certificate, key, "127.0.0.1")
    server.start(args.port)

    invite = encode_invite("127.0.0.1", args.relay_port, session.session_id, fingerprint)
    os.makedirs(args.work, exist_ok=True)
    temporary = os.path.join(args.work, "gd5-invite.tmp")
    with open(temporary, "w") as f:
        f.write(invite)
    os.replace(temporary, os.path.join(args.work, "gd5-invite.txt"))

    done = threading.Event()
    reported = {"turn": 1}

    def watch(state):
        if state.get("phase") == "TURN" and state.get("turn", 1) > reported["turn"]:
            for finished in range(reported["turn"], state["turn"]):
                say(f"OJH turn {finished}")
            reported["turn"] = state["turn"]
        if state.get("phase") == "GAME_OVER":
            if not state.get("processing") and reported["turn"] <= args.turns and state.get("game_over_reason") == "turn_limit":
                say(f"OJH turn {reported['turn']}")
                reported["turn"] += 1
            done.set()

    session.add_listener(watch)
    session.select_country(session.host_id, countries[0])
    session.set_ready(session.host_id, True)
    say(f"OJH players {len(countries)}")
    say(f"OJH regions {len(game_map.map_data)} provinces")
    say("OJH ready")

    deadline = time.monotonic() + args.join_timeout
    assigned = {session.host_id}
    while time.monotonic() < deadline:
        guests = [p for p in list(session.players.values()) if p.player_id not in assigned]
        for index, guest in enumerate(guests):
            session.select_country(guest.player_id, countries[len(assigned)])
            session.set_ready(guest.player_id, True)
            assigned.add(guest.player_id)
        if len(assigned) >= args.clients + 1:
            break
        time.sleep(0.05)
    if len(assigned) < args.clients + 1:
        say("OJH error not every client joined")
        server.stop("timeout")
        sys.exit(2)

    session.start(session.host_id)
    say("OJH started")
    submitted = 0
    while not done.is_set():
        if session.phase == "TURN" and session.turn_number != submitted:
            submitted = session.turn_number
            session.submit(session.host_id, submitted)
        time.sleep(0.02)
    time.sleep(1.0)
    server.stop("finished")


def client(args):
    gd5 = os.path.abspath(args.gd5)
    os.chdir(gd5)
    sys.path.insert(0, gd5)
    os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
    os.environ.setdefault("SDL_AUDIODRIVER", "dummy")
    from data.io.realtime_multiplayer import RealtimeClient, decode_invite

    deadline = time.monotonic() + 120
    while not os.path.exists(args.invite_file) and time.monotonic() < deadline:
        time.sleep(0.05)
    with open(args.invite_file) as f:
        invite = decode_invite(f.read().strip())
    connection = RealtimeClient(invite)
    connection.connect()
    connection.send("join", {"name": args.name, "password": ""})
    submitted = 0
    while True:
        for event in connection.poll():
            kind = event.get("type")
            if kind in ("disconnected", "shutdown", "kicked"):
                return
            state = event.get("payload", {}).get("state") if kind == "ok" else event.get("payload") if kind == "state" else None
            if not state:
                continue
            if state.get("phase") == "GAME_OVER":
                connection.close()
                return
            if state.get("phase") == "TURN" and state.get("turn") != submitted and connection.player_id:
                mine = next((p for p in state.get("players", []) if p.get("player_id") == connection.player_id), None)
                if mine and not mine.get("submitted"):
                    submitted = state["turn"]
                    connection.send("submit", {"turn": submitted})
        time.sleep(0.01)


def main():
    ap = argparse.ArgumentParser(description="OJH network driver for Greater Diplomacy 5: a real-time match through OJH's relay.")
    ap.add_argument("role", choices=["host", "client"])
    ap.add_argument("--gd5", required=True)
    ap.add_argument("--port", type=int, default=38475)
    ap.add_argument("--relay-port", type=int, default=0)
    ap.add_argument("--clients", type=int, default=2)
    ap.add_argument("--turns", type=int, default=20)
    ap.add_argument("--work", default=".")
    ap.add_argument("--join-timeout", type=float, default=180)
    ap.add_argument("--invite-file", default="gd5-invite.txt")
    ap.add_argument("--name", default="OJH Guest")
    args = ap.parse_args()
    if args.role == "host":
        host(args)
    else:
        client(args)


if __name__ == "__main__":
    main()
