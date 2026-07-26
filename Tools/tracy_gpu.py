#!/usr/bin/env python3
"""
tracy_gpu.py -- shared parser/aggregator for tracy-csvexport GPU output.

IMPORTANT (this Tracy build, 0.13.5): `tracy-csvexport --gpu` emits RAW per-event
rows -- one row per zone occurrence per frame -- with columns:

    name, src_file, Time from start of program, GPU execution time    (ns)

There is NO aggregate mode in this build. Earlier versions of the compare script
assumed pre-aggregated columns (total_ns, mean_ns, counts, ...) that DO NOT EXIST
here, so recurring passes silently aggregated to 0 and the speed gate always
"passed". This module is the single source of truth for aggregation so that bug
cannot be reintroduced in one script but not the other.

aggregate_gpu_csv(path) groups by pass name and derives:
    counts, total_ns, mean_ns, min_ns, max_ns, std_ns
from the per-event "GPU execution time" column. It also transparently accepts a
pre-aggregated CSV (should a future Tracy build produce one), so callers don't
have to care which format they got.

A pass's `counts` distinguishes steady-state per-frame work (counts ~= frame
count) from one-shot startup passes (counts == 1, e.g. precompute/noise bakes).
Rank on steady-state passes; a one-shot's huge total_ns is startup cost, not
per-frame cost.
"""
import csv
import statistics


def _norm_fields(fieldnames):
    """Map stripped/lowercased header -> actual header string."""
    return {(f.strip().lower() if f else ""): f for f in (fieldnames or [])}


def _stats(times):
    n = len(times)
    total = float(sum(times))
    return {
        "counts": n,
        "total_ns": total,
        "mean_ns": total / n if n else 0.0,
        "min_ns": float(min(times)) if times else 0.0,
        "max_ns": float(max(times)) if times else 0.0,
        "std_ns": float(statistics.pstdev(times)) if n > 1 else 0.0,
    }


def _aggregate_raw(reader, name_col, time_col):
    times = {}
    for row in reader:
        name = (row.get(name_col) or "").strip()
        raw = row.get(time_col)
        if not name or raw is None or raw == "":
            continue
        try:
            t = float(raw)
        except ValueError:
            continue
        times.setdefault(name, []).append(t)
    return {name: _stats(ts) for name, ts in times.items() if ts}


def _read_preaggregated(reader, name_col, cols):
    out = {}
    numeric = ("total_ns", "mean_ns", "min_ns", "max_ns", "std_ns", "counts")
    for row in reader:
        name = (row.get(name_col) or "").strip()
        if not name:
            continue
        rec = {}
        for key in numeric:
            c = cols.get(key)
            if c and row.get(c) not in (None, ""):
                try:
                    rec[key] = float(row[c])
                except ValueError:
                    pass
        if name in out:  # merge dup names conservatively
            prev = out[name]
            prev["total_ns"] = prev.get("total_ns", 0.0) + rec.get("total_ns", 0.0)
            prev["counts"] = prev.get("counts", 0.0) + rec.get("counts", 0.0)
            prev["mean_ns"] = max(prev.get("mean_ns", 0.0), rec.get("mean_ns", 0.0))
        else:
            out[name] = rec
    return out


def aggregate_gpu_csv(path):
    """Return {pass_name: {counts,total_ns,mean_ns,min_ns,max_ns,std_ns}}."""
    with open(path, newline="", encoding="utf-8") as fh:
        sample = fh.read(8192)
        fh.seek(0)
        try:
            dialect = csv.Sniffer().sniff(sample, delimiters=",;\t")
        except csv.Error:
            dialect = csv.excel
        reader = csv.DictReader(fh, dialect=dialect)
        cols = _norm_fields(reader.fieldnames)
        if "name" not in cols:
            raise ValueError(
                f"{path}: not a tracy-csvexport CSV (no 'name' column); "
                f"header was {reader.fieldnames}"
            )
        name_col = cols["name"]
        time_col = cols.get("gpu execution time")
        if time_col:
            return _aggregate_raw(reader, name_col, time_col)
        if cols.get("total_ns") or cols.get("mean_ns"):
            return _read_preaggregated(reader, name_col, cols)
        raise ValueError(
            f"{path}: unrecognized columns {reader.fieldnames}. Expected a "
            f"'GPU execution time' (raw per-event) or 'total_ns'/'mean_ns' "
            f"(pre-aggregated) column."
        )


def steady_state(table, min_counts=2):
    """Passes that run every frame (counts >= min_counts) -- real per-frame targets."""
    return {n: r for n, r in table.items() if r.get("counts", 0) >= min_counts}


def one_shot(table):
    """Passes that ran exactly once -- startup/precompute cost, not per-frame."""
    return {n: r for n, r in table.items() if r.get("counts", 0) == 1}


def per_frame_total_ns(table):
    """Representative single-frame GPU pass time: sum of mean_ns over steady-state passes.

    Uses mean_ns (not summed total_ns) so the figure isn't diluted by one-shot
    startup passes or skewed by differing frame counts between runs.
    """
    return sum(r["mean_ns"] for r in steady_state(table).values() if "mean_ns" in r)
