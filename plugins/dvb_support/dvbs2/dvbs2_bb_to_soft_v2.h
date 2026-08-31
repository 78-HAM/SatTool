#pragma once

#include "common/dsp/block.h"
#include "s2_defs.h"
#include "common/dsp/demod/constellation.h"
#include "codings/dvb-s2/s2_scrambling.h"
#include "codings/dvb-s2/s2_deinterleaver.h"
#include "utils/binary.h"

namespace dvbs2
{
    class S2BBToSoftV2 : public dsp::Block<complex_t, int8_t>
    {
    private:
        s2_plscodes pls;
        S2Scrambling descrambler;
        int8_t *soft_slots_buffer;
        void work();
        int checkSyncMarker(uint64_t marker, uint64_t test) const;

    public:
        int detect_modcod = 0;
        bool detect_shortframes = false;
        bool detect_pilots = false;
        bool pilots = false;
        int frame_slot_count = 0;
        float noise_sigma = 0.45f;
        float soft_scale = dsp::DEFAULT_DVBS2_LLR_SCALE;
        std::shared_ptr<dsp::constellation_t> constellation;
        std::shared_ptr<S2Deinterleaver> deinterleaver;

        S2BBToSoftV2(std::shared_ptr<dsp::stream<complex_t>> input);
        ~S2BBToSoftV2();
    };
}
