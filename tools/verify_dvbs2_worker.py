#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugins/dvb_support/dvbs2/dvbs2_pl_sync_v2.h"
PLL_HEADER = ROOT / "plugins/dvb_support/dvbs2/dvbs2_pll_v2.h"
PLL_SOURCE = ROOT / "plugins/dvb_support/dvbs2/dvbs2_pll_v2.cpp"
MODULE_SOURCE = ROOT / "plugins/dvb_support/dvbs2/module_dvbs2_demod.cpp"
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
assert "void S2PLLBlockV2::estimate_frame_carrier" in pll_source, (
    "the improved DVB-S2 PLL must provide frame-level carrier estimation"
)
assert "estimate_frame_carrier(input_stream->readBuf, count)" in pll_source, (
    "the improved DVB-S2 PLL must apply look-ahead carrier estimation per frame"
)

module_source = MODULE_SOURCE.read_text(encoding="utf-8")
assert "pll_freq / final_sps" in module_source, (
    "PLL frequency must be converted from radians per symbol to radians per sample"
)
assert "rad_to_hz(current_freq, final_samplerate)" in module_source, (
    "displayed frequency must use the same per-sample unit as FreqShiftBlock"
)

print("Improved DVB-S2 worker and carrier acquisition lifecycles verified")
