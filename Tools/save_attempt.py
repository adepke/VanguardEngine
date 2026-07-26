#!/usr/bin/env python3
"""
save_attempt.py -- write a results.md for an optimization attempt (success or fail).

Called at the END of every attempt, whichever way it went. It:
  * copies the files named by --files (only -- never inferred from a bare `git diff`,
    since the working tree may carry large unrelated pre-existing dirty files) into
    <run>/changed/, preserving structure, plus a unified <run>/changes.diff scoped
    to just those files;
  * reads the gate JSONs (compare_images / compare_profiles, dir mode) to embed
    the visual-diff and Tracy-profile outcomes;
  * writes <run>/results.md: a short header, the driver's notes paragraph, the two
    outcomes, and the changed-file list.
  * with --revert, `git checkout --` restores exactly the --files list (nothing else).

The <run> folder IS the attempt run dir (e.g. runs/attempt_opus_2026_07_25_14_30),
which already holds the per-scene frame.png + Tracy data from profile_headless.py.

Usage:
    python3 Tools/save_attempt.py --run runs/attempt_opus_2026_07_25_14_30 \\
        --verdict failed --pass "Clouds Pass" \\
        --files VanguardEngine/Shaders/Clouds/Core.hlsli \\
        --image-json runs/attempt_.../image.json \\
        --profile-json runs/attempt_.../profile.json \\
        --notes "Hoisted the ... ; exact gate failed on clouds (FMA reassoc)." --revert
"""
import argparse
import datetime as _dt
import json
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent


def git_lines(repo, args):
    try:
        out = subprocess.check_output(
            ["git", "-C", str(repo)] + args, text=True, stderr=subprocess.DEVNULL
        )
    except Exception:  # noqa: BLE001
        return []
    return [ln for ln in out.splitlines() if ln.strip()]


def git_sha(repo):
    v = git_lines(repo, ["rev-parse", "--short", "HEAD"])
    return v[0] if v else "unknown"


def changed_files(repo, runs_dir_name, explicit_files):
    """The files the driver says this attempt touched, restricted to the ones that
    actually show a diff against HEAD (so a stale/typo'd --files entry is silently
    dropped instead of acted on).

    Never inferred from a bare `git diff --name-only` -- the working tree may carry
    large unrelated pre-existing dirty files that must never be swept into an
    attempt's changed-file set (and, critically, must never be reverted by --revert).
    """
    dirty = set(git_lines(repo, ["diff", "--name-only"]))
    dirty.update(git_lines(repo, ["diff", "--name-only", "--cached"]))
    wanted = {f.replace("\\", "/") for f in explicit_files}
    return sorted(f for f in wanted if f in dirty and not f.startswith(runs_dir_name + "/"))


def load_json(path):
    if not path:
        return None
    try:
        with open(path, encoding="utf-8") as fh:
            return json.load(fh)
    except Exception as e:  # noqa: BLE001
        sys.stderr.write(f"WARNING: could not read {path}: {e}\n")
        return None


def summarize_profile(pj):
    if not pj:
        return "_No profile gate JSON supplied._"
    if pj.get("mode") == "dir":
        v = "PASS" if pj.get("pass") else "FAIL"
        wt = pj.get("worst_total_pct")
        ws = pj.get("worst_total_scene")
        lines = [f"**{v}** — worst-case per-frame GPU **{wt:+.1f}%** (scene `{ws}`)."]
        for t, imp in (pj.get("target_worst_improvement") or {}).items():
            if imp is None:
                lines.append(f"- Target `{t}`: not present in any matched scene.")
            else:
                lines.append(f"- Target `{t}`: **{imp:+.1f}%** in its worst scene.")
        return "\n".join(lines)
    # file mode
    v = "PASS" if pj.get("pass") else "FAIL"
    tp = pj.get("total_delta_pct", 0.0)
    imp = ", ".join(f"`{k}` {x:+.1f}%" for k, x in (pj.get("improvements") or {}).items() if x is not None)
    return f"**{v}** — per-frame GPU {tp:+.1f}%. {imp}".strip()


def summarize_image(ij):
    if not ij:
        return "_No image gate JSON supplied._"
    if "scenes" in ij and isinstance(ij.get("scenes"), dict) and "worst" in ij:
        v = "PASS" if ij.get("pass") else "FAIL"
        lines = [f"**{v}** — worst scene `{ij.get('worst')}`."]
        for stem, m in ij["scenes"].items():
            mark = "identical" if m.get("max_abs", 1) == 0 else f"max_abs={m.get('max_abs')}"
            lines.append(f"- `{stem}`: {'PASS' if m.get('pass') else 'FAIL'} ({mark})")
        return "\n".join(lines)
    # file mode metrics
    v = "PASS" if ij.get("pass") else "FAIL"
    return f"**{v}** — max_abs={ij.get('max_abs')}, changed={ij.get('changed_pct')}%."


