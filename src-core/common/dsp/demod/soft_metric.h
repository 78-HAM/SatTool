#pragma once

#include <cstdint>

namespace dsp
{
    constexpr float DEFAULT_DVBS2_LLR_SCALE = 5.0f;

    inline int8_t quantize_soft_metric(float value)
    {
        if (value != value)
            return 0;
        if (value > 127.0f)
            return 127;
        if (value < -127.0f)
            return -127;
        return static_cast<int8_t>(value);
    }
}
