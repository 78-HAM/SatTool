#include "logger.h"
#include <cstdio>
#include <cstdlib>

// Standalone DSP tests use the production stream implementation and VOLK.
// Only application globals and lifecycle-error reporting are supplied here.
namespace dsp
{
    int STREAM_BUFFER_SIZE = 65536;
    int RING_BUF_SZ = 1000000;
}
std::shared_ptr<slog::Logger> logger = std::make_shared<slog::Logger>();
void slog::Logger::logf(LogLevel, std::string message, va_list)
{
    std::fprintf(stderr, "Unexpected DSP lifecycle error: %s\n", message.c_str());
    std::abort();
}
