#include "s2_scrambling.h"
#include <stdexcept>

/*
Adapted from LeanDVB
*/

namespace dvbs2
{
    uint32_t S2Scrambling::gold_to_root(int gold_code)
    {
        if (gold_code < 0 || gold_code > 262141)
            throw std::invalid_argument("gold_code must be in the range 0..262141");
        uint32_t state = 1;
        for (int i = 0; i < gold_code; ++i)
            state = (((state ^ (state >> 7)) & 1) << 17) | (state >> 1);
        return state;
    }

    S2Scrambling::S2Scrambling(int code, bool root_state)
    {
        if (root_state && (code < 0 || code > 262143))
            throw std::invalid_argument("root_code must be in the range 0..262143");
        uint32_t stx = root_state ? (code == 0 ? 1 : static_cast<uint32_t>(code)) : gold_to_root(code);
        uint32_t sty = 0x3ffff;
        // First half of sequence is LSB of scrambling angle
        for (int i = 0; i < 131072; ++i)
        {
            int zn = (stx ^ sty) & 1;
            Rn[i] = zn;
            stx = lfsr_x(stx);
            sty = lfsr_y(sty);
        }
        // Second half is MSB
        for (int i = 0; i < 131072; ++i)
        {
            int zn = (stx ^ sty) & 1;
            Rn[i] |= zn << 1;
            stx = lfsr_x(stx);
            sty = lfsr_y(sty);
        }
    }

    S2Scrambling::~S2Scrambling()
    {
    }

    void S2Scrambling::reset()
    {
        pos = 0;
    }

    void S2Scrambling::skip(int symbols)
    {
        pos += symbols;
    }

    complex_t S2Scrambling::descramble(complex_t &p)
    {
        r = Rn[pos++];

        switch (r)
        {
        case 3:
            res.real = -p.imag;
            res.imag = p.real;
            break;
        case 2:
            res.real = -p.real;
            res.imag = -p.imag;
            break;
        case 1:
            res.real = p.imag;
            res.imag = -p.real;
            break;
        default:
            res = p;
        }

        return res;
    }

    complex_t S2Scrambling::scramble(complex_t &p)
    {
        r = Rn[pos++];

        switch (r)
        {
        case 3:
            res.imag = -p.real;
            res.real = p.imag;
            break;
        case 2:
            res.real = -p.real;
            res.imag = -p.imag;
            break;
        case 1:
            res.imag = p.real;
            res.real = -p.imag;
            break;
        default:
            res = p;
        }

        return res;
    }
}
