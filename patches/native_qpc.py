#!/usr/bin/env python3
"""Select the official QP/C 8.1.4 POSIX port for a native host build."""
from pathlib import Path
import shutil
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: native_qpc.py <kode-sdk-dir> <project-root>")

sdk = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
source = root / "ports/qpc-posix"
target = sdk / "components/qpc/port"
target.mkdir(parents=True, exist_ok=True)

for name in ("qp_port.h", "qp_config.h", "qf_port.c", "safe_std.h"):
    src = source / name
    if not src.is_file():
        raise SystemExit(f"Missing vendored official POSIX port file: {src}")
    shutil.copy2(src, target / name)

print("Selected Quantum Leaps QP/C 8.1.4 POSIX port for native host build.")
