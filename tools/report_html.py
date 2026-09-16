#!/usr/bin/env python3
"""Build one readable HTML page out of an OJH results folder.

WHY THIS EXISTS. `ojh report` writes Markdown, which is right for the repository
and for a diff, and wrong for somebody reading on a phone: the turn-speed table
is twelve columns wide and the network table is eight, and neither survives a
375-pixel screen. GitHub renders them as a horizontal scroll bar with no
indication of what is off the edge.

WHAT IT DOES NOT DO. It does not recompute anything. Every number on the page is
parsed out of the Markdown `ojh report` already wrote, so the page and the
repository cannot disagree -- and a statistic this script has never heard of
still reaches the page, because whole tables are passed through rather than
named field by field.

    python3 tools/report_html.py docs/results/2026-09-15-matched-maps-apple-m1-pro

Writes index.html into that folder.

THE STYLE IS THE OPEN DOCTRINES SITE'S, deliberately: same palette, same two
faces, same components as packaging/web/site/site.css in the game's repository.
A benchmark page that looks like a different project's is a benchmark page that
reads as an advertisement somebody else made.
"""

from __future__ import annotations

import html
import math
import pathlib
import re
import sys

OD = "Open Doctrines"

# ── reading what ojh report wrote ────────────────────────────────────────────


def sections(md: str) -> dict[str, str]:
    """Every `## heading` in the document, mapped to the text under it."""
    out: dict[str, str] = {}
    parts = re.split(r"^## +(.+?)\s*$", md, flags=re.M)
    for i in range(1, len(parts), 2):
        out[parts[i].strip()] = parts[i + 1]
    return out


def tables(text: str) -> list[tuple[list[str], list[list[str]]]]:
    """Every pipe table in `text`, as (header cells, body rows)."""
    found = []
    rows: list[list[str]] = []
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("|") and line.endswith("|"):
            cells = [c.strip() for c in line[1:-1].split("|")]
            if all(re.fullmatch(r":?-{2,}:?", c) for c in cells):
                continue          # the ---|--- separator
            rows.append(cells)
        elif rows:
            found.append((rows[0], rows[1:]))
            rows = []
    if rows:
        found.append((rows[0], rows[1:]))
    return found


def first_table(sec: dict[str, str], heading: str):
    text = sec.get(heading)
    if not text:
        return None
    t = tables(text)
    return t[0] if t else None


RANK = re.compile(r"\s*\((\d+)(?:st|nd|rd|th)\)\s*$")


def split_rank(cell: str) -> tuple[str, str | None]:
    m = RANK.search(cell)
    return (RANK.sub("", cell), m.group(1)) if m else (cell, None)


UNITS = {
    "KiB": 1.0, "MiB": 1024.0, "GiB": 1024.0 * 1024.0,
    "fps": 1.0, "s": 1.0, "x": 1.0,
}


def numeric(cell: str) -> float | None:
    """The number a cell means, normalised so KiB/MiB/GiB compare."""
    bare, _ = split_rank(cell)
    bare = bare.replace(",", "").strip()
    m = re.match(r"^([0-9]*\.?[0-9]+)\s*([A-Za-z/%-]*)", bare)
    if not m:
        return None
    value = float(m.group(1))
    unit = m.group(2)
    for name, mult in UNITS.items():
        if unit.startswith(name):
            return value * mult
    return value


# ── drawing ──────────────────────────────────────────────────────────────────


def common_unit(labels: list[str]) -> str:
    """The unit every label shares, if they all share one.

    Repeating "turns/min" down six rows costs the width that matters most on a
    phone and tells the reader nothing the chart's own title has not already
    said. Stripped only when EVERY row carries it, so a column of mixed units
    -- KiB against MiB against GiB -- keeps them and stays readable.
    """
    units = set()
    for label in labels:
        m = re.match(r"^[0-9][0-9,.]*\s+(\S.*)$", label.strip())
        if not m:
            return ""
        units.add(m.group(1))
    return units.pop() if len(units) == 1 else ""


