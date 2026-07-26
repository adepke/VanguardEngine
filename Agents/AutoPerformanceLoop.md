# Auto Performance Loop — Runbook

Runbook for letting AI agents profile, examine, and optimize the engine in an iterative loop.

Paste-and-go instructions for a fresh context. **Run the driver as Sonnet.** The driver does the
cheap mechanical work (loop, profile, locate code, compare, commit) and delegates only the hard
part — reasoning about an optimization over real code — to an **Opus subagent**.

All commands use `python3` (not `python`) and are run from the repo root.

---

## Roles

| Model | Does |
| --- | --- |
| **Sonnet** (driver / this context) | Runs the loop: profile, rank passes, locate the pass + shader code, apply edits, rebuild (only if needed), run the image + speed gates, commit or revert, maintain the tried list. Keeps its own context lean — does **not** read large shaders itself. |
| **Opus** (subagent, one per hotspot) | Receives a single pass's code chunk (the `.cpp` Execute region + its `.hlsl`/`.hlsli`) and returns a concrete, fidelity-neutral optimization as edits + a one-line rationale. |

Delegate to Opus with the Agent tool, `model: "opus"`. Give it only the relevant chunk, not the repo.

## Fixed run parameters (keep identical every iteration)

- Scenes: one or more, e.g. `--scene empty.scene --scene clouds.scene`  ·  Frames: `--frames 240`
  (at least 240, more frames reduces trace data noise but increases loop time).
  Capture the **same scene set and frame count** for baseline and candidate (change it once, then
  never mid-loop — recapture the baseline if either changes).
