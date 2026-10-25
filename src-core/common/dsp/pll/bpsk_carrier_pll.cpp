#include "bpsk_carrier_pll.h"
#include "common/dsp/utils/fast_trig.h"
#include <cmath>

#define M_TWOPI (2 * M_PI)

#ifdef __APPLE__
#define sincosf __sincosf
#endif

#if defined(_WIN32) && !defined(SATDUMP_HAVE_SINCOSF)
// MinGW's <math.h> already declares sincosf(float, float*, float*), and GCC
// lowers sinf+cosf pairs into calls to it. Defining a global sincosf here
// therefore recursed into itself forever (stack overflow at the first
// sample). Provide a file-local helper under a distinct name instead.
static inline void satdump_sincosf(float x, float *sinx, float *cosx)
{
    *sinx = sinf(x);
    *cosx = cosf(x);
}
#else
#define satdump_sincosf sincosf
#endif

namespace dsp
{
    BPSKCarrierPLLBlock::BPSKCarrierPLLBlock(std::shared_ptr<dsp::stream<complex_t>> input, float alpha, float beta, float max_offset) : Block(input), alpha(alpha), beta(beta), max_offset(max_offset)
    {
    }

    void BPSKCarrierPLLBlock::work()
    {
        int nsamples = input_stream->read();
        if (nsamples <= 0)
        {
            input_stream->flush();
            return;
        }

        for (int i = 0; i < nsamples; i++)
        {
            // Generate and mix out carrier
            float re, im;
            satdump_sincosf(phase, &im, &re);
            output_stream->writeBuf[i] = (input_stream->readBuf[i] * complex_t(re, -im)).imag;

            // Compute error and wrap
            float error = fast_atan2f(input_stream->readBuf[i].imag, input_stream->readBuf[i].real) - phase;
            while (error < -M_PI)
                error += M_TWOPI;
            while (error > M_PI)
                error -= M_TWOPI;

            // Clip frequency
            freq = branchless_clip(freq + error * beta, max_offset);

            // Wrap phase
            phase = phase + error * alpha + freq;
            while (phase < -M_PI)
                phase += M_TWOPI;
            while (phase > M_PI)
                phase -= M_TWOPI;
        }

        input_stream->flush();
        output_stream->swap(nsamples);
    }
}