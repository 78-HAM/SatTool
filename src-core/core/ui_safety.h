#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace satdump::ui_safety
{
    inline float effectiveScale(float device_scale, float manual_scale)
    {
        if (!std::isfinite(device_scale) || device_scale <= 0.0f)
            device_scale = 1.0f;
        if (!std::isfinite(manual_scale) || manual_scale <= 0.0f)
            manual_scale = 1.0f;
        return std::clamp(device_scale * manual_scale, 0.5f, 3.0f);
    }

    inline float fittedInputWidth(float available_width, float reserved_width)
    {
        if (!std::isfinite(available_width) || !std::isfinite(reserved_width))
            return 1.0f;
        return std::max(1.0f, available_width - std::max(0.0f, reserved_width));
    }

    inline float labelColumnWidth(float available_width, float scale)
    {
        if (!std::isfinite(available_width) || available_width <= 0.0f)
            return 1.0f;
        if (!std::isfinite(scale) || scale <= 0.0f)
            scale = 1.0f;
        return std::max(1.0f, std::min(available_width * 0.42f, 220.0f * scale));
    }

    inline bool validIndex(int index, std::size_t size)
    {
        return index >= 0 && static_cast<std::size_t>(index) < size;
    }

    inline bool needsUtf8LocaleFallback(const char *locale_name)
    {
        return locale_name == nullptr || std::strcmp(locale_name, "C") == 0 || std::strcmp(locale_name, "POSIX") == 0;
    }
} // namespace satdump::ui_safety
