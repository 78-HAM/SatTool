#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <complex>
#include "common/codings/dvb-s2/dvbs2.h"

typedef int8_t code_type;

#include "codings/dvb-s2/ldpc/simd.hh"
#include "codings/dvb-s2/ldpc/layered_decoder.hh"
#include "codings/dvb-s2/ldpc/algorithms.hh"
#include "codings/dvb-s2/ldpc/encoder.hh"

namespace dvbs2
{
#if defined(__AVX2__OFF)
    typedef SIMD<code_type, 32> simd_type;
#elif defined(__SSE4_1__)
    typedef SIMD<code_type, 16> simd_type;
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
    typedef SIMD<code_type, 16> simd_type;
#else
    typedef SIMD<code_type, 1> simd_type;
#endif

    typedef NormalUpdate<simd_type> update_type;
    typedef OffsetMinSumAlgorithm<simd_type, update_type, 2> algorithm_type;

    class BBFrameLDPCInterface
    {
    public:
        virtual ~BBFrameLDPCInterface() = default;
        virtual int dataSize() const = 0;
        virtual int decode(int8_t *frame, int max_trials) = 0;
        virtual void encode(uint8_t *frame) = 0;
    };

    class BBFrameLDPC : public BBFrameLDPCInterface
    {
    private:
        simd_type *aligned_buffer = nullptr;

        LDPCInterface *ldpc = nullptr;
        LDPCDecoder<simd_type, algorithm_type> decoder;
        LDPCEncoder<int8_t> encoder;
        void init();

    public:
        BBFrameLDPC(dvbs2_framesize_t framesize, dvbs2_code_rate_t rate);
        ~BBFrameLDPC() override;

        int dataSize() const override
        {
            return ldpc->data_len();
        }

        int decode(int8_t *frame, int max_trials) override;
        void encode(uint8_t *frame) override;

        LDPCInterface *get_instance()
        {
            return ldpc;
        }
    };

    typedef MinSumCAlgorithm<simd_type, update_type, 2> improved_algorithm_type;

    // SDRangel's DVB-S2 decoder uses the corrected min-sum check-node rule.
    // Keep it as a separate implementation so the legacy decoder remains
    // available for compatibility and A/B testing.
    class BBFrameLDPCImproved : public BBFrameLDPCInterface
    {
    private:
        simd_type *aligned_buffer = nullptr;

        LDPCInterface *ldpc = nullptr;
        LDPCDecoder<simd_type, improved_algorithm_type> decoder;
        LDPCEncoder<int8_t> encoder;

    public:
        BBFrameLDPCImproved(dvbs2_framesize_t framesize, dvbs2_code_rate_t rate);
        ~BBFrameLDPCImproved() override;

        int dataSize() const override
        {
            return ldpc->data_len();
        }

        int decode(int8_t *frame, int max_trials) override;
        void encode(uint8_t *frame) override;

        LDPCInterface *get_instance()
        {
            return ldpc;
        }
    };
}
