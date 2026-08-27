#include "core/ui_safety.h"

#include <cassert>
#include <cmath>
#include <cstddef>

int main()
{
    using namespace satdump::ui_safety;

    assert(effectiveScale(1.0f, 2.0f) == 2.0f);
    assert(effectiveScale(1.0f, 0.1f) == 0.5f);
    assert(effectiveScale(2.0f, 2.0f) == 3.0f);
    assert(effectiveScale(NAN, 1.0f) == 1.0f);
    assert(effectiveScale(1.0f, NAN) == 1.0f);

    assert(fittedInputWidth(200.0f, 30.0f) == 170.0f);
    assert(fittedInputWidth(20.0f, 30.0f) == 1.0f);
    assert(fittedInputWidth(NAN, 30.0f) == 1.0f);

    assert(labelColumnWidth(1000.0f, 1.0f) == 220.0f);
    assert(labelColumnWidth(400.0f, 1.0f) == 168.0f);
    assert(labelColumnWidth(1000.0f, 2.0f) == 420.0f);
    assert(labelColumnWidth(NAN, 1.0f) == 1.0f);

    assert(validIndex(0, std::size_t{1}));
    assert(!validIndex(-1, std::size_t{1}));
    assert(!validIndex(0, std::size_t{0}));
    assert(!validIndex(2, std::size_t{2}));

    assert(needsUtf8LocaleFallback("C"));
    assert(needsUtf8LocaleFallback("POSIX"));
    assert(!needsUtf8LocaleFallback("C.UTF-8"));
    assert(!needsUtf8LocaleFallback("en_US.UTF-8"));
}
