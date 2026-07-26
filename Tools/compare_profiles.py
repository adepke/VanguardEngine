#!/usr/bin/env python3
"""
compare_profiles.py -- per-pass speed gate for the auto performance loop.

Consumes the CSVs produced by `tracy-csvexport --gpu profile.tracy` (per render
pass, keyed by the render-graph pass stableName) and reports how each pass moved
between a baseline and a candidate build. It then gates the change:

  * the pass you targeted must actually get faster, and
  * the total GPU frame time must not regress beyond a small noise tolerance
    (guards against speeding up one pass while silently slowing another).

To beat run-to-run jitter, pass several CSVs per side; the per-zone metric is the
MEDIAN across the runs.

This build's `tracy-csvexport --gpu` emits RAW per-event rows (name, src_file,
Time from start of program, GPU execution time). Aggregation is done by the shared
tracy_gpu.aggregate_gpu_csv() -- do NOT assume pre-aggregated columns here.

The total gate is on PER-FRAME GPU time: the sum of each steady-state pass's mean
(one-shot startup passes, counts==1, are excluded so they can't dilute the number).

Two modes:
  * File mode:  compare CSV list vs CSV list (one scene).
  * Dir mode:   --baseline-dir runs/baseline --candidate-dir runs/cand
                matches scenes by stem and gates on the WORST scene: worst total
                regression, and the smallest improvement of each --require-speedup
                pass across the scenes that contain it. Use this in the loop.

Usage:
    python3 compare_profiles.py --baseline-dir runs/baseline --candidate-dir runs/cand \\
                                --require-speedup "Clouds Pass"
    python3 compare_profiles.py --baseline b1.csv [...] --candidate c1.csv [...] \\
                                [--require-speedup NAME] [--metric mean_ns] [--tol-total 1.0]

Options:
    --metric        mean_ns | total_ns    per-pass number to compare (default mean_ns)
    --require-speedup NAME    pass (stableName) that must improve; repeatable
    --min-speedup   FLOAT     min % improvement required on each target (default 0.0)
    --tol-total     FLOAT     allowed % per-frame GPU regression before failing (default 1.0)
    --top           INT       rows to print per scene, by candidate cost (default 15)
    --json          PATH      write full comparison as JSON

Exit codes:
    0  PASS   1  FAIL (gate not met)   2  ERROR (bad input)
"""
import argparse
import json
import statistics
import sys
from pathlib import Path

# Share the aggregator with rank_passes.py so the two can never diverge.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from tracy_gpu import aggregate_gpu_csv, per_frame_total_ns  # noqa: E402
from run_meta import match_scenes  # noqa: E402


def median_side(paths, metric):
    """Median of `metric` per zone across CSVs, plus median PER-FRAME GPU total."""
    per_zone = {}
    totals = []
    for p in paths:
        table = aggregate_gpu_csv(p)
        totals.append(per_frame_total_ns(table))  # steady-state sum(mean_ns)
        for name, rec in table.items():
            if metric in rec:
                per_zone.setdefault(name, []).append(rec[metric])
    zone_median = {n: statistics.median(vals) for n, vals in per_zone.items() if vals}
    total_median = statistics.median(totals) if totals else 0.0
    return zone_median, total_median


def pct(old, new):
    if old == 0:
        return 0.0 if new == 0 else float("inf")
    return (new - old) / old * 100.0


def fmt_ns(ns):
    if ns >= 1_000_000:
        return f"{ns/1e6:.3f}ms"
    if ns >= 1_000:
        return f"{ns/1e3:.2f}us"
    return f"{ns:.0f}ns"


