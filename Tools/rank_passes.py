#!/usr/bin/env python3
"""
rank_passes.py -- rank render passes by GPU cost from a tracy-csvexport --gpu CSV.

Step 1 of the loop. Aggregates the raw per-event CSV (via tracy_gpu) and prints a
ranked table so the driver doesn't hand-write aggregation each session.

Crucially it separates:
  * STEADY-STATE passes (run every frame, counts >= 2) -- the real optimization
    targets, ranked by mean per-frame GPU time.
  * ONE-SHOT / startup passes (counts == 1, e.g. "Clouds Noise Pass",
    "Atmosphere Precompute Pass") -- these dominate raw total_ns but are startup
    cost, NOT per-frame cost. Listed separately and excluded from the ranking so
    you don't waste an iteration optimizing a pass that runs once at load.

Usage:
    python3 rank_passes.py runs/baseline/gpu_zones.csv [--metric mean_ns|total_ns]
                           [--top N] [--all] [--json OUT]

    --metric   rank key for steady-state passes (default mean_ns = per-frame cost)
    --top      rows to show (default 20)
    --all      also rank one-shot passes together with steady-state
    --json     write the full aggregation to OUT
"""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from tracy_gpu import aggregate_gpu_csv, steady_state, one_shot, per_frame_total_ns  # noqa: E402


def fmt_ns(ns):
    if ns >= 1_000_000:
        return f"{ns/1e6:.3f}ms"
    if ns >= 1_000:
        return f"{ns/1e3:.2f}us"
    return f"{ns:.0f}ns"


def print_table(title, items, metric):
    print(f"\n{title}")
    print(f"{'#':>3}  {'pass (stableName)':<38}{'mean':>11}{'total':>12}{'count':>7}")
    print("-" * 72)
    ranked = sorted(items.items(), key=lambda kv: kv[1].get(metric, 0.0), reverse=True)
    for i, (name, rec) in enumerate(ranked, 1):
        print(
            f"{i:>3}  {name[:37]:<38}"
            f"{fmt_ns(rec.get('mean_ns', 0)):>11}"
            f"{fmt_ns(rec.get('total_ns', 0)):>12}"
            f"{int(rec.get('counts', 0)):>7}"
        )
    return ranked


def main(argv=None):
    p = argparse.ArgumentParser(description="Rank render passes by GPU cost.")
    p.add_argument("csv", help="a tracy-csvexport --gpu file (e.g. runs/baseline/gpu_zones.csv)")
    p.add_argument("--metric", choices=("mean_ns", "total_ns"), default="mean_ns")
    p.add_argument("--top", type=int, default=20)
    p.add_argument("--all", action="store_true", help="rank one-shot passes together with steady-state")
    p.add_argument("--json", dest="json_out", default=None)
    args = p.parse_args(argv)

    try:
        table = aggregate_gpu_csv(args.csv)
    except (OSError, ValueError) as e:
        sys.stderr.write(f"ERROR: {e}\n")
        return 2
    if not table:
        sys.stderr.write("ERROR: no GPU zones parsed.\n")
        return 2

    steady = steady_state(table)
    once = one_shot(table)
    frame_total = per_frame_total_ns(table)

    if args.all:
        ranked = print_table(
            f"ALL PASSES (ranked by {args.metric})", table, args.metric
        )[: args.top]
    else:
        ranked = print_table(
            f"STEADY-STATE PASSES -- optimization targets (ranked by {args.metric})",
            steady, args.metric,
        )[: args.top]
        if once:
            print_table(
                "ONE-SHOT / STARTUP PASSES -- excluded from targets (counts==1)",
                once, "total_ns",
            )

    print("-" * 72)
    print(f"per-frame GPU (sum of steady-state means): {fmt_ns(frame_total)}"
          f"   across {len(steady)} steady + {len(once)} one-shot passes")
    if not args.all and ranked:
        top_name = ranked[0][0]
        print(f"next target -> \"{top_name}\"  ({fmt_ns(ranked[0][1].get(args.metric,0))} {args.metric})")

    if args.json_out:
        with open(args.json_out, "w", encoding="utf-8") as fh:
            json.dump({"per_frame_total_ns": frame_total, "passes": table}, fh, indent=2)

    return 0


if __name__ == "__main__":
    sys.exit(main())
