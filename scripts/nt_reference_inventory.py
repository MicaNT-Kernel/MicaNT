#!/usr/bin/env python3
"""Inventory ReactOS NT kernel source areas without importing source code."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path


AREAS = {
    "ke": ("ntoskrnl/ke",),
    "mm": ("ntoskrnl/mm",),
    "ps": ("ntoskrnl/ps",),
    "ob": ("ntoskrnl/ob",),
    "io": ("ntoskrnl/io",),
    "cm": ("ntoskrnl/config", "ntoskrnl/cm"),
    "se": ("ntoskrnl/se",),
    "cc": ("ntoskrnl/cc", "ntoskrnl/cache"),
    "fsrtl": ("ntoskrnl/fsrtl",),
    "ex": ("ntoskrnl/ex",),
    "lpc": ("ntoskrnl/lpc",),
    "po": ("ntoskrnl/po",),
    "pnp": ("ntoskrnl/io/pnpmgr",),
    "hal": ("hal",),
}
SOURCE_SUFFIXES = {".c", ".h", ".cpp", ".hpp", ".S", ".asm"}
DECLARATION = re.compile(
    r"(?m)^\s*(?:static\s+)?(?:inline\s+)?"
    r"(?:[A-Za-z_]\w*(?:\s+|\s*[*&]\s*)+)+"
    r"([A-Za-z_]\w*)\s*\([^;{}]*\)\s*(?:\n\s*)?\{"
)
TYPE_DECLARATION = re.compile(r"\b(?:struct|union|enum)\s+([A-Za-z_]\w*)")
TYPEDEF_ALIAS = re.compile(r"\btypedef\b[^;]*\b([A-Za-z_]\w*)\s*;")


def _git_revision(root: Path) -> str | None:
    try:
        return subprocess.run(
            ["git", "-C", str(root), "rev-parse", "HEAD"],
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def _symbols(path: Path) -> tuple[set[str], set[str]]:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return set(), set()
    functions = set(DECLARATION.findall(text))
    types = set(TYPE_DECLARATION.findall(text))
    types.update(TYPEDEF_ALIAS.findall(text))
    return functions, types


def inventory(root: Path) -> dict[str, object]:
    result: dict[str, object] = {
        "schema_version": 1,
        "reference": "ReactOS",
        "revision": _git_revision(root),
        "areas": {},
    }
    areas: dict[str, object] = {}
    for name, candidates in AREAS.items():
        files: list[Path] = []
        missing: list[str] = []
        for candidate in candidates:
            directory = root / candidate
            if directory.is_dir():
                files.extend(
                    path for path in directory.rglob("*")
                    if path.is_file() and path.suffix in SOURCE_SUFFIXES
                )
            else:
                missing.append(candidate)

        functions: set[str] = set()
        types: set[str] = set()
        for path in files:
            found_functions, found_types = _symbols(path)
            functions.update(found_functions)
            types.update(found_types)
        areas[name] = {
            "source_roots": list(candidates),
            "missing_roots": missing,
            "source_file_count": len(files),
            "source_files": sorted(
                path.relative_to(root).as_posix() for path in files
            ),
            "candidate_function_count": len(functions),
            "candidate_type_count": len(types),
            "candidate_functions": sorted(functions),
            "candidate_types": sorted(types),
            "test_paths": sorted({
                path.relative_to(root).as_posix()
                for source_root in candidates
                for path in (root / source_root).rglob("*")
                if path.is_file()
                and ("test" in path.parts or "tests" in path.parts)
                and path.suffix in SOURCE_SUFFIXES
            }),
        }
    result["areas"] = areas
    return result


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Inventory ReactOS NT kernel source areas as JSON."
    )
    parser.add_argument("reactos_root", type=Path, help="path to a ReactOS checkout")
    parser.add_argument(
        "--output", type=Path, help="write JSON here instead of stdout"
    )
    args = parser.parse_args()
    root = args.reactos_root.resolve()
    if not root.is_dir():
        parser.error(f"not a directory: {root}")
    output = json.dumps(inventory(root), indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(output, encoding="utf-8")
    else:
        print(output, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