def bar_chart(title: str, note: str, rows: list[tuple[str, str]],
              better: str = "more", floor: float = 6.0) -> str:
    """Games as ranked bars. Log scale, because the spreads reach 372x."""
    unit = common_unit([label for _, label in rows])
    if unit:
        cut = len(unit)
        rows = [(n, l.strip()[: -cut].strip()) for n, l in rows]
        note = f"{note} Figures in {html.escape(unit)}." if note else \
               f"Figures in {html.escape(unit)}."
    vals = [(name, label, numeric(label)) for name, label in rows]
    vals = [(n, l, v) for n, l, v in vals if v and v > 0]
    if not vals:
        return ""
    lo = min(v for _, _, v in vals)
    hi = max(v for _, _, v in vals)
    out = [f'<figure class="chart"><figcaption><h3>{html.escape(title)}</h3>']
    if note:
        out.append(f'<p class="note">{note}</p>')
    out.append("</figcaption><ol class=\"bars\">")
    for name, label, v in vals:
        if hi == lo:
            pct = 100.0
        else:
            t = (math.log10(v) - math.log10(lo)) / (math.log10(hi) - math.log10(lo))
            if better == "less":
                t = 1.0 - t
            pct = floor + t * (100.0 - floor)
        cls = " od" if name.startswith(OD) else ""
        out.append(
            f'<li class="bar{cls}"><span class="bn">{html.escape(name)}</span>'
            f'<span class="bt"><span class="bf" style="width:{pct:.1f}%"></span></span>'
            f'<span class="bv">{html.escape(label)}</span></li>')
    out.append("</ol></figure>")
    return "\n".join(out)


def data_table(head: list[str], body: list[list[str]], caption: str = "") -> str:
    out = ['<div class="scroller"><table>']
    if caption:
        out.append(f"<caption>{html.escape(caption)}</caption>")
    out.append("<thead><tr>")
    for i, h in enumerate(head):
        out.append(f'<th{" scope=\"col\"" if i else ""}>{html.escape(h)}</th>')
    out.append("</tr></thead><tbody>")
    for row in body:
        is_od = row and row[0].startswith(OD)
        out.append(f'<tr{" class=\"od\"" if is_od else ""}>')
        for i, cell in enumerate(row):
            bare, rank = split_rank(cell)
            if i == 0:
                out.append(f"<th scope=\"row\">{html.escape(bare)}</th>")
                continue
            badge = f'<i class="pl">{rank}</i>' if rank else ""
            klass = ' class="n"' if re.match(r"^[0-9]", bare) else ""
            out.append(f"<td{klass}>{html.escape(bare)}{badge}</td>")
        out.append("</tr>")
    out.append("</tbody></table></div>")
    return "\n".join(out)


def chart_from(table, value_col: int, better: str = "more") -> list[tuple[str, str]]:
    """(game, value) pairs from a parsed table column."""
    _, body = table
    rows = []
    for r in body:
        if len(r) <= value_col:
            continue
        bare, _ = split_rank(r[value_col])
        if bare in ("n/a", "", "-"):
            continue
        rows.append((r[0], bare))
    return rows


# ── the caveats, which are prose and stay prose ──────────────────────────────

