#!/usr/bin/env python3
"""Embed an original script with the symbol expected by its C++ consumer."""
import sys
from pathlib import Path

source, destination, symbol = sys.argv[1:]
data = Path(source).read_bytes() + b"\0"
rows = [",".join(str(x) for x in data[i:i+24]) for i in range(0, len(data), 24)]
Path(destination).parent.mkdir(parents=True, exist_ok=True)
Path(destination).write_text(f"static const char {symbol}[] = {{\n" + ",\n".join(rows) + "\n};\n")
