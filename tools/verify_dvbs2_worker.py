#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugins/dvb_support/dvbs2/dvbs2_pl_sync_v2.h"
source = HEADER.read_text(encoding="utf-8")

assert "while (should_run2" in source, (
    "the improved DVB-S2 synchronizer worker is not persistent; "
    "without a loop it emits exactly one frame and exits"
)
assert "std::thread(&S2PLSyncBlockV2::run2, this)" in source, (
    "the improved DVB-S2 synchronizer must start its persistent worker entry point"
)

print("Improved DVB-S2 synchronizer worker lifecycle verified")
