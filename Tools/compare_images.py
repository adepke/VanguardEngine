#!/usr/bin/env python3
"""
compare_images.py -- visual-regression gate for the auto performance loop.

Compares a candidate render against a baseline render and exits non-zero if they
differ beyond tolerance. This is the hard safety rail for "no impact to visual
fidelity": an optimization is only accepted if the image is (near) identical.

The engine is deterministic: the same build with the same args renders the exact
same image every time (fixed timestep, no wall-clock input). A truly fidelity-neutral
edit should render byte-for-byte identical (--exact). In practice, restructuring
shader source (even with no value-changing edit -- e.g. removing a provably-dead
branch) can shift DXC's FMA-contraction/instruction-scheduling choices enough to
flip the LSB of a handful of pixels' 8-bit output, without any real visual or
algorithmic change. --exact cannot tell that apart from a real regression.

--near-exact is the recommended gate for the perf loop: it requires max_abs <= 1.0
(no channel of any pixel may differ by more than one 8-bit step) AND changed_pct
== 0.0 (using the default --eps=2.0, so literally nothing crosses even that small
threshold). This passes single-ULP-class compiler noise while still being far
tighter than the general tolerance options below (PSNR/mean/changed), which remain
for the rare case of comparing across intentionally different builds/args.

Two modes:
  * File mode:  compare two PNGs directly.
  * Dir mode:   --baseline-dir runs/baseline --candidate-dir runs/cand
                matches scenes by stem (from meta.json) and gates on the WORST
                scene. Fails if ANY scene differs. Use this in the loop.

Usage:
    python3 compare_images.py --baseline-dir runs/baseline --candidate-dir runs/cand --near-exact
    python3 compare_images.py BASELINE.png CANDIDATE.png --near-exact
    python3 compare_images.py BASELINE.png CANDIDATE.png [tolerance options]

Options:
    --baseline-dir/--candidate-dir  runs/<tag> dirs; compare every matched scene.
    --near-exact      Recommended gate. max_abs <= 1.0 (one 8-bit step) AND
                      changed_pct == 0.0 (nothing crosses even --eps). Passes
                      single-ULP compiler noise, rejects real regressions.
    --exact           Require byte-for-byte identical pixels (max_abs == 0).
                      Stricter than --near-exact; --exact wins if both given.
    --psnr     FLOAT  Minimum acceptable PSNR in dB           (default 45.0)
    --mean     FLOAT  Max mean absolute per-channel delta,     (default 0.5)
                      on a 0-255 scale
    --changed  FLOAT  Max percent of pixels that may change    (default 0.5)
                      by more than --eps
    --eps      FLOAT  Per-channel delta (0-255) above which a  (default 2.0)
                      pixel counts as "changed"
    --out      PATH   Write an amplified diff heatmap PNG here (optional)
    --json     PATH   Write the metrics as JSON here           (optional)
    --amplify  FLOAT  Heatmap amplification factor             (default 8.0)

Exit codes:
    0  PASS  (within all tolerances)
    1  FAIL  (a visual regression exceeded tolerance)
    2  ERROR (bad input: missing file, size mismatch, etc.)
"""
import argparse
import json
import sys
from pathlib import Path

try:
    import numpy as np
    from PIL import Image
except ImportError as e:  # pragma: no cover
    sys.stderr.write(
        "ERROR: compare_images.py needs numpy and Pillow.\n"
        "       pip install numpy Pillow  (add --break-system-packages if needed)\n"
        f"       ({e})\n"
    )
    sys.exit(2)

sys.path.insert(0, str(Path(__file__).resolve().parent))
from run_meta import match_scenes  # noqa: E402


def load_rgb(path):
    img = Image.open(path).convert("RGB")
    return np.asarray(img, dtype=np.float64)


def evaluate_pair(baseline_png, candidate_png, args, diff_out=None):
    """Compare two PNGs. Returns (metrics, passed) or raises on bad input."""
    a = load_rgb(baseline_png)
    b = load_rgb(candidate_png)
    if a.shape != b.shape:
        raise ValueError(
            f"dimension/channel mismatch: {baseline_png} {a.shape} vs "
            f"{candidate_png} {b.shape}"
        )

    diff = np.abs(a - b)
    max_abs = float(diff.max())
    mean_abs = float(diff.mean())
    mse = float((diff ** 2).mean())
    psnr = float("inf") if mse == 0.0 else 20.0 * np.log10(255.0) - 10.0 * np.log10(mse)

    per_pixel_max = diff.max(axis=2)          # a pixel "changed" if ANY channel > eps
    changed_pct = float((per_pixel_max > args.eps).mean()) * 100.0

    metrics = {
        "baseline": str(baseline_png),
        "candidate": str(candidate_png),
        "width": int(a.shape[1]),
        "height": int(a.shape[0]),
        "max_abs": round(max_abs, 4),
        "mean_abs": round(mean_abs, 6),
        "psnr_db": (None if psnr == float("inf") else round(psnr, 3)),
        "changed_pct": round(changed_pct, 5),
    }

    if args.exact:
        passed = (max_abs == 0.0)
        metrics["mode"] = "exact"
        metrics["checks"] = {"exact": passed}
    elif args.near_exact:
        ok_max_abs = (max_abs <= 1.0)
        ok_changed = (changed_pct == 0.0)
        passed = ok_max_abs and ok_changed
        metrics["mode"] = "near_exact"
        metrics["checks"] = {"max_abs": ok_max_abs, "changed": ok_changed}
    else:
        ok_psnr = (psnr >= args.psnr)
        ok_mean = (mean_abs <= args.mean)
        ok_changed = (changed_pct <= args.changed)
        passed = ok_psnr and ok_mean and ok_changed
        metrics["mode"] = "tolerance"
        metrics["checks"] = {"psnr": ok_psnr, "mean": ok_mean, "changed": ok_changed}
    metrics["pass"] = bool(passed)

    if diff_out:
        try:
            heat = np.clip(diff * args.amplify, 0, 255).astype(np.uint8)
            Image.fromarray(heat, mode="RGB").save(diff_out)
            metrics["diff_image"] = str(diff_out)
        except Exception as e:  # noqa: BLE001
            sys.stderr.write(f"WARNING: could not write diff image: {e}\n")

    return metrics, passed


