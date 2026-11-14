#!/usr/bin/env python3
"""Export addresses of the simulator-only KodeOS framebuffer bridge symbols."""
from __future__ import annotations

import json
import sys
from pathlib import Path

from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection

NAMES = (
    "g_kode_web_fb_front",
    "g_kode_web_fb_seq",
    "g_kode_web_fb_dirty_y1",
    "g_kode_web_fb_dirty_y2",
)


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: export_framebuffer_symbols.py <kodeos.elf> <output.json>")
    elf_path = Path(sys.argv[1])
    out_path = Path(sys.argv[2])
    if not elf_path.is_file():
        raise SystemExit(f"firmware ELF not found: {elf_path}")

    found: dict[str, int] = {}
    with elf_path.open("rb") as stream:
        elf = ELFFile(stream)
        for section in elf.iter_sections():
            if not isinstance(section, SymbolTableSection):
                continue
            for symbol in section.iter_symbols():
                if symbol.name in NAMES:
                    found[symbol.name] = int(symbol["st_value"])

    missing = [name for name in NAMES if name not in found or found[name] == 0]
    if missing:
        raise SystemExit("framebuffer bridge symbols missing from ELF: " + ", ".join(missing))

    payload = {
        "architecture": "riscv32",
        "width": 410,
        "height": 502,
        "bytes_per_pixel": 2,
        "max_read_bytes": 4096,
        "front_pointer_address": hex(found["g_kode_web_fb_front"]),
        "sequence_address": hex(found["g_kode_web_fb_seq"]),
        "dirty_y1_address": hex(found["g_kode_web_fb_dirty_y1"]),
        "dirty_y2_address": hex(found["g_kode_web_fb_dirty_y2"]),
    }
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(payload, indent=2) + "\n")
    print(json.dumps(payload, indent=2))


if __name__ == "__main__":
    main()
