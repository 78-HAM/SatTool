#pragma once

#include "common/dsp/block.h"
#include "s2_defs.h"
#include "common/dsp/demod/constellation.h"
#include "codings/dvb-s2/s2_scrambling.h"
#include <algorithm>
#include <limits>

namespace dvbs2
{
    class S2PLLBlockV2 : public dsp::Block<complex_t, complex_t>
    {
    private:
        float phase = 0.0f;
        float freq = 0.0f;
        float alpha;
        float beta;
        float loop_bw;
        bool coarse_acquired = false;
        s2_sof sof;
        s2_plscodes pls;
        S2Scrambling scrambling;
        void work();
        void coarse_lock_header(const complex_t *samples);
        int pilot_cnt = 0;

        void update_loop(float error)
        {
            freq += beta * error;
            phase += freq + alpha * error;
            while (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
            while (phase < -2.0f * M_PI) phase += 2.0f * M_PI;
            const float max_freq = std::max(0.25f, loop_bw * 12.0f);
            freq = std::max(-max_freq, std::min(max_freq, freq));
        }

    public:
        int pls_code = 0;
        int frame_slot_count = 0;
        bool pilots = false;
        std::shared_ptr<dsp::constellation_t> constellation;
        dsp::constellation_t constellation_pilots = dsp::constellation_t(dsp::QPSK);

        S2PLLBlockV2(std::shared_ptr<dsp::stream<complex_t>> input, float loop_bw);
        ~S2PLLBlockV2();
        void update();
        float getFreq() { return freq; }
    };
}
