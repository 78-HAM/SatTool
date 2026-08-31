#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugins/dvb_support/dvbs2/dvbs2_pl_sync_v2.h"
PLL_HEADER = ROOT / "plugins/dvb_support/dvbs2/dvbs2_pll_v2.h"
PLL_SOURCE = ROOT / "plugins/dvb_support/dvbs2/dvbs2_pll_v2.cpp"
source = HEADER.read_text(encoding="utf-8")

assert "while (should_run2" in source, (
    "the improved DVB-S2 synchronizer worker is not persistent; "
    "without a loop it emits exactly one frame and exits"
)
assert "std::thread(&S2PLSyncBlockV2::run2, this)" in source, (
    "the improved DVB-S2 synchronizer must start its persistent worker entry point"
)

pll_header = PLL_HEADER.read_text(encoding="utf-8")
pll_source = PLL_SOURCE.read_text(encoding="utf-8")
assert "bool coarse_acquired = false" in pll_header, (
    "the improved DVB-S2 PLL must remember when coarse carrier acquisition is complete"
)
assert "if (!coarse_acquired" in pll_source, (
    "coarse carrier acquisition must not overwrite the tracking loop on every frame"
)
assert "coarse_acquired = true" in pll_source, (
    "the improved DVB-S2 PLL never records successful coarse acquisition"
)

print("Improved DVB-S2 worker and carrier acquisition lifecycles verified")
