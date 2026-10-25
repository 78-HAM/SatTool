#pragma once

#include "common/dsp/block.h"
#include "s2_defs.h"
#include "common/dsp/demod/constellation.h"

/*
DVB-S2 PLL, meant to frequency & phase-recover
a synchronized DVB-S2 frame.
*/
namespace dvbs2
{
    class S2PLLBlock : public dsp::Block<complex_t, complex_t>
    {
    private:
        float phase = 0, freq = 0;
        float alpha, beta;

        s2_sof sof;
        s2_plscodes pls;

        complex_t tmp_val;

        void work();

        int pilot_cnt = 0;

    public:
        int pls_code;
        int frame_slot_count;
        bool pilots;

        void update()
        {
            // A 36-symbol pilot block follows every 16 slots of 90 data symbols,
            // i.e. one pilot block per 1440 data symbols.
            pilot_cnt = pilots ? (frame_slot_count / 16) : 0;
        }

        std::shared_ptr<dsp::constellation_t> constellation;
        dsp::constellation_t constellation_pilots = dsp::constellation_t(dsp::QPSK);

    public:
        S2PLLBlock(std::shared_ptr<dsp::stream<complex_t>> input, float loop_bw);
        ~S2PLLBlock();

        float getFreq() { return freq; }
    };
}