#!/usr/bin/env python3
"""
Verifies if all dependencies for the AutoPerformanceLoop runbook are present.
Exit 0 if everything needed is available; 1 if a hard requirement is missing.
"""
import os
import platform
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
ENGINE_DIR = REPO / "Build" / "Bin"
TOOLS_BIN = REPO / "Build" / "Tools" / "TracyServer" / "Bin"
REQUIRED_TOOLS = [TOOLS_BIN / "tracy-capture.exe", TOOLS_BIN / "tracy-csvexport.exe"]


def detect_wsl():
    # WSL exposes "microsoft"/"WSL" in the kernel release/version.
    for path in ("/proc/sys/kernel/osrelease", "/proc/version"):
        try:
            with open(path, encoding="utf-8", errors="ignore") as fh:
                if "microsoft" in fh.read().lower():
                    return True
        except OSError:
            pass
    return bool(os.environ.get("WSL_DISTRO_NAME"))


def ok(label, good, detail=""):
    mark = "OK " if good else "!! "
    print(f"  [{mark}] {label}{(' -- ' + detail) if detail else ''}")
    return good


def main():
    print("Perf-loop environment check")
    print("=" * 60)

    system = platform.system()
    is_wsl = (system == "Linux") and detect_wsl()
    env = "WSL2" if is_wsl else system
    print(f"  interpreter : {sys.executable}")
    print(f"  python      : {platform.python_version()}")
    print(f"  environment : {env}")

    hard_ok = True

    print("\nPython interpreter:")
    # `python` may be Python 2 or absent on Linux/WSL; the loop standardizes on python3.
    if shutil.which("python3") is None:
        hard_ok &= ok("python3 on PATH", False, "install python3 (the loop uses `python3`, not `python`)")
    else:
        ok("python3 on PATH", True)

    print("\nPython packages (needed by compare_images.py):")
    for mod in ("numpy", "PIL"):
        try:
            __import__(mod)
            ok(mod, True)
        except ImportError:
            hard_ok &= ok(mod, False, "pip install numpy Pillow")

    print("\nEngine build (need a profiling-enabled build):")
    engines = list(ENGINE_DIR.glob("*/Vanguard.exe")) if ENGINE_DIR.exists() else []
    if engines:
        for e in engines:
            ok(f"engine: {e.relative_to(REPO)}", True)
    else:
        hard_ok &= ok("Vanguard.exe", False,
                      "build Release (or Development); see runbook prereqs")

    print("\nTracy headless CLIs (built by the TracyServer premake target):")
    for t in REQUIRED_TOOLS:
        if t.exists():
            ok(t.name, True)
        else:
            hard_ok &= ok(t.name, False, "build the TracyServer project (premake target)")

    if is_wsl:
        print("\n" + "=" * 60)
        print("WSL2 PATH INVARIANT (do not violate):")
        print("  The Tracy tools and Vanguard.exe are WINDOWS binaries. A POSIX")
        print("  absolute arg like /mnt/c/dev/... is read by them as relative to the")
        print("  current drive root -> a phantom C:\\mnt\\c\\... location, silently.")
        print("  => Always pass paths RELATIVE to the repo root with cwd=REPO.")
        print("  profile_headless.py already does this; keep it that way.")

    print("\n" + "=" * 60)
    print("READY" if hard_ok else "NOT READY -- resolve the !! items above")
    return 0 if hard_ok else 1


if __name__ == "__main__":
    sys.exit(main())