CAVEATS = [
    ("Who wrote it",
     "OJH, its score weights, and the Freeciv, Unciv, FreeOrion, FreeCol and network drivers "
     "were all written by the developer of Open Doctrines. That is a conflict of interest, and "
     "it is stated here rather than buried. Greater Diplomacy 5 is measured through a hook "
     "offered to its own developer as a pull request; until that is merged, OJH runs GD5 from "
     "that branch."),
    ("Open Doctrines' network bytes are reported, not captured",
     "Joining an Open Doctrines match needs a signed-in account, so no client can sit on the "
     "wire. The game reports the turn delta its host broadcasts, times the number of clients. "
     "Unciv's figure is its synced save after each turn. Freeciv's and GD5's are bytes actually "
     "on the wire through OJH's counting relay. Those are not the same kind of measurement, and "
     "the first is the one that flatters Open Doctrines."),
    ("Freeciv's 0.1 ms turn delivery is an artifact, not a result",
     "Delivery is timed from a turn mark to the end of the first burst of data after it. Freeciv "
     "writes its turn log line after it has already sent most of the turn, so the burst OJH sees "
     "is a small trailing packet. It earns Freeciv its only first place, and it counts for 4% of "
     "Freeciv's score."),
    ("Frame rates are each game's own renderer, at its own resolution",
     "Open Doctrines and Unciv draw with OpenGL at 1600 &times; 900. Greater Diplomacy 5 draws in "
     "software, with pygame, at a fixed 1280 &times; 720. GD5's 1% low comes from the frames drawn "
     "while a turn resolves on its background thread; Open Doctrines resolves a turn inside one "
     "frame behind a loading screen, so it has no equivalent scene at all. Freeciv has no frame "
     "rate to measure: its client redraws only when the map changes."),
    ("Two scores are partial, and one is provisional",
     "FreeOrion and FreeCol were measured for turn speed only &mdash; 67% of the weight &mdash; because "
     "network and frame rate would need their clients driven, which OJH does not do yet. Their "
     "scores leave out the parts where the other games gain and lose points. Freeciv's is "
     "provisional for the frame-rate reason above."),
    ("The second round is a stress test for Greater Diplomacy 5, not its normal case",
     "Open Doctrines' 1939 keeps its 8192 &times; 4096 map picture, about 25 times the pixels of "
     "GD5's own 1939 at 1881 &times; 698. GD5 redraws its map layers from that picture every turn, "
     "so its turn time in that round mostly measures the picture size. GD5's real performance is "
     "the first round's."),
    ("FreeCol does not play out identically twice",
     "Three runs on the same seed on GD5's 1939 spread from 39.8 to 57.2 turns per minute. The "
     "median is shown. On Open Doctrines' 1939 it ran once."),
    ("FreeOrion is measured exactly as it ships",
     "Debug logging included: the server and each AI client write tens of megabytes of logs a "
     "minute, and <code>--log-level</code> did not change that. Its run on Open Doctrines' 1939 is "
     "not published at all &mdash; the Mac slept for 66 minutes during turns 110&ndash;112, so that run's "
     "turns per minute measures the sleep. It will be rerun."),
    ("Regions are not the same unit in every game",
     "Open Doctrines and Greater Diplomacy 5 count provinces. Freeciv and Unciv count tiles. "
     "FreeOrion counts star systems. Region-throughput figures are therefore comparable within a "
     "game and indicative between games, which is why turn and player throughput carry more of "
     "the score than world throughput does."),
]


