#!/usr/bin/env python3
"""
profile_headless.py -- profiled, deterministic headless run(s), one per scene.

Coordinates a Vanguard headless render + Tracy capture for EACH --scene, then
exports per-render-pass GPU timings to CSV. Output for tag T lands in runs/<T>/,
nested per scene (stem = scene filename without extension):
    <stem>/frame.png        the captured render (the visual-fidelity reference)
    <stem>/profile.tracy    the raw Tracy trace
    <stem>/gpu_zones.csv    per-pass GPU stats  (tracy-csvexport --gpu)
    <stem>/cpu_zones.csv    per-scope CPU stats (tracy-csvexport)
    meta.json               git sha, scenes, frames, cvars -> reproducibility
(--repeat N nests each capture under <stem>/run{i}/ and surfaces run0 at <stem>/.)

The compare scripts take runs/<tag> directories and match scenes by stem, gating
on the WORST scene. So capture the same scene set for baseline and candidate.

How the short-run drain is handled (see notes in AutoPerformanceLoopPlan.md):
  * TRACY_NO_EXIT=1 is exported so the client waits for the capture to connect and
    drain even though the run finishes in well under a second.
  * tracy-capture is started FIRST and retries the connection until the client
    appears, so nothing is missed.

Usage:
    python3 profile_headless.py --scene empty.scene [--scene other.scene ...] \
                                --frames 240 --tag baseline [--cvar n=v ...] [--repeat 3]

Requires the CLIs built by the TracyServer premake target:
    Build/Tools/TracyServer/Bin/{tracy-capture.exe, tracy-csvexport.exe}
and the engine at:
    Build/Bin/Win64_Release/Vanguard.exe
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
ENGINE = REPO / "Build" / "Bin" / "Win64_Release" / "Vanguard.exe"
TOOLS_BIN = REPO / "Build" / "Tools" / "TracyServer" / "Bin"
CAPTURE = TOOLS_BIN / "tracy-capture.exe"
CSVEXPORT = TOOLS_BIN / "tracy-csvexport.exe"


def git_sha():
    try:
        return subprocess.check_output(
            ["git", "-C", str(REPO), "rev-parse", "--short", "HEAD"],
            text=True, stderr=subprocess.DEVNULL,
        ).strip()
    except Exception:  # noqa: BLE001
        return "unknown"


def check_tools():
    missing = [str(p) for p in (ENGINE, CAPTURE, CSVEXPORT) if not p.exists()]
    if missing:
        sys.stderr.write(
            "ERROR: missing required binaries:\n  " + "\n  ".join(missing) + "\n"
            "Build the engine (Win64_Release) and the TracyServer target "
            "(builds tracy-capture / tracy-csvexport).\n"
        )
        return False
    return True


def one_capture(out_dir, scene, frames, cvars, address, port):
    tracy_path = out_dir / "profile.tracy"
    png_path = out_dir / "frame.png"
    if tracy_path.exists():
        tracy_path.unlink()

    # Windows binaries misinterpret WSL's absolute /mnt/c/... paths (a leading "/"
    # is read as "relative to the current drive root"), so pass paths relative to
    # REPO with cwd=REPO instead of the absolute POSIX paths.
    tracy_rel = tracy_path.relative_to(REPO)
    png_rel = png_path.relative_to(REPO)

    # 1. Start the capture first; it retries the connection until the client shows up.
    cap = subprocess.Popen(
        [str(CAPTURE), "-f", "-o", str(tracy_rel), "-a", address, "-p", str(port)],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, cwd=str(REPO),
    )

    # 2. Launch the engine with TRACY_NO_EXIT so it blocks at shutdown until drained.
    env = dict(os.environ)
    env["TRACY_NO_EXIT"] = "1"
    env["TRACY_PORT"] = str(port)
    cmd = [str(ENGINE), "--headless", "--profile",
           "--scene", scene, "--delay", str(frames), "--output", str(png_rel)]
    for cv in cvars:
        cmd += ["--cvar", cv]

    time.sleep(0.2)  # let the capture bind before the engine connects
    eng = subprocess.run(cmd, env=env, cwd=str(REPO))
    if eng.returncode != 0:
        cap.terminate()
        raise RuntimeError(f"engine exited {eng.returncode} (see Log.txt)")

    # 3. Capture finishes once the client drains + disconnects.
    try:
        cap.wait(timeout=120)
    except subprocess.TimeoutExpired:
        cap.terminate()
        raise RuntimeError("tracy-capture did not finish; is TRACY_NO_EXIT honored?")

    if not tracy_path.exists():
        raise RuntimeError("no .tracy produced; capture never connected to the client")
    return tracy_path, png_path


def export_csv(tracy_path, out_dir):
    gpu_csv = out_dir / "gpu_zones.csv"
    cpu_csv = out_dir / "cpu_zones.csv"
    tracy_rel = tracy_path.relative_to(REPO)
    with open(gpu_csv, "w", encoding="utf-8") as fh:
        subprocess.run([str(CSVEXPORT), "--gpu", str(tracy_rel)], stdout=fh, check=True, cwd=str(REPO))
    with open(cpu_csv, "w", encoding="utf-8") as fh:
        subprocess.run([str(CSVEXPORT), str(tracy_rel)], stdout=fh, check=True, cwd=str(REPO))
    return gpu_csv, cpu_csv


def scene_stem(scene):
    return Path(scene).stem or "scene"


def run_scene(scene, base_out, args):
    """Capture one scene (args.repeat times), returning its meta entry.

    Paths in the returned entry are RELATIVE to base_out so the compare scripts can
    resolve them against a runs/<tag> directory regardless of where it lives.
    """
    stem = scene_stem(scene)
    scene_dir = base_out / stem
    scene_dir.mkdir(parents=True, exist_ok=True)

    gpu_rel = []
    for i in range(args.repeat):
        out_dir = scene_dir if args.repeat == 1 else scene_dir / f"run{i}"
        out_dir.mkdir(parents=True, exist_ok=True)
        print(f"[{stem}] capture {i+1}/{args.repeat}  frames={args.frames} -> {out_dir}")
        tracy_path, _png = one_capture(
            out_dir, scene, args.frames, args.cvar, args.address, args.port
        )
        gpu_csv, _cpu_csv = export_csv(tracy_path, out_dir)
        gpu_rel.append(str(gpu_csv.relative_to(base_out)))

    # For repeat>1, surface run0's artifacts at the scene dir root for image diffing.
    if args.repeat > 1:
        for fname in ("frame.png", "gpu_zones.csv", "profile.tracy"):
            src = scene_dir / "run0" / fname
            if src.exists():
                shutil.copy2(src, scene_dir / fname)

    return {
        "scene": scene,
        "stem": stem,
        "dir": stem,
        "png": str((scene_dir / "frame.png").relative_to(base_out)),
        "gpu_csv": str((scene_dir / "gpu_zones.csv").relative_to(base_out)),
        "gpu_csvs": gpu_rel,  # all repeats, for median in the profile gate
    }


def main(argv=None):
    p = argparse.ArgumentParser(description="Profiled headless Vanguard run(s), one per scene.")
    p.add_argument("--scene", action="append", required=True,
                   help="scene file; repeat to capture several scenes in one tag")
    p.add_argument("--frames", type=int, default=120, help="--delay frames before capture")
    p.add_argument("--tag", required=True, help="output goes to runs/<tag>/")
    p.add_argument("--cvar", action="append", default=[], help="name=value, repeatable")
    p.add_argument("--repeat", type=int, default=1, help="repeat captures per scene (for median)")
    p.add_argument("--address", default="127.0.0.1")
    p.add_argument("--port", type=int, default=8086)
    p.add_argument("--runs-dir", default=str(REPO / "runs"))
    args = p.parse_args(argv)

    if not check_tools():
        return 2

    # Reject duplicate scene stems -- they'd collide on the same output folder.
    stems = [scene_stem(s) for s in args.scene]
    dupes = {s for s in stems if stems.count(s) > 1}
    if dupes:
        sys.stderr.write(f"ERROR: scenes collide on folder name(s): {sorted(dupes)}\n")
        return 2

    base_out = Path(args.runs_dir) / args.tag
    base_out.mkdir(parents=True, exist_ok=True)

    scenes_meta = []
    for scene in args.scene:
        try:
            scenes_meta.append(run_scene(scene, base_out, args))
        except Exception as e:  # noqa: BLE001
            sys.stderr.write(f"ERROR ({scene}): {e}\n")
            return 1

    meta = {
        "tag": args.tag, "frames": args.frames, "cvars": args.cvar,
        "repeat": args.repeat, "git": git_sha(), "scenes": scenes_meta,
    }
    with open(base_out / "meta.json", "w", encoding="utf-8") as fh:
        json.dump(meta, fh, indent=2)

    print(f"\nDone. git={meta['git']}  tag={args.tag}  scenes={len(scenes_meta)}")
    for s in scenes_meta:
        print(f"  {s['stem']:<16} image: {base_out / s['png']}")
    print("Next: rank passes (rank_passes.py), optimize, then gate the whole tag with")
    print("      compare_images.py  --baseline-dir runs/baseline --candidate-dir runs/<cand> --exact")
    print("      compare_profiles.py --baseline-dir runs/baseline --candidate-dir runs/<cand> --require-speedup <P>")
    return 0


if __name__ == "__main__":
    sys.exit(main())