def evaluate(base_paths, cand_paths, metric, require):
    """Compare one scene. Returns a dict of numbers (no gating).

    require -> {target: improvement_pct or None (absent in one/both sides)}.
    improvement is positive when the candidate is faster.
    """
    base_zone, base_total = median_side(base_paths, metric)
    cand_zone, cand_total = median_side(cand_paths, metric)

    names = sorted(set(base_zone) | set(cand_zone))
    rows = []
    for n in names:
        ov = base_zone.get(n)
        nv = cand_zone.get(n)
        rows.append({
            "name": n, "baseline": ov, "candidate": nv,
            "delta_ns": (None if ov is None or nv is None else nv - ov),
            "delta_pct": (None if ov is None or nv is None else pct(ov, nv)),
        })
    rows.sort(key=lambda r: (r["candidate"] or 0.0), reverse=True)

    improvements = {}
    for target in require:
        if target in base_zone and target in cand_zone:
            improvements[target] = -pct(base_zone[target], cand_zone[target])
        else:
            improvements[target] = None

    return {
        "rows": rows,
        "base_total": base_total,
        "cand_total": cand_total,
        "total_pct": pct(base_total, cand_total),
        "improvements": improvements,
    }


def print_table(res, metric, top, header=None):
    if header:
        print(header)
    print(f"{'pass (stableName)':<34}{'baseline':>12}{'candidate':>12}{'delta':>12}{'delta%':>9}")
    print("-" * 79)
    for r in res["rows"][:top]:
        b = fmt_ns(r["baseline"]) if r["baseline"] is not None else "--"
        c = fmt_ns(r["candidate"]) if r["candidate"] is not None else "--"
        if r["delta_pct"] is None:
            d, dp = "--", "--"
        else:
            d = ("+" if r["delta_ns"] >= 0 else "-") + fmt_ns(abs(r["delta_ns"]))
            dp = f"{r['delta_pct']:+.1f}%"
        print(f"{r['name'][:33]:<34}{b:>12}{c:>12}{d:>12}{dp:>9}")
    bt, ct, tp = res["base_total"], res["cand_total"], res["total_pct"]
    print("-" * 79)
    print(
        f"{'PER-FRAME GPU (steady mean_ns)':<34}{fmt_ns(bt):>12}{fmt_ns(ct):>12}"
        f"{('+' if ct>=bt else '-')+fmt_ns(abs(ct-bt)):>12}{tp:>+8.1f}%"
    )


def gate(worst_total_pct, target_worst_improvement, args):
    """Apply the gate to worst-case numbers. Returns (passed, reasons)."""
    passed, reasons = True, []
    if worst_total_pct > args.tol_total:
        passed = False
        reasons.append(
            f"per-frame GPU regressed {worst_total_pct:+.1f}% (> {args.tol_total}% tol)"
        )
    for target, imp in target_worst_improvement.items():
        if imp is None:
            passed = False
            reasons.append(f"required pass '{target}' not present in any matched scene")
        elif imp < args.min_speedup:
            passed = False
            reasons.append(
                f"pass '{target}' improved only {imp:+.1f}% in its worst scene "
                f"(need >= {args.min_speedup}%)"
            )
    return passed, reasons


