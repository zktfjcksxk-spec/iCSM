#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
if [[ "$(uname -s)" != Linux || "$(uname -m)" != x86_64 ]]; then
  echo '需要 Linux x86_64；此包不能在 macOS 或 ARM64 上运行。' >&2
  exit 1
fi
if ! python3 -c 'from pathlib import Path; import sys; sys.exit(0 if "cx16" in Path("/proc/cpuinfo").read_text().split() else 1)'; then
  echo 'CPU 需要支持 CMPXCHG16B（cx16）；虚拟机也需要开放此功能。' >&2
  exit 1
fi
if [[ ! -x ./csgo_dedicated ]]; then
  echo '缺少 csgo_dedicated 或文件没有执行权限。' >&2
  exit 1
fi
export LD_LIBRARY_PATH="$PWD/bin${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
for module in ./csgo_dedicated ./bin/*.so; do
  if ! dependencies="$(ldd "$module" 2>&1)"; then
    printf '%s\n' "$dependencies" >&2
    echo "无法检查模块：$module" >&2
    exit 1
  fi
  if [[ "$dependencies" == *'not found'* ]]; then
    printf '%s\n' "$dependencies" >&2
    echo '缺少运行库。请安装 README.txt 列出的 Ubuntu 依赖。' >&2
    exit 1
  fi
done
echo 'Linux x86_64 与动态库依赖检查通过。'