- The engine is deterministic: same build + same args = pixel-identical image *for a byte-for-byte
  identical shader binary*. In practice, restructuring shader source (even with no value-changing
  edit) can shift DXC's instruction scheduling enough to flip the LSB of a handful of pixels without
  any real visual change — see the `--near-exact` note below. The image gate is **`--near-exact`**
  (see `Tools/compare_images.py`): `max_abs <= 1.0` (no channel of any pixel differs by more than one
  8-bit step) **and** `changed_pct == 0.0` (nothing crosses even the tool's small `--eps` threshold).
  Tight enough that a real algorithmic bug should still fail it, loose enough to pass pure compiler
  noise. Do not fall back to the looser general tolerance mode (PSNR/mean/changed-pct options) — that
  one is for comparing intentionally different builds, not for gating this loop.
- Each scene is captured and compared independently; the gates match scenes by stem and take the
  **worst** result across scenes. A change is accepted only if *every* scene passes.

## Environment invariant — read once, never violate

**WSL2:** the Tracy tools and `Vanguard.exe` are **Windows** binaries. A POSIX absolute arg like
`/mnt/c/dev/...` is interpreted by them as relative to the current drive root → a silent phantom
`C:\mnt\c\...`. **Always pass repo-relative paths with `cwd=REPO`.** `profile_headless.py` already
does this; do not "simplify" it back to absolute paths.

---

## Step 0 — one-time setup check

```
python3 Tools/env_check.py
```
Confirms necessary dependencies such as python and Tracy tooling is compiled. If the script returns
non-zero, the missing dependencies must be addressed before continuing.

Then load the tried list: read `runs/tried_passes.json` if it exists (list of
`{pass, verdict, reason, git}`). Skip any pass already recorded against the current git sha — a fresh
session discovers prior work here instead of being told.

## Run naming

- **Baseline** run tag: `baseline` → `runs/baseline/`.
- **Optimization attempt** run tag: `attempt_<model>_<YYYY>_<MM>_<DD>_<HH>_<MM>`, where `<model>` is the
  optimizer subagent's model (e.g. `opus`). Generate the timestamp with `date +%Y_%m_%d_%H_%M`, e.g.
  `runs/attempt_opus_2026_07_25_14_30/`.

Each run folder holds the per-scene frame PNG and all Tracy data
(`<stem>/{frame.png, profile.tracy, gpu_zones.csv, cpu_zones.csv}`) plus `meta.json`. An attempt folder
additionally gets `results.md`, `changes.diff`, and `changed/` (see step 6).

## Step 0b — baseline

**If `runs/baseline/` already exists AND was captured at the current `--frames` count and the same scenes
, do NOT re-generate it** — go straight to step 1. Review `meta.json` to see how it was configured.
Otherwise (absent, or captured with different args), delete the existing baseline if it exists and capture
it. Modify args as needed to match current testing setup:
```
python3 Tools/profile_headless.py --scene empty.scene --scene clouds.scene --frames 240 --tag baseline
```
Output nests per scene: `runs/baseline/<stem>/{frame.png, gpu_zones.csv, ...}` + `runs/baseline/meta.json`.

---

## The loop

**1. Pick a hotspot (Sonnet).** Rank each scene (per-scene CSVs live at `runs/baseline/<stem>/gpu_zones.csv`):
```
python3 Tools/rank_passes.py runs/baseline/<stem>/gpu_zones.csv
```
This aggregates the raw per-event CSV and prints **steady-state** passes ranked by per-frame `mean_ns`,
with **one-shot startup passes listed separately (counts==1) and excluded** — never target those
(they're load-time precompute, not per-frame cost). Pick a top steady-state pass `P` (not in the tried
list) that is expensive across the scenes — prefer one that's hot in more than one scene.

**2. Locate the code (Sonnet).**
```
grep -rn 'AddPass("<P>"' VanguardEngine/Source/Rendering
```
Open that pass's `Execute` lambda; find the shader(s) it dispatches (grep the shader/pipeline name)
under `VanguardEngine/Shaders`. Collect the `.cpp` region + the `.hlsl`/`.hlsli` file(s) to hand to
Opus. Do not analyze them yourself.

**3. Optimize (Opus subagent).** Spawn `Agent(model: "opus")` with the chunk and these instructions:
> Pass `P` costs `<mean_ns>`/frame on the GPU. Here is its Execute code and shader source: `<chunk>`.
> Propose a **fidelity-neutral** optimization — reformulate for speed only. Do **not** reduce sample
> counts, resolution, or precision, or otherwise change output.
> - Prefer the **narrow edit**. Smaller diffs are far more likely to survive the gate.
> - Any change must cause minimal impact on the rendered output, the output will be visually-diffed to
>   ensure nearly-exact visual results.
> - Do **not** claim to have compiled, verified, or diffed DXIL/assembly — you can't run the compiler.
>   The **only** real verification comes from the parent agent, who will perform the checks. Report
>   confidence honestly, and based off code only.
> Return concrete edits (file + exact old/new text) and a one-line rationale. If nothing safe is
> available, say so.

**4. Apply + build (Sonnet).** Apply Opus's edits, then:
- **Edits touch only `.hlsl`/`.hlsli`?** → **Skip the C++ rebuild.** Shaders are JIT-compiled from
  source at launch with zero on-disk shader cache, so re-profiling picks them up directly.
- **Edits touch `.cpp`/`.h`?** → rebuild:
  `msbuild Vanguard.sln /p:Configuration=Release /p:Platform=Win64 /m`
  On build failure: hand the compiler error back to the same Opus subagent, or revert and mark `P` tried.

**5. Re-profile (Sonnet).** Use the **same scene set and frame count** as the baseline. Name the tag
per the convention (`RUN=attempt_<model>_$(date +%Y_%m_%d_%H_%M)`):
```
python3 Tools/profile_headless.py --scene empty.scene --scene clouds.scene --frames 240 --tag "$RUN"
```

**6. Gate + record (Sonnet).** Run both gates in **dir mode** (match scenes by stem, gate on the worst
scene), writing their JSON into the attempt folder. **Both must pass**: the image gate alone is not
proof of a successful optimization — `compare_profiles.py --require-speedup "<P>"` must also show a
real worst-scene speedup for `P`, not just an absence of regression:
```
python3 Tools/compare_images.py   --baseline-dir runs/baseline --candidate-dir "runs/$RUN" --near-exact \
        --out "runs/$RUN/diffs" --json "runs/$RUN/image.json"
python3 Tools/compare_profiles.py --baseline-dir runs/baseline --candidate-dir "runs/$RUN" \
        --require-speedup "<P>" --json "runs/$RUN/profile.json"
```
Then **always write `results.md`** — on success or failure — with a paragraph covering *both* the Tracy
profile outcome and the visual-diff outcome:
Both `save_attempt.py` calls below **require `--files <path> [<path> ...]`** — the exact repo-relative
path(s) this iteration's edit touched (e.g. `--files VanguardEngine/Shaders/Clouds/Core.hlsli`). This
list is the *only* thing `save_attempt.py` archives, diffs, or (with `--revert`) reverts — it is never
inferred from `git diff`, because the working tree may carry large unrelated pre-existing dirty files
that must never be swept in or reverted. Get this list wrong and either the wrong file gets reverted, or
an unrelated dirty file does.

- **Either gate fails:** rejected. Archive + revert:
  ```
  python3 Tools/save_attempt.py --run "runs/$RUN" --verdict failed --pass "<P>" \
      --files <path(s) this edit touched> \
      --image-json "runs/$RUN/image.json" --profile-json "runs/$RUN/profile.json" \
      --notes "<what you changed; the profile result; the visual-diff result and likely cause>" --revert
  ```
  (Image fail is usually DXC FMA/scheduling, not a logic bug; speed fail = no real win / a scene regressed.)
  Record in the tried list (note the `results.md` path), go to **1**.
- **Both pass:** accept. Record results first (no revert), then commit and promote:
  ```
  python3 Tools/save_attempt.py --run "runs/$RUN" --verdict success --pass "<P>" \
      --files <path(s) this edit touched> \
      --image-json "runs/$RUN/image.json" --profile-json "runs/$RUN/profile.json" \
      --notes "<what you changed; the worst-case GPU win; visual diff exact-match confirmed>"
  git add <only the same --files paths — never `-A`/`.`> && git commit -m "<P>: <what changed> (-X.X% worst-scene GPU)"
  ```
  Promote candidate to baseline: copy `runs/$RUN/*` over `runs/baseline/`. Record in tried list. Go to **1**.

`save_attempt.py` copies the named `--files` into `runs/$RUN/changed/`, writes `runs/$RUN/changes.diff`
(scoped to those files), and generates `runs/$RUN/results.md` (header + your notes + both outcomes +
changed-file list).

**Always update `runs/tried_passes.json`** after each iteration: append
`{pass, verdict: accepted|reverted, reason, git, run}` where `run` is the attempt folder
(`runs/attempt_<model>_<ts>/`) so a later review can open its `results.md` and `changes.diff`.

**Stop when:** no untried steady-state pass yields a passing + faster change, a target frame time is
met, or the requested iteration count is reached. Then summarize: passes optimized, per-frame and
per-pass GPU delta, commits made.

## Guardrails

- **Never `git push`.** Commit locally per iteration only; the user reviews and pushes once all
  optimization passes for the session are complete.
- **Never `git add -A`/`git add .`.** Stage only the files this iteration's edit actually touched —
  the working tree may carry large unrelated pre-existing diffs that must not ride along.
- **Always pass explicit `--files` to `save_attempt.py`.** Never let it infer touched files from
  `git diff` — with a dirty pre-existing working tree that silently reverts/archives everything, not
  just this iteration's edit.
- **Fidelity-neutral only.** The `--near-exact` image gate is the hard enforcement — never relax it
  further (e.g. never fall back to the general PSNR/mean/changed-pct tolerance mode).
- **A passing image gate is not a successful optimization by itself.** `compare_profiles.py
  --require-speedup "<P>"` must also show a real worst-scene speedup — an image-only pass with no
  measurable speedup (or a regression) is still rejected.
- **One pass per iteration.** Always gate before moving on; never batch unverified edits.
- **Keep the driver lean.** Sonnet greps/reads *just enough* to identify the region; Opus reads and
  reasons over the code chunk.
- **Never accept a per-frame GPU regression** beyond the `compare_profiles` tolerance, even if `P` sped up.
- **Baseline and candidate must share the same `--frames` count.** Recapture the baseline if it changes.
- **Target steady-state passes only** — one-shot startup passes are not per-frame cost.