def main(argv=None):
    p = argparse.ArgumentParser(description="Per-pass GPU speed gate (file or dir mode).")
    p.add_argument("--baseline", nargs="+", help="baseline CSV(s) (file mode)")
    p.add_argument("--candidate", nargs="+", help="candidate CSV(s) (file mode)")
    p.add_argument("--baseline-dir", help="runs/<tag> baseline dir (dir mode)")
    p.add_argument("--candidate-dir", help="runs/<tag> candidate dir (dir mode)")
    p.add_argument("--metric", choices=("mean_ns", "total_ns"), default="mean_ns")
    p.add_argument("--require-speedup", action="append", default=[], dest="require")
    p.add_argument("--min-speedup", type=float, default=0.0)
    p.add_argument("--tol-total", type=float, default=1.0)
    p.add_argument("--top", type=int, default=15)
    p.add_argument("--json", dest="json_out", default=None)
    args = p.parse_args(argv)

    # ---- Dir mode: per-scene, gate on the worst ----
    if args.baseline_dir or args.candidate_dir:
        if not (args.baseline_dir and args.candidate_dir):
            sys.stderr.write("ERROR: dir mode needs both --baseline-dir and --candidate-dir\n")
            return 2
        try:
            matched, miss_cand, miss_base = match_scenes(args.baseline_dir, args.candidate_dir)
        except (OSError, ValueError) as e:
            sys.stderr.write(f"ERROR: {e}\n")
            return 2
        if miss_cand or miss_base:
            sys.stderr.write(
                f"ERROR: scene sets differ; missing in candidate={miss_cand}, "
                f"missing in baseline={miss_base}.\n"
            )
            return 2
        if not matched:
            sys.stderr.write("ERROR: no scenes matched between the two tags.\n")
            return 2

        per_scene = {}
        worst_total_pct = float("-inf")
        worst_total_scene = None
        target_worst = {t: None for t in args.require}   # min improvement across scenes
        target_worst_scene = {t: None for t in args.require}
        for stem, be, ce in matched:
            try:
                res = evaluate(be["gpu_csv_paths"], ce["gpu_csv_paths"], args.metric, args.require)
            except (OSError, ValueError) as e:
                sys.stderr.write(f"ERROR ({stem}): {e}\n")
                return 2
            per_scene[stem] = res
            print_table(res, args.metric, args.top, header=f"\n=== scene: {stem} ===")

            if res["total_pct"] > worst_total_pct:
                worst_total_pct, worst_total_scene = res["total_pct"], stem
            for t, imp in res["improvements"].items():
                if imp is None:
                    continue
                if target_worst[t] is None or imp < target_worst[t]:
                    target_worst[t], target_worst_scene[t] = imp, stem

        passed, reasons = gate(worst_total_pct, target_worst, args)

        print("\n" + "=" * 79)
        print(f"WORST-CASE across {len(matched)} scene(s):")
        print(f"  per-frame GPU  {worst_total_pct:+.1f}%   (worst scene: {worst_total_scene})")
        for t in args.require:
            imp, sc = target_worst[t], target_worst_scene[t]
            print(f"  {t}: " + (f"{imp:+.1f}% (worst scene: {sc})" if imp is not None
                                else "NOT PRESENT in any matched scene"))
        print(f"\n[{'PASS' if passed else 'FAIL'}] metric={args.metric}")
        for why in reasons:
            print(f"       - {why}", file=sys.stderr)

        if args.json_out:
            with open(args.json_out, "w", encoding="utf-8") as fh:
                json.dump({
                    "mode": "dir", "metric": args.metric, "pass": passed,
                    "worst_total_pct": worst_total_pct, "worst_total_scene": worst_total_scene,
                    "target_worst_improvement": target_worst,
                    "reasons": reasons,
                    "scenes": {s: {"total_pct": r["total_pct"],
                                   "improvements": r["improvements"]}
                               for s, r in per_scene.items()},
                }, fh, indent=2)
        return 0 if passed else 1

    # ---- File mode ----
    if not (args.baseline and args.candidate):
        sys.stderr.write("ERROR: file mode needs --baseline and --candidate CSVs (or use dir mode)\n")
        return 2
    try:
        res = evaluate(args.baseline, args.candidate, args.metric, args.require)
    except (OSError, ValueError) as e:
        sys.stderr.write(f"ERROR: {e}\n")
        return 2
    if not res["rows"]:
        sys.stderr.write("ERROR: no GPU zones parsed.\n")
        return 2

    print_table(res, args.metric, args.top)
    passed, reasons = gate(res["total_pct"], res["improvements"], args)
    print(f"\n[{'PASS' if passed else 'FAIL'}] metric={args.metric}  total {res['total_pct']:+.1f}%",
          end="")
    if res["improvements"]:
        shown = {k: v for k, v in res["improvements"].items() if v is not None}
        if shown:
            print("  " + "  ".join(f"{k}: {v:+.1f}%" for k, v in shown.items()), end="")
    print()
    for why in reasons:
        print(f"       - {why}", file=sys.stderr)

    if args.json_out:
        with open(args.json_out, "w", encoding="utf-8") as fh:
            json.dump({
                "mode": "file", "metric": args.metric, "pass": passed,
                "total_delta_pct": res["total_pct"], "improvements": res["improvements"],
                "reasons": reasons, "zones": res["rows"],
            }, fh, indent=2)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
