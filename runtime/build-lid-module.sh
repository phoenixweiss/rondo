#!/bin/bash
# Offline build with pinned Linux/BC sources and the audited vendor contract.
set -euo pipefail
export PATH=/opt/aarch64-nextui-linux-gnu/bin:$PATH
export KBUILD_BUILD_TIMESTAMP='2026-10-16 00:00:00 +0000' KBUILD_BUILD_USER=rondo KBUILD_BUILD_HOST=builder
cd /tmp
cp -a /work/sources/bc-1.07.1 /tmp/
cd bc-1.07.1
# Replace the build-only ed formatting step with identical Python operations.
cat > bc/fix-libmath_h <<'FIX'
python3 - <<'PYCODE'
from pathlib import Path
p=Path('libmath.h');lines=p.read_text().splitlines();lines[0]='{"'+lines[0]
lines=[line+'",' for line in lines]
lines[1:]=['"'+line for line in lines[1:]]
lines.pop();lines[-1]=lines[-1][:-1]+',0}'
p.write_text('\n'.join(lines)+'\n')
PYCODE
FIX
./configure --prefix=/tmp/bc > /work/lid-bc-build.log 2>&1
make MAKEINFO=true -j4 >> /work/lid-bc-build.log 2>&1
make MAKEINFO=true install >> /work/lid-bc-build.log 2>&1
export PATH=/tmp/bc/bin:$PATH
mkdir -p /work/lid-module
cp /src/runtime/lid-wake/rondo_lid_wake.c /src/runtime/lid-wake/Makefile /work/lid-module/
python3 /src/runtime/lid_module.py prepare /src/runtime/lid-wake /work/lid-module
cd /tmp/rondo-linux/linux-4.9.170
cp /src/runtime/lid-wake/kernel.config .config
export ARCH=arm64 CROSS_COMPILE=aarch64-nextui-linux-gnu-
make HOSTCFLAGS='-O2 -fcommon' olddefconfig modules_prepare > /work/lid-prepare.log 2>&1
cp /src/runtime/lid-wake/vendor.symvers Module.symvers
make HOSTCFLAGS='-O2 -fcommon' M=/work/lid-module modules > /work/lid-build.log 2>&1
aarch64-nextui-linux-gnu-strip --strip-debug /work/lid-module/rondo_lid_wake.ko
python3 /src/runtime/lid_module.py verify /src/runtime/lid-wake /work/lid-module
