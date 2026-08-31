#include "dvbs2_bb_to_soft_v2.h"

#include <algorithm>
#include <cmath>

namespace dvbs2
{
    S2BBToSoftV2::S2BBToSoftV2(std::shared_ptr<dsp::stream<complex_t>> input)
        : Block(input)
    {
        soft_slots_buffer = new int8_t[64800];
    }

    S2BBToSoftV2::~S2BBToSoftV2()
    {
        delete[] soft_slots_buffer;
    }

    int S2BBToSoftV2::checkSyncMarker(uint64_t marker, uint64_t test) const
    {
        uint64_t value = marker ^ test;
        int errors = 0;
        while (value != 0)
        {
            value &= value - 1;
            ++errors;
        }
        return errors;
    }

    void S2BBToSoftV2::work()
    {
        const int nsamples = input_stream->read();
        if (nsamples <= 0)
        {
            input_stream->flush();
            return;
        }

        uint64_t plheader = 0;
        for (int y = 0; y < 64; ++y)
        {
            const complex_t rotated = input_stream->readBuf[26 + y] * complex_t(cosf(-M_PI / 4.0f), sinf(-M_PI / 4.0f));
            plheader = (plheader << 1) | (rotated.real > 0 ? 0ULL : 1ULL);
        }

        int best_header = 0;
        int header_errors = 64;
        for (int c = 0; c < s2_plscodes::COUNT; ++c)
        {
            const int errors = checkSyncMarker(pls.codewords[c], plheader);
            if (errors < header_errors)
            {
                header_errors = errors;
                best_header = c;
            }
        }

        detect_modcod = best_header >> 2;
        detect_shortframes = (best_header & 2) != 0;
        detect_pilots = (best_header & 1) != 0;

        descrambler.reset();
        const int bits_per_symbol = constellation ? constellation->getBitsCnt() : 0;
        int data_symbol = 0;
        int physical = 90;
        const int max_data_symbols = frame_slot_count * 90;

        for (int slot = 0; slot < frame_slot_count; ++slot)
        {
            if (pilots && slot > 0 && (slot % 16) == 0)
            {
                // Consume the 36 scrambled pilot symbols so that the
                // physical-layer scrambler stays aligned for later slots.
                for (int p = 0; p < 36 && physical < nsamples; ++p)
                    (void)descrambler.descramble(input_stream->readBuf[physical++]);
            }

            for (int symbol = 0; symbol < 90 && data_symbol < max_data_symbols && physical < nsamples; ++symbol)
            {
                complex_t sample = input_stream->readBuf[physical++];
                sample = descrambler.descramble(sample);
                if (constellation)
                    constellation->demod_soft_improved(sample, &soft_slots_buffer[data_symbol * bits_per_symbol], noise_sigma, soft_scale);
                ++data_symbol;
            }
        }

        if (data_symbol != max_data_symbols)
        {
            input_stream->flush();
            return;
        }

        deinterleaver->deinterleave(soft_slots_buffer, output_stream->writeBuf);
        input_stream->flush();
        output_stream->swap(max_data_symbols * bits_per_symbol);
    }
}
