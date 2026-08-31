#include "common/dsp/demod/soft_metric.h"

#include <cassert>

int main()
{
    using dsp::quantize_soft_metric;

    static_assert(dsp::DEFAULT_DVBS2_LLR_SCALE == 5.0f);
    assert(quantize_soft_metric(0.0f) == 0);
    assert(quantize_soft_metric(126.0f) == 126);
    assert(quantize_soft_metric(128.0f) == 127);
    assert(quantize_soft_metric(1000.0f) == 127);
    assert(quantize_soft_metric(-128.0f) == -127);
    assert(quantize_soft_metric(-1000.0f) == -127);
}
