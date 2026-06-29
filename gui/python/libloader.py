"""
libloader.py — locate each example's compiled shared library and expose it to
the per-example ctypes bindings via their ``<NAME>_CORE_LIB`` env hooks.

Import this module before importing anything from ``bindings`` (it primes the
environment variables on import).
"""
from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import Dict, List

# repo root = .../multi-gui-control-templates  (this file is gui/python/libloader.py)
_ROOT = Path(__file__).resolve().parents[2]

# example subdir -> (lib basename, env var consumed by its binding)
_EXAMPLES: Dict[str, "tuple[str, str]"] = {
    "pid":                ("pid_core",   "PID_CORE_LIB"),
    "pi_path_tracking":   ("track_core", "TRACK_CORE_LIB"),
    "two_dof":            ("tdof_core",  "TDOF_CORE_LIB"),
    "mass_spring_damper": ("msd_core",   "MSD_CORE_LIB"),
}


def _libfile_names(base: str) -> List[str]:
    if sys.platform.startswith("win"):
        return [f"{base}.dll"]
    if sys.platform == "darwin":
        return [f"lib{base}.dylib"]
    return [f"lib{base}.so"]


def _find_lib(example: str, base: str) -> Path | None:
    build = _ROOT / "examples" / example / "core" / "build"
    search = [build, build / "Release", build / "Debug", build / "RelWithDebInfo"]
    for rb in (_ROOT / "build", _ROOT / "build" / "lib"):
        search += [rb, rb / "Release", rb / "Debug", rb / "RelWithDebInfo"]
    newest: Path | None = None
    for d in search:
        for name in _libfile_names(base):
            p = d / name
            if p.is_file():
                if newest is None or p.stat().st_mtime > newest.stat().st_mtime:
                    newest = p
    return newest


def prime() -> Dict[str, Path]:
    found: Dict[str, Path] = {}
    for example, (base, env) in _EXAMPLES.items():
        if os.environ.get(env):
            found[example] = Path(os.environ[env])
            continue
        lib = _find_lib(example, base)
        if lib is not None:
            os.environ[env] = str(lib)
            found[example] = lib
    return found


RESOLVED = prime()