def scenes_from_meta(run_dir):
    meta = run_dir / "meta.json"
    if not meta.exists():
        return []
    try:
        with open(meta, encoding="utf-8") as fh:
            return [s.get("stem", "?") for s in json.load(fh).get("scenes", [])]
    except Exception:  # noqa: BLE001
        return []


def infer_model(run_name, override):
    if override:
        return override
    parts = run_name.split("_")
    # attempt_<model>_YYYY_MM_DD_HH_MM
    if len(parts) >= 2 and parts[0] == "attempt":
        return parts[1]
    return "unknown"


def main(argv=None):
    p = argparse.ArgumentParser(description="Write results.md for an optimization attempt.")
    p.add_argument("--run", required=True, help="attempt run dir, e.g. runs/attempt_opus_2026_07_25_14_30")
    p.add_argument("--verdict", required=True, choices=("success", "failed"))
    p.add_argument("--pass", dest="target", default="", help="target pass name")
    p.add_argument("--model", default="", help="optimizer model (else parsed from run name)")
    p.add_argument("--notes", default="", help="the driver's paragraph on the attempt + outcome")
    p.add_argument("--image-json", default=None, help="compare_images --json output")
    p.add_argument("--profile-json", default=None, help="compare_profiles --json output")
    p.add_argument("--files", nargs="+", required=True,
                   help="explicit repo-relative path(s) this attempt's edit touched -- "
                        "the ONLY files ever archived/diffed/reverted, never inferred from git diff")
    p.add_argument("--revert", action="store_true", help="git checkout -- the files named by --files")
    p.add_argument("--repo", default=str(REPO))
    p.add_argument("--max-mb", type=float, default=5.0)
    args = p.parse_args(argv)

    repo = Path(args.repo).resolve()
    run_dir = Path(args.run)
    if not run_dir.is_absolute():
        run_dir = repo / run_dir
    run_dir.mkdir(parents=True, exist_ok=True)
    runs_root_name = run_dir.parent.name

    files = changed_files(repo, runs_root_name, args.files)

    # Copy changed files into <run>/changed/, preserving structure.
    changed_dir = run_dir / "changed"
    copied, skipped = [], []
    cap = args.max_mb * 1024 * 1024
    for rel in files:
        src = repo / rel
        if not src.exists():
            continue
        if src.stat().st_size > cap:
            skipped.append(rel)
            continue
        dst = changed_dir / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        copied.append(rel)

    # Unified diff, scoped to only this attempt's files -- never a bare `git diff`,
    # which would also capture any unrelated pre-existing dirty files.
    try:
        diff = subprocess.check_output(["git", "-C", str(repo), "diff", "--"] + files, text=True,
                                       stderr=subprocess.DEVNULL) if files else ""
        (run_dir / "changes.diff").write_text(diff, encoding="utf-8")
    except Exception as e:  # noqa: BLE001
        sys.stderr.write(f"WARNING: could not write changes.diff: {e}\n")

    ij = load_json(args.image_json)
    pj = load_json(args.profile_json)
    model = infer_model(run_dir.name, args.model)
    scenes = scenes_from_meta(run_dir)
    ts = _dt.datetime.now().isoformat(timespec="minutes")
    verdict_label = "SUCCESS" if args.verdict == "success" else "FAILED"

    file_list = "\n".join(f"- `{f}`" for f in copied) or "_none detected via git diff_"
    if skipped:
        file_list += f"\n\n_(skipped {len(skipped)} file(s) over {args.max_mb} MB)_"

    results = f"""# Optimization attempt — {verdict_label}

| | |
|---|---|
| Date | {ts} |
| Optimizer model | {model} |
| Base git | {git_sha(repo)} |
| Target pass | {args.target or "_(unspecified)_"} |
| Scenes | {", ".join(scenes) if scenes else "_(see meta.json)_"} |

## Notes
{args.notes.strip() or "_(none provided)_"}

## Speed (Tracy profile) outcome
{summarize_profile(pj)}

## Visual diff outcome
{summarize_image(ij)}

## Changed files
Copies preserved in `changed/`; full patch in `changes.diff`.

{file_list}
"""
    (run_dir / "results.md").write_text(results, encoding="utf-8")
    print(f"Wrote {run_dir / 'results.md'}  ({verdict_label}); archived {len(copied)} changed file(s).")

    if args.revert:
        if files:
            subprocess.run(["git", "-C", str(repo), "checkout", "--"] + files)
            print(f"Reverted {len(files)} tracked file(s): {', '.join(files)}")
        else:
            print("Nothing to revert (no named --files showed a diff).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
