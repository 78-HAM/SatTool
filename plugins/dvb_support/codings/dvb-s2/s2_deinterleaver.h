#pragma once

#include <cstdint>
#include <array>
#include "common/codings/dvb-s2/dvbs2.h"

namespace dvbs2
{
    class S2Deinterleaver
    {
    private:
        int mod_bits;
        int rows;
        int frame_length;

        std::array<int, 6> columns{};
        std::array<int, 6> permutation{};

    public:
        S2Deinterleaver(dvbs2_constellation_t constellation, dvbs2_framesize_t framesize, dvbs2_code_rate_t rate);
        ~S2Deinterleaver();

        void deinterleave(int8_t *input, int8_t *output);
        void interleave(uint8_t *input, uint8_t *output);
    };
}
