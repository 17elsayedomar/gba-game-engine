"""Counts lines of code in the editor prototypes (standard library only).

For each folder it reports:
  code     - lines with actual code
  comment  - lines that are only a comment (// ... or inside /* ... */)
  blank    - empty lines
Third-party code (prototypes/third_party) is not counted.

Usage:  python prototypes/tools/count_lines.py      (prints JSON)
"""

import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
FOLDERS = {
    "proto_core (shared)": ROOT / "proto_core" / "src",
    "proto_core headers": ROOT / "proto_core" / "include",
    "proto_core tests": ROOT / "proto_core" / "tests",
    "imgui_editor": ROOT / "imgui_editor",
    "qt_editor": ROOT / "qt_editor",
}
EXTENSIONS = {".cpp", ".hpp", ".h"}


def count_file(path: pathlib.Path) -> dict:
    code = comment = blank = 0
    in_block = False
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if in_block:
            comment += 1
            if "*/" in line:
                in_block = False
            continue
        if not line:
            blank += 1
        elif line.startswith("//"):
            comment += 1
        elif line.startswith("/*"):
            comment += 1
            in_block = "*/" not in line
        else:
            code += 1
    return {"code": code, "comment": comment, "blank": blank}


def main() -> None:
    result = {}
    for name, folder in FOLDERS.items():
        total = {"code": 0, "comment": 0, "blank": 0, "files": 0}
        for path in sorted(folder.rglob("*")):
            if path.suffix in EXTENSIONS:
                counts = count_file(path)
                for key, value in counts.items():
                    total[key] += value
                total["files"] += 1
        result[name] = total
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
