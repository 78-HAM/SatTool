#include "s2_deinterleaver.h"

namespace dvbs2
{
    S2Deinterleaver::S2Deinterleaver(dvbs2_constellation_t constellation,
                                     dvbs2_framesize_t framesize,
                                     dvbs2_code_rate_t rate)
    {
        frame_length = framesize == FECFRAME_NORMAL ? FRAME_SIZE_NORMAL
                      : framesize == FECFRAME_MEDIUM ? FRAME_SIZE_MEDIUM
                                                     : FRAME_SIZE_SHORT;

        permutation.fill(0);
        if (constellation == MOD_QPSK)
        {
            mod_bits = 2;
            rows = frame_length / mod_bits;
            permutation = {1, 0, 0, 0, 0, 0};
        }
        else
        {
            switch (constellation)
            {
            case MOD_8PSK:
            case MOD_8APSK:
                mod_bits = 3;
                break;
            case MOD_16APSK:
            case MOD_8_8APSK:
                mod_bits = 4;
                break;
            case MOD_32APSK:
            case MOD_4_12_16APSK:
            case MOD_4_8_4_16APSK:
                mod_bits = 5;
                break;
            case MOD_BPSK:
            case MOD_BPSK_SF2:
                mod_bits = 1;
                break;
            default:
                mod_bits = 2;
                break;
            }
            rows = frame_length / mod_bits;
            for (int i = 0; i < mod_bits; ++i)
                permutation[i] = i;

            if (constellation == MOD_8PSK && rate == C3_5)
                permutation = {2, 1, 0, 0, 0, 0};
            // DVB-S2X 8PSK rates 25/36, 13/18 and the 7/15..26/45 family
            // use the 102 column order.  The 23/36 profile remains the
            // standard 012 order in EN 302 307-2; confusing it with 102
            // feeds the LDPC decoder with interleaved bit planes.
            if (constellation == MOD_8PSK &&
                (rate == C25_36 || rate == C13_18 ||
                 rate == C7_15 || rate == C8_15 || rate == C26_45))
                permutation = {1, 0, 2, 0, 0, 0};

            if (constellation == MOD_16APSK)
            {
                if (rate == C26_45)
                    permutation = framesize == FECFRAME_SHORT ? std::array<int, 6>{2, 1, 3, 0, 0, 0}
                                                               : std::array<int, 6>{3, 2, 0, 1, 0, 0};
                else if (rate == C3_5)
                    permutation = framesize == FECFRAME_SHORT ? std::array<int, 6>{3, 2, 0, 1, 0, 0}
                                                               : std::array<int, 6>{3, 2, 1, 0, 0, 0};
                else if (rate == C28_45)
                    permutation = {3, 0, 1, 2, 0, 0};
                else if (rate == C23_36 || rate == C13_18)
                    permutation = {3, 0, 2, 1, 0, 0};
                else if (rate == C25_36)
                    permutation = {2, 3, 1, 0, 0, 0};
                else if (rate == C7_15 || rate == C8_15)
                    permutation = {2, 1, 0, 3, 0, 0};
                else if (rate == C140_180)
                    permutation = {3, 2, 1, 0, 0, 0};
                else if (rate == C154_180)
                    permutation = {0, 3, 2, 1, 0, 0};
            }
            else if (constellation == MOD_8_8APSK)
            {
                if (rate == C90_180)
                    permutation = {3, 2, 1, 0, 0, 0};
                else if (rate == C96_180)
                    permutation = {2, 3, 1, 0, 0, 0};
                else if (rate == C100_180)
                    permutation = {2, 3, 0, 1, 0, 0};
            }
            else if (constellation == MOD_4_12_16APSK)
            {
                permutation = framesize == FECFRAME_SHORT && rate == C2_3
                                   ? std::array<int, 6>{4, 1, 2, 3, 0, 0}
                                   : framesize == FECFRAME_SHORT && rate == C32_45
                                   ? std::array<int, 6>{1, 0, 4, 2, 3, 0}
                                   : std::array<int, 6>{2, 1, 4, 3, 0, 0};
            }
            else if (constellation == MOD_4_8_4_16APSK)
            {
                permutation = rate == C140_180
                                   ? std::array<int, 6>{4, 0, 2, 1, 3, 0}
                                   : std::array<int, 6>{4, 0, 3, 1, 2, 0};
            }
        }

        for (int i = 0; i < mod_bits; ++i)
            columns[i] = permutation[i] * rows;
    }

    S2Deinterleaver::~S2Deinterleaver() = default;

    void S2Deinterleaver::deinterleave(int8_t *input, int8_t *output)
    {
        if (mod_bits == 2 && permutation[0] == 1 && permutation[1] == 0)
        {
            for (int i = 0; i < rows; ++i)
            {
                output[i * 2] = input[i * 2 + 1];
                output[i * 2 + 1] = input[i * 2];
            }
            return;
        }

        int in = 0;
        for (int row = 0; row < rows; ++row)
            for (int bit = 0; bit < mod_bits; ++bit)
                output[columns[bit] + row] = input[in++];
    }

    void S2Deinterleaver::interleave(uint8_t *input, uint8_t *output)
    {
        if (mod_bits == 2 && permutation[0] == 1 && permutation[1] == 0)
        {
            for (int i = 0; i < rows; ++i)
            {
                output[i * 2] = input[i * 2 + 1];
                output[i * 2 + 1] = input[i * 2];
            }
            return;
        }

        int out = 0;
        for (int row = 0; row < rows; ++row)
            for (int bit = 0; bit < mod_bits; ++bit)
                output[out++] = input[columns[bit] + row];
    }
}
