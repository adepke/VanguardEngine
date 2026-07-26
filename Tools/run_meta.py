#!/usr/bin/env python3
"""
run_meta.py -- shared helpers for reading a runs/<tag>/meta.json and matching the
scenes of a baseline tag against a candidate tag (by stem).

Used by compare_images.py and compare_profiles.py so both agree on the layout the
current profile_headless.py produces:
    runs/<tag>/meta.json         -> {"scenes": [{stem, png, gpu_csv, gpu_csvs, ...}]}
    runs/<tag>/<stem>/frame.png, gpu_zones.csv, ...
"""
import json
from pathlib import Path


def load_scenes(tag_dir):
    """Return (meta, scenes) for a runs/<tag> directory."""
    tag_dir = Path(tag_dir)
    meta_path = tag_dir / "meta.json"
    if not meta_path.exists():
        raise FileNotFoundError(
            f"{meta_path} not found; run profile_headless.py to produce it"
        )
    with open(meta_path, encoding="utf-8") as fh:
        meta = json.load(fh)
    scenes = meta.get("scenes")
    if scenes is None:
        raise ValueError(
            f"{meta_path}: no 'scenes' key (old single-scene layout?). "
            f"Re-capture with the current profile_headless.py."
        )
    return meta, scenes


def match_scenes(base_dir, cand_dir):
    """Return (matched, missing_in_cand, missing_in_base).

    matched is a list of (stem, base_entry, cand_entry) for stems in BOTH tags,
    with absolute paths resolved for 'png'/'gpu_csv'/'gpu_csvs'.
    """
    base_dir = Path(base_dir)
    cand_dir = Path(cand_dir)
    _, bscenes = load_scenes(base_dir)
    _, cscenes = load_scenes(cand_dir)

    def resolve(entry, root):
        e = dict(entry)
        e["png_path"] = root / entry["png"]
        e["gpu_csv_path"] = root / entry["gpu_csv"]
        e["gpu_csv_paths"] = [root / c for c in entry.get("gpu_csvs", [entry["gpu_csv"]])]
        return e

    bmap = {s["stem"]: resolve(s, base_dir) for s in bscenes}
    cmap = {s["stem"]: resolve(s, cand_dir) for s in cscenes}
    common = sorted(set(bmap) & set(cmap))
    matched = [(st, bmap[st], cmap[st]) for st in common]
    return matched, sorted(set(bmap) - set(cmap)), sorted(set(cmap) - set(bmap))
