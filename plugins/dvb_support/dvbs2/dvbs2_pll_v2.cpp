#include "dvbs2_pll_v2.h"

#include <limits>

namespace dvbs2
{
    S2PLLBlockV2::S2PLLBlockV2(std::shared_ptr<dsp::stream<complex_t>> input, float bw)
        : Block(input), loop_bw(std::max(1.0e-5f, bw))
    {
        const float damping = sqrtf(2.0f) / 2.0f;
        const float denom = 1.0f + 2.0f * damping * loop_bw + loop_bw * loop_bw;
        alpha = (4.0f * damping * loop_bw) / denom;
        beta = (4.0f * loop_bw * loop_bw) / denom;
    }

    S2PLLBlockV2::~S2PLLBlockV2() {}

    void S2PLLBlockV2::update()
    {
        pilot_cnt = pilots ? (frame_slot_count - 1) / 16 : 0;
    }

    void S2PLLBlockV2::work()
    {
        const int nsamples = input_stream->read();
        if (nsamples <= 0)
        {
            input_stream->flush();
            return;
        }

        const int expected = (frame_slot_count + 1) * 90 + pilot_cnt * 36;
        const int count = std::min(nsamples, expected);
        scrambling.reset();
        int data_seen = 0;
        int next_pilot_data = pilots ? 16 * 90 : std::numeric_limits<int>::max();

        for (int i = 0; i < count; ++i)
        {
            complex_t rotated = input_stream->readBuf[i] * complex_t(cosf(-phase), sinf(-phase));
            complex_t phase_sample = rotated;
            float error = 0.0f;

            if (i < 26)
            {
                error = (rotated * sof.symbols[i].conj()).arg();
                output_stream->writeBuf[i] = (i & 1) ? complex_t(-rotated.real, rotated.imag) : complex_t(rotated.imag, rotated.real);
            }
            else if (i < 90)
            {
                error = (rotated * pls.symbols[pls_code][i - 26].conj()).arg();
                output_stream->writeBuf[i] = (i & 1) ? complex_t(-rotated.real, rotated.imag) : complex_t(rotated.imag, rotated.real);
            }
            else
            {
                const bool is_pilot = data_seen == next_pilot_data;
                if (is_pilot)
                {
                    // Pilots are part of the physical scrambling sequence but
                    // are not part of the soft-symbol output.
                    for (int p = 0; p < 36 && i + p < count; ++p)
                    {
                        complex_t pilot_rotated = input_stream->readBuf[i + p] * complex_t(cosf(-phase), sinf(-phase));
                        complex_t pilot_sample = pilot_rotated;
                        complex_t pilot_descrambled = scrambling.descramble(pilot_sample);
                        error = (pilot_descrambled * complex_t(0.70710678f, 0.70710678f).conj()).arg();
                        output_stream->writeBuf[i + p] = pilot_rotated;
                        update_loop(error);
                    }
                    i += 35;
                    next_pilot_data += 16 * 90;
                    continue;
                }

                complex_t descrambled = scrambling.descramble(phase_sample);
                if (constellation)
                    constellation->demod_soft_improved(descrambled, nullptr, 0.45f, 1.0f, &error);
                output_stream->writeBuf[i] = rotated;
                ++data_seen;
            }
            update_loop(error);
        }

        input_stream->flush();
        output_stream->swap(count);
    }
}
