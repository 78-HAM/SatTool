#pragma once

#include <cstdint>
#include <cmath>
#include "common/dsp/complex.h"

/*
Stuff from LeanDVB, which I could for sure have re-written...
But why? Already did what I needed :-)
*/
namespace dvbs2
{
    static const int cstln_amp = 1; // 75;

    inline int s2_pilot_count(int slot_count)
    {
        int count = slot_count / 16;
        if (slot_count > 0 && (slot_count % 16) == 0)
            --count; // No pilot block follows the final data slot.
        return count > 0 ? count : 0;
    }

    struct s2_sof
    {
        static const uint32_t VALUE = 0x18d2e82;
        static const uint32_t MASK = 0x3ffffff;
        static const int LENGTH = 26;

        complex_t symbols[LENGTH];

        s2_sof()
        {
            for (int s = 0; s < LENGTH; ++s)
            {
                bool bit = ((VALUE >> (LENGTH - 1 - s)) & 1);
                int angle = bit * 2 + (s & 1); // pi/2-BPSK
                symbols[s].real = cstln_amp * cosf(M_PI / 4 + 2 * M_PI * angle / 4);
                symbols[s].imag = cstln_amp * sinf(M_PI / 4 + 2 * M_PI * angle / 4);
            }
        }
    };

    struct s2_plscodes
    {
        // DVB-S2 has a 7-bit PLS input (MODCOD[4:0], frame type and pilots).
        // DVB-S2X uses an 8-bit extended input.  Keep both code spaces in
        // the same table: indices 0..127 retain their DVB-S2 meaning, while
        // indices 128..255 use the S2X generator matrix.
        static const int COUNT = 256;
        static const int LENGTH = 64;

        uint64_t codewords[COUNT];
        complex_t symbols[COUNT][LENGTH];

        s2_plscodes()
        {
            uint32_t G[6] = {0x55555555,
                             0x33333333,
                             0x0f0f0f0f,
                             0x00ff00ff,
                             0x0000ffff,
                             0xffffffff};
            uint32_t Gx[7] = {0x90ac2ddd,
                              0x55555555,
                              0x33333333,
                              0x0f0f0f0f,
                              0x00ff00ff,
                              0x0000ffff,
                              0xffffffff};

            for (int index = 0; index < COUNT; ++index)
            {
                uint32_t y = 0;
                if (index < 128)
                {
                    for (int row = 0; row < 6; ++row)
                        if ((index >> (6 - row)) & 1)
                            y ^= G[row];
                }
                else
                {
                    for (int row = 0; row < 7; ++row)
                        if ((index >> (7 - row)) & 1)
                            y ^= Gx[row];
                }

                uint64_t code = 0;
                for (int bit = 31; bit >= 0; --bit)
                {
                    int yi = (y >> bit) & 1;
                    if (index & 1)
                        code = (code << 2) | (yi << 1) | (yi ^ 1);
                    else
                        code = (code << 2) | (yi << 1) | yi;
                }

                // Scrambling
                code ^= SCRAMBLING;

                // Store precomputed codeword.
                codewords[index] = code;

                // Also store as symbols.  DVB-S2X extended PLS codewords
                // use the alternate PLHEADER BPSK basis selected by
                // modcod bit 7 (see gr-dvbs2 physical_cc).  SOF has already
                // been removed from this array, so S2X starts with the
                // opposite basis at local symbol zero (PL symbol 26).
                for (int i = 0; i < LENGTH; ++i)
                {
                    int yi = (code >> (LENGTH - 1 - i)) & 1;
                    const int sign = 1 - 2 * yi;
                    if (index >= 128)
                    {
                        // The S2X PL header uses m_bpsk[(i & 1) + 2].
                        // Rows 2 and 3 are (-,+)/(+,-) and (-,-)/(+,+),
                        // respectively, for the two possible coded bits.
                        symbols[index][i].real = -cstln_amp * sign / sqrtf(2);
                        symbols[index][i].imag = (i & 1) ? -cstln_amp * sign / sqrtf(2)
                                                         : cstln_amp * sign / sqrtf(2);
                    }
                    else
                    {
                        const int nyi = yi ^ (i & 1);
                        symbols[index][i].real = cstln_amp * (1 - 2 * nyi) / sqrtf(2);
                        symbols[index][i].imag = cstln_amp * sign / sqrtf(2);
                    }
                }
            }
        }

        static const uint64_t SCRAMBLING = 0x719d83c953422dfa;
    }; // s2_plscodes
}
