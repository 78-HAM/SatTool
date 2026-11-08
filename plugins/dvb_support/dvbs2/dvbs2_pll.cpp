#include "dvbs2_pll.h"

namespace dvbs2
{
    S2PLLBlock::S2PLLBlock(std::shared_ptr<dsp::stream<complex_t>> input, float loop_bw, int root_code)
        : Block(input), descrambler(root_code, true)
    {
        float damping = sqrtf(2.0f) / 2.0f;
        float denom = (1.0 + 2.0 * damping * loop_bw + loop_bw * loop_bw);

        alpha = (4 * damping * loop_bw) / denom;
        beta = (4 * loop_bw * loop_bw) / denom;
        constellation_pilots.make_lut(256);
    }

    S2PLLBlock::~S2PLLBlock()
    {
    }

    void S2PLLBlock::work()
    {
        int nsamples = input_stream->read();
        if (nsamples <= 0)
        {
            input_stream->flush();
            return;
        }

        descrambler.reset();
        for (int i = 0; i < (frame_slot_count + 1) * 90 + pilot_cnt * 36; i++)
        {
            tmp_val = input_stream->readBuf[i] * complex_t(cosf(-phase), sinf(-phase));

            float error = 0;
            if (i >= 90)
            {
                // Pilot blocks are always QPSK symbols, even when the data
                // constellation is 8PSK/APSK.  They are inserted after each
                // 16 data slots (1,440 symbols), with no block after the
                // final data slot.  Using the data constellation here makes
                // the phase detector biased on APSK pilots and can prevent
                // the carrier loop from locking on S2X frames.
                bool pilot_symbol = false;
                if (pilots && pilot_cnt > 0)
                {
                    const int data_position = i - 90;
                    const int pilot_period = 16 * 90 + 36;
                    const int pilot_block = data_position / pilot_period;
                    const int within_period = data_position % pilot_period;
                    pilot_symbol = pilot_block < pilot_cnt && within_period >= 16 * 90;
                }

                // Unlike PSK and the other APSK mappings, 8APSK is not
                // invariant under the scrambler's quarter-turn rotations.
                // Make phase decisions in the unscrambled constellation;
                // keep the output scrambled for S2BBToSoft to process.
                complex_t decision_sample = descrambler.descramble(tmp_val);
                if (pilot_symbol)
                    constellation_pilots.demod_soft_lut(decision_sample, nullptr, &error);
                else
                    constellation->demod_soft_lut(decision_sample, nullptr, &error);
                output_stream->writeBuf[i] = tmp_val;
            }
            else
            {
                if (i < 26) // Use known symbols for SOF
                    error = (tmp_val * sof.symbols[i].conj()).arg();
                else // And also use known Modcod and config to synchronize onto the PLS
                    error = (tmp_val * pls.symbols[pls_code][i - 26].conj()).arg();

                // Convert the PL header to the common 45-degree BPSK basis
                // used by S2BBToSoft. S2X signals select the alternate
                // alternating basis after SOF (modcod bit 7).
                const bool s2x_header = (pls_code & 0x80) != 0 && i >= 26;
                if (s2x_header)
                    output_stream->writeBuf[i] = (i & 1) ? complex_t(-tmp_val.real, -tmp_val.imag)
                                                        : complex_t(tmp_val.imag, -tmp_val.real);
                else
                    output_stream->writeBuf[i] = (i & 1) ? complex_t(-tmp_val.real, tmp_val.imag) : complex_t(tmp_val.imag, tmp_val.real);
            }

            // Compute new freq and phase.
            freq += beta * error;
            phase += freq + alpha * error;

            // Wrap phase
            while (phase > (2 * M_PI))
                phase -= 2 * M_PI;
            while (phase < (-2 * M_PI))
                phase += 2 * M_PI;

            // Clamp freq
            if (freq > 1.0)
                freq = 1.0;
            if (freq < -1.0)
                freq = -1.0;
        }

        input_stream->flush();
        output_stream->swap(nsamples);
    }
}