def format_line(tag, m):
    psnr_str = "inf" if m["psnr_db"] is None else f"{m['psnr_db']:.2f}"
    verdict = "PASS" if m["pass"] else "FAIL"
    if m["mode"] == "exact":
        return (f"[{verdict}] {tag}exact: max_abs={m['max_abs']:.1f} (need 0)  "
                f"changed={m['changed_pct']:.4f}%  PSNR={psnr_str}dB")
    if m["mode"] == "near_exact":
        return (f"[{verdict}] {tag}near-exact: max_abs={m['max_abs']:.1f} (need <=1)  "
                f"changed={m['changed_pct']:.4f}% (need 0)  PSNR={psnr_str}dB")
    return (f"[{verdict}] {tag}PSNR={psnr_str}dB  mean_abs={m['mean_abs']:.4f}  "
            f"changed={m['changed_pct']:.4f}%  max_abs={m['max_abs']:.1f}")


def main(argv=None):
    p = argparse.ArgumentParser(description="Visual-regression gate (file or dir mode).")
    p.add_argument("baseline", nargs="?", help="baseline PNG (file mode)")
    p.add_argument("candidate", nargs="?", help="candidate PNG (file mode)")
    p.add_argument("--baseline-dir", help="runs/<tag> baseline dir (dir mode)")
    p.add_argument("--candidate-dir", help="runs/<tag> candidate dir (dir mode)")
    p.add_argument("--exact", action="store_true",
                   help="require byte-for-byte identical pixels (max_abs == 0)")
    p.add_argument("--near-exact", dest="near_exact", action="store_true",
                   help="recommended perf-loop gate: max_abs <= 1.0 and changed_pct == 0.0")
    p.add_argument("--psnr", type=float, default=45.0)
    p.add_argument("--mean", type=float, default=0.5)
    p.add_argument("--changed", type=float, default=0.5)
    p.add_argument("--eps", type=float, default=2.0)
    p.add_argument("--out", default=None, help="diff heatmap (file mode) / dir for per-scene diffs")
    p.add_argument("--json", dest="json_out", default=None)
    p.add_argument("--amplify", type=float, default=8.0)
    args = p.parse_args(argv)

    # ---- Dir mode: match scenes, gate on the worst ----
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
                f"missing in baseline={miss_base}. Capture the same scenes for both.\n"
            )
            return 2
        if not matched:
            sys.stderr.write("ERROR: no scenes matched between the two tags.\n")
            return 2

        all_pass = True
        results = []
        for stem, be, ce in matched:
            diff_out = None
            if args.out:
                Path(args.out).mkdir(parents=True, exist_ok=True)
                diff_out = str(Path(args.out) / f"{stem}_diff.png")
            try:
                m, passed = evaluate_pair(be["png_path"], ce["png_path"], args, diff_out)
            except Exception as e:  # noqa: BLE001
                sys.stderr.write(f"ERROR ({stem}): {e}\n")
                return 2
            results.append((stem, m))
            all_pass &= passed
            print(format_line(f"{stem}: ", m))

        # Worst scene = highest max_abs (exact) / lowest PSNR (tolerance).
        worst = max(results, key=lambda r: r[1]["max_abs"])[0]
        print(f"\n[{'PASS' if all_pass else 'FAIL'}] {len(matched)} scene(s); "
              f"worst = {worst}")
        if args.json_out:
            with open(args.json_out, "w", encoding="utf-8") as fh:
                json.dump({"pass": all_pass, "worst": worst,
                           "scenes": {s: m for s, m in results}}, fh, indent=2)
        return 0 if all_pass else 1

    # ---- File mode ----
    if not (args.baseline and args.candidate):
        sys.stderr.write("ERROR: file mode needs BASELINE and CANDIDATE (or use dir mode)\n")
        return 2
    try:
        metrics, passed = evaluate_pair(args.baseline, args.candidate, args, args.out)
    except FileNotFoundError as e:
        sys.stderr.write(f"ERROR: {e}\n")
        return 2
    except ValueError as e:
        sys.stderr.write(f"ERROR: {e}\n")
        return 2
    except Exception as e:  # noqa: BLE001
        sys.stderr.write(f"ERROR: failed to load images: {e}\n")
        return 2

    if args.json_out:
        with open(args.json_out, "w", encoding="utf-8") as fh:
            json.dump(metrics, fh, indent=2)
    print(format_line("", metrics))
    if not passed:
        failed = [k for k, v in metrics["checks"].items() if not v]
        print(f"       regression on: {', '.join(failed)}", file=sys.stderr)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
