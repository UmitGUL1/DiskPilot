"""Measure real CLI metadata scanning on a controlled synthetic tree.

Example: python tools/benchmark.py --cli build/Release/diskpilot-cli.exe --files 10000
No performance claim is inferred from fixture generation time or other processes.
"""
import argparse
import json
import platform
from pathlib import Path
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cli", required=True, type=Path)
    parser.add_argument("--files", type=int, choices=[10000, 100000, 500000, 1000000], default=10000)
    args = parser.parse_args()
    executable = args.cli.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="diskpilot-benchmark-") as temporary:
        root = Path(temporary)
        for start in range(0, args.files, 1000):
            folder = root / f"group-{start // 1000:04d}"
            folder.mkdir()
            for index in range(start, min(start + 1000, args.files)):
                (folder / f"file-{index:07d}.dat").write_bytes(b"x" * 128)
        started = time.perf_counter()
        completed = subprocess.run([str(executable), "scan", str(root), "--format", "json"], capture_output=True, text=True, encoding="utf-8")
        elapsed = time.perf_counter() - started
        if completed.returncode != 0:
            raise RuntimeError(completed.stderr)
        scan = json.loads(completed.stdout)
        if not scan["complete"] or scan["files"] != args.files or scan["logical_bytes"] != args.files * 128:
            raise RuntimeError("Benchmark result did not match the fixture")
        print(json.dumps({"platform": platform.platform(), "processor": platform.processor(),
                          "fixture_files": args.files, "bytes_per_file": 128,
                          "engine_duration_ms": scan["duration_ms"], "process_wall_seconds": elapsed,
                          "files_per_wall_second": args.files / elapsed,
                          "peak_memory_bytes": None, "peak_memory_note": "Not measured; no estimate is substituted.",
                          "scope": "Metadata scan plus process startup and JSON export; fixture creation excluded."}, indent=2))


if __name__ == "__main__":
    main()