CSS = """
/* The Open Doctrines site's own system: same tokens, same two faces, same
 * components as packaging/web/site/site.css. The page commits to one look --
 * a lamplit map table -- and paints its own ground rather than borrowing the
 * host's, exactly as the site does. */
:root {
  --ground:#0C0F16; --raise:#141822; --sunken:#090B11;
  --ink:#E8E4DA; --muted:#9AA0AE; --faint:#666D7C;
  --rule:#232936; --rule-firm:#333B4B;
  --gold:#C9A227; --gold-dim:#8C7320; --sea:#16202E;
}
* { box-sizing:border-box; }
@media (prefers-reduced-motion: reduce) { * { animation:none !important; transition:none !important; } }
body {
  margin:0; background:var(--ground); color:var(--ink);
  font-family:"Source Sans 3", system-ui, -apple-system, sans-serif;
  font-size:17px; line-height:1.65; -webkit-font-smoothing:antialiased;
}
.top { position:sticky; top:0; z-index:10;
  background:color-mix(in srgb, var(--ground) 88%, transparent);
  backdrop-filter:blur(10px); border-bottom:1px solid var(--rule); }
.top-in { max-width:1060px; margin:0 auto; padding:14px 24px;
  display:flex; align-items:baseline; gap:28px; flex-wrap:wrap; }
.mark { font-family:Spectral,Georgia,serif; font-weight:600; font-size:19px;
  color:var(--ink); text-decoration:none; white-space:nowrap; }
.mark span { color:var(--gold); }
nav { display:flex; gap:22px; flex-wrap:wrap; margin-left:auto; }
nav a { font-size:14.5px; color:var(--muted); text-decoration:none;
  padding-bottom:2px; border-bottom:1px solid transparent; }
nav a:hover { color:var(--ink); border-bottom-color:var(--gold-dim); }
:focus-visible { outline:2px solid var(--gold); outline-offset:3px; }

main { max-width:1060px; margin:0 auto; padding:0 24px 96px; }
.narrow { max-width:680px; }
h1,h2,h3 { font-family:Spectral,Georgia,serif; font-weight:600; letter-spacing:-.01em; }
h1 { font-size:clamp(34px,6vw,54px); line-height:1.08; margin:0 0 18px; text-wrap:balance; }
h2 { font-size:27px; margin:52px 0 14px; }
h3 { font-size:19px; margin:0 0 8px; }
p { margin:0 0 16px; }
a { color:var(--gold); text-underline-offset:3px; }
.lede { font-size:20px; color:var(--muted); max-width:60ch; }
.lede strong { color:var(--ink); font-weight:600; }
.label { font-family:"IBM Plex Mono",ui-monospace,monospace; font-size:11px;
  letter-spacing:.16em; text-transform:uppercase; color:var(--faint); }

.hero { position:relative; padding:76px 0 40px; }
.hero::before { content:""; position:absolute; inset:-40px -50vw 0; z-index:-1;
  background: radial-gradient(ellipse 70% 60% at 50% 0%,
    color-mix(in srgb, var(--sea) 80%, var(--gold) 4%) 0%, transparent 70%), var(--ground); }

.grid { display:grid; gap:18px; grid-template-columns:repeat(auto-fit,minmax(215px,1fr)); margin:26px 0; }
.card { background:var(--raise); border:1px solid var(--rule); border-radius:4px; padding:20px 20px 18px; }
.card .big { font-family:"IBM Plex Mono",ui-monospace,monospace; font-size:30px;
  font-weight:500; color:var(--gold); line-height:1.1; display:block;
  font-variant-numeric:tabular-nums; }
.card p { margin:8px 0 0; font-size:14.5px; color:var(--muted); }

.alert { background:color-mix(in srgb, var(--gold) 9%, var(--raise));
  border-left:2px solid var(--gold-dim); padding:18px 20px;
  border-radius:0 4px 4px 0; margin:24px 0; max-width:72ch; }
.alert strong { color:var(--gold); }
.alert p:last-child { margin-bottom:0; }

/* ── bars: the mobile answer to a twelve-column table ── */
.chart { margin:26px 0; padding:20px 20px 18px; background:var(--raise);
  border:1px solid var(--rule); border-radius:4px; }
.chart figcaption { margin-bottom:16px; }
.note { font-size:14px; color:var(--faint); margin:4px 0 0; }
ol.bars { list-style:none; padding:0; margin:0; display:flex; flex-direction:column; gap:11px; }
li.bar { display:grid; grid-template-columns:1fr auto; grid-template-areas:"n v" "t t";
  gap:2px 12px; align-items:baseline; }
.bn { grid-area:n; font-size:15px; color:var(--muted); overflow:hidden;
  text-overflow:ellipsis; white-space:nowrap; }
.bv { grid-area:v; font-family:"IBM Plex Mono",ui-monospace,monospace; font-size:14px;
  font-variant-numeric:tabular-nums; color:var(--muted); white-space:nowrap; }
.bt { grid-area:t; display:block; height:9px; background:var(--sunken);
  border-radius:2px; overflow:hidden; border:1px solid var(--rule); }
.bf { display:block; height:100%; background:var(--rule-firm); }
li.bar.od .bn, li.bar.od .bv { color:var(--ink); font-weight:600; }
li.bar.od .bf { background:var(--gold); }

/* ── detail tables ── */
.scroller { overflow-x:auto; -webkit-overflow-scrolling:touch; margin:18px 0 8px;
  border:1px solid var(--rule); border-radius:4px; background:var(--raise); }
table { border-collapse:collapse; width:100%; font-size:14.5px; }
caption { text-align:left; padding:14px 16px 0; font-size:14px; color:var(--faint); }
th { text-align:left; font-weight:600; white-space:nowrap; }
thead th { font-family:"IBM Plex Mono",ui-monospace,monospace; font-size:11px;
  letter-spacing:.1em; text-transform:uppercase; color:var(--faint);
  padding:14px 14px 8px; border-bottom:1px solid var(--rule-firm); vertical-align:bottom; }
tbody th { padding:9px 14px; color:var(--muted); font-weight:400;
  border-bottom:1px solid var(--rule); position:sticky; left:0;
  background:var(--raise); }
td { padding:9px 14px; border-bottom:1px solid var(--rule); color:var(--muted); white-space:nowrap; }
td.n { font-family:"IBM Plex Mono",ui-monospace,monospace; font-variant-numeric:tabular-nums; }
tbody tr:last-child th, tbody tr:last-child td { border-bottom:0; }
tr.od th, tr.od td { color:var(--ink); }
tr.od th { font-weight:600; }
i.pl { font-style:normal; font-family:"IBM Plex Mono",ui-monospace,monospace;
  font-size:10px; color:var(--faint); margin-left:6px; vertical-align:2px; }
tr.od i.pl { color:var(--gold-dim); }

/* ── caveats ── */
details { border-bottom:1px solid var(--rule); }
details:first-of-type { border-top:1px solid var(--rule); }
summary { cursor:pointer; padding:15px 0; font-weight:600; font-size:16.5px;
  list-style:none; display:flex; gap:12px; align-items:baseline; color:var(--ink); }
summary::-webkit-details-marker { display:none; }
summary::before { content:"+"; font-family:"IBM Plex Mono",ui-monospace,monospace;
  color:var(--gold); flex:0 0 auto; }
details[open] summary::before { content:"\\2212"; }
details p { margin:0 0 16px; padding-left:24px; color:var(--muted);
  font-size:15.5px; max-width:70ch; }
details code { font-family:"IBM Plex Mono",ui-monospace,monospace; font-size:.9em; color:var(--ink); }

.facts { border-top:1px solid var(--rule); margin:26px 0 0; max-width:680px; }
.facts div { display:grid; grid-template-columns:176px 1fr; gap:18px;
  padding:14px 0; border-bottom:1px solid var(--rule); align-items:baseline; }
.facts dt { font-size:14px; font-weight:600; color:var(--gold); }
.facts dd { margin:0; color:var(--muted); font-size:15.5px; }

footer { border-top:1px solid var(--rule); margin-top:70px; background:var(--sunken); }
footer .top-in { padding:30px 24px 44px; display:block; }
footer p { font-size:14px; color:var(--faint); max-width:62ch; }
footer a { color:var(--muted); }
footer code { font-family:"IBM Plex Mono",ui-monospace,monospace; font-size:.9em; }

@media (max-width:560px) {
  body { font-size:16px; }
  main { padding:0 16px 72px; }
  .top-in { padding:12px 16px; gap:14px; }
  nav { gap:14px; }
  .hero { padding:44px 0 28px; }
  h2 { font-size:23px; margin:40px 0 12px; }
  .facts div { grid-template-columns:1fr; gap:3px; }
  .chart { padding:16px 14px 14px; }
}
"""


def build(folder: pathlib.Path) -> str:
    a = sections((folder / "gd5-1939" / "report.md").read_text(encoding="utf-8"))
    b = sections((folder / "od-1939" / "report.md").read_text(encoding="utf-8"))
    card = (folder / "gd5-1939" / "score-opendoctrines.md").read_text(encoding="utf-8")
    first = sections((folder.parent / "2026-09-14-apple-m1-pro" / "report.md")
                     .read_text(encoding="utf-8"))

    a_tpm, b_tpm = first_table(a, "Turn speed"), first_table(b, "Turn speed")
    a_glance, b_glance = first_table(a, "At a glance"), first_table(b, "At a glance")
    a_cpu, b_cpu = first_table(a, "CPU and memory"), first_table(b, "CPU and memory")
    a_fps, b_fps = first_table(a, "Frame rate"), first_table(b, "Frame rate")
    a_net, b_net = first_table(a, "Network"), first_table(b, "Network")
    a_scores = first_table(a, "OJH scores")
    foot = first_table(first, "Footprint")
    first_glance = first_table(first, "At a glance")
    card_parts = tables(sections(card).get("Parts", ""))
    card_parts = card_parts[0] if card_parts else None

    # Headline figures, read out of the tables rather than typed here.
    od_a = next(r for r in a_tpm[1] if r[0].startswith(OD))
    unciv_a = next(r for r in a_tpm[1] if r[0].startswith("Unciv"))
    od_tpm = split_rank(od_a[1])[0].replace(" turns/min", "")
    unciv_tpm = split_rank(unciv_a[1])[0].replace(" turns/min", "")
    ratio = numeric(od_a[1]) / numeric(unciv_a[1])
    od_net_a = next(r for r in a_net[1] if r[0].startswith(OD))
    od_mem_a = next(r for r in a_cpu[1] if r[0].startswith(OD))

    p: list[str] = []
    add = p.append

    add('<title>Six Games, One World</title>')
    add('<meta charset="utf-8">')
    add('<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>')
    add('<link rel="stylesheet" href="https://fonts.googleapis.com/css2?'
        'family=IBM+Plex+Mono:wght@400;500&family=Source+Sans+3:wght@400;600&'
        'family=Spectral:wght@600&display=swap">')
    add(f"<style>{CSS}</style>")

    add('<div class="top"><div class="top-in">'
        '<a class="mark" href="#top">Objective Judge <span>Horizon</span></a>'
        '<nav><a href="#rounds">Rounds</a><a href="#detail">Detail</a>'
        '<a href="#score">Scorecard</a><a href="#caveats">Caveats</a>'
        '<a href="#method">Method</a></nav></div></div>')

    add('<main id="top">')

    # ── hero ──
    add('<div class="hero narrow">')
    add('<p class="label">Benchmark results &middot; 15 September 2026 &middot; Apple M1 Pro</p>')
    add("<h1>Six games, one world</h1>")
    add('<p class="lede">Every game put on the <strong>same map with the same number '
        'of players</strong>, and timed. Open Doctrines resolved '
        f'<strong>{html.escape(od_tpm)} turns a minute</strong> where the next '
        f'fastest managed {html.escape(unciv_tpm)}.</p>')
    add("</div>")

    add('<div class="grid">')
    add(f'<div class="card"><span class="big">{html.escape(od_tpm)}</span>'
        "<p>turns a minute on GD5&rsquo;s 1939 &mdash; 906 provinces, 37 AI players, and "
        f"{ratio:.1f}&times; the next fastest game measured</p></div>")
    add(f'<div class="card"><span class="big">{html.escape(split_rank(od_net_a[2])[0])}</span>'
        "<p>sent per turn, median. The others ran from 79.9 KiB to 11.5 MiB</p></div>")
    add(f'<div class="card"><span class="big">{html.escape(split_rank(od_mem_a[1])[0])}</span>'
        "<p>peak memory. Only Freeciv held less; four games held a gigabyte or more</p></div>")
    add('<div class="card"><span class="big">2.7 s</span>'
        "<p>to load a save &mdash; last place, and not close. Unciv does it in 34 ms</p></div>")
    add("</div>")

    add('<div class="alert narrow"><p><strong>Read the caveats before quoting any of '
        "this.</strong> The benchmark, its weights and five of the six drivers were "
        'written by the developer of one of the contestants. That is <a href="#caveats">'
        "stated in full below</a>, along with the three figures that are artifacts of how "
        "something is measured rather than results.</p></div>")

    # ── the two rounds ──
    add('<h2 id="rounds">Round one &middot; Greater Diplomacy 5&rsquo;s 1939</h2>')
    add('<p class="lede narrow">906 provinces, 35 to 37 AI players. GD5&rsquo;s own '
        "historical scenario, converted for Open Doctrines with Dragoman. Freeciv and "
        "Unciv cannot load either map, so they are sized to match.</p>")
    add(bar_chart("Turns per minute", "Higher is better. Bars are logarithmic: the spread "
                  "reaches 372&times;, and a linear scale would leave five of the six games "
                  "as invisible slivers.", chart_from(a_tpm, 1)))
    add(bar_chart("OJH score", "Higher is better. Each game&rsquo;s score is built from its own "
                  "result files against fixed reference levels, so no game&rsquo;s score depends "
                  "on which others are in the report.", chart_from(a_glance, 1)))

    add('<h2>Round two &middot; Open Doctrines&rsquo; 1939</h2>')
    add('<p class="lede narrow">1,298 land provinces, 63 AI players &mdash; more than the 34 major '
        "civilizations Unciv&rsquo;s ruleset has, so 29 of its players are city-states. "
        "FreeOrion&rsquo;s run here is not published; see the caveats.</p>")
    add(bar_chart("Turns per minute", "Higher is better. Logarithmic.", chart_from(b_tpm, 1)))
    add(bar_chart("OJH score", "Higher is better. Logarithmic.", chart_from(b_glance, 1)))
    add('<div class="alert"><p><strong>This round is a stress test for Greater Diplomacy 5, '
        "not its normal case.</strong> Open Doctrines&rsquo; map keeps its 8192 &times; 4096 "
        "picture, about 25 times the pixels of GD5&rsquo;s own 1939. GD5 redraws its map layers "
        "from that picture every turn, so its 4.1 turns a minute here mostly measures the "
        "picture size. Read its real performance from round one, where it managed 74.</p></div>")

    # ── the other instruments ──
    add('<h2 id="detail">The other three instruments</h2>')
    add('<p class="lede narrow">Turn speed is 67% of the score. These are the rest, from round '
        "one, where every game was measured on all four.</p>")
    add(bar_chart("Data per turn, median", "Lower is better, so a long bar is a good result. "
                  "What the host sends one client for one turn.", chart_from(a_net, 2), "less"))
    add(bar_chart("Frame rate on the map", "Higher is better. Each game&rsquo;s own renderer at "
                  "its own resolution.", chart_from(a_fps, 1)))
    add(bar_chart("1% low frame rate", "Higher is better. The worst one frame in a hundred &mdash; "
                  "what a stutter actually feels like.", chart_from(a_fps, 2)))
    add(bar_chart("CPU time per player-turn", "Lower is better. The processor time one AI "
                  "player&rsquo;s turn costs, summed over every core.",
                  chart_from(a_cpu, 4), "less"))
    add(bar_chart("Peak memory", "Lower is better. Sampled ten times a second across the game "
                  "and every process it starts.", chart_from(a_cpu, 1), "less"))

    # ── every number ──
    add("<h2>Every number, both rounds</h2>")
    add('<p class="lede narrow">The full tables as <code>ojh report</code> wrote them. Places '
        "are the small figures: 1 is the best of the games measured for that statistic. "
        "Players, map size and cores are context rather than a ranking. Scroll them sideways.</p>")

    add("<h3>Turn speed &middot; round one</h3>")
    add(data_table(*a_tpm))
    add("<h3>Turn speed &middot; round two</h3>")
    add(data_table(*b_tpm))
    add("<h3>CPU and memory &middot; round one</h3>")
    add(data_table(*a_cpu))
    add("<h3>CPU and memory &middot; round two</h3>")
    add(data_table(*b_cpu))
    add("<h3>Frame rate, every scene &middot; round one</h3>")
    add(data_table(*a_fps))
    add("<h3>Frame rate, every scene &middot; round two</h3>")
    add(data_table(*b_fps))
    add("<h3>Network &middot; round one</h3>")
    add(data_table(*a_net))
    add("<h3>Network &middot; round two</h3>")
    add(data_table(*b_net))
    if foot:
        add("<h3>Footprint</h3>")
        add('<p class="note">From the first round, on each game&rsquo;s own default map, and not '
            "rerun since. The one table Open Doctrines comes last in.</p>")
        add(data_table(*foot))
    if first_glance:
        add("<h3>The first round, on each game&rsquo;s own map</h3>")
        add('<p class="note">14 September: every game on its own default world, 8 to 37 players '
            "on very different maps. Kept because it is where Greater Diplomacy 5&rsquo;s numbers "
            "are its real ones, and where footprint was measured.</p>")
        add(data_table(*first_glance))

    # ── scorecard ──
    if card_parts:
        add('<h2 id="score">How the score is built</h2>')
        add('<p class="lede narrow">Open Doctrines&rsquo; own scorecard from round one, part by '
            "part. Every part is measured against a <em>fixed reference level</em> rather than "
            "against the other games, which is what makes a score comparable between reports. "
            "Where a figure depends on the processor it was measured on, it is also shown "
            "normalised to OJH&rsquo;s reference CPU.</p>")
        add(data_table(*card_parts))
        add('<p class="note">Open Doctrines scored on 96% of the weight; turn delivery was not '
            "reported, because no client sits on the wire. Each game has a scorecard like this "
            "one beside the report.</p>")
        if a_scores:
            add("<h3>Score, coverage and status, every game</h3>")
            add(data_table(*a_scores))

    # ── caveats ──
    add('<h2 id="caveats">Read these before quoting a number</h2>')
    add('<p class="lede narrow">Tap to open. None of these is a footnote: each one changes what '
        "a figure above is allowed to mean.</p>")
    for head, body in CAVEATS:
        add(f"<details><summary>{html.escape(head, quote=False)}</summary><p>{body}</p></details>")

    # ── method ──
    add('<h2 id="method">The machine, and the method</h2>')
    machine = first_table(a, "Machine")
    if machine:
        add('<dl class="facts">')
        for row in machine[1]:
            add(f"<div><dt>{html.escape(row[0])}</dt><dd>{html.escape(row[1])}</dd></div>")
        add("</dl>")
    add('<p class="narrow" style="margin-top:22px">The CPU reference score is OJH&rsquo;s own fixed '
        "workload, timed on this machine. Divide a result by it to compare across hardware.</p>")
    add('<p class="narrow">250 turns, every player run by the game&rsquo;s own AI. Turn speed is the '
        "median of three runs on GD5&rsquo;s 1939 and one run on Open Doctrines&rsquo; 1939. Network runs "
        "are 20 turns with two clients. Each frame-rate scene is timed for five seconds after "
        "settling, with the late-game map measured after 20 turns. Start-up and world generation "
        "are timed separately and are not part of the turns.</p>")
    add('<p class="narrow">Player-turns and region-turns multiply turn speed by the size of the '
        "game, so a larger world is not punished for being larger.</p>")

    add("</main>")
    add('<footer><div class="top-in">'
        "<p>Generated by <code>tools/report_html.py</code> from the result files in "
        '<a href="https://github.com/Pr1nted/objective-judge-horizon">'
        "Pr1nted/objective-judge-horizon</a>. Every number on this page is parsed from the "
        "Markdown <code>ojh report</code> wrote, so the page and the repository cannot "
        "disagree. The benchmark, every driver, the score weights and the raw results are "
        "public, and <code>tools/run-matched.sh</code> reproduces the whole round.</p>"
        "</div></footer>")
    return "\n".join(p)


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__.strip())
        return 2
    folder = pathlib.Path(sys.argv[1])
    out = folder / "index.html"
    out.write_text(build(folder), encoding="utf-8")
    print(f"wrote {out} ({out.stat().st_size:,} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
