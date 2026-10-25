#pragma once

#include "common/dsp/block.h"
#include <memory>

namespace dsp
{
    /*
    De-emphasis filter, as used by analog FM demodulators.

    This is a one-pole IIR low-pass (y[n] = a*x[n] + (1-a)*y[n-1]) whose time
    constant tau sets the corner where the emphasis applied by the transmitter
    is undone.
    */
    template <typename T>
    class DeEmphasisBlock : public Block<T, T>
    {
    private:
        // Filter state, carried across work() calls
        float lastSamp = 0;
        float lastSampImag = 0;
        float lastSampReal = 0;

        double tau;
        void work();

        float alpha;
        double quad_sampl_rate = 0;

    public:
        DeEmphasisBlock(std::shared_ptr<dsp::stream<T>> input, double quad_sampl_rate, double tau);
        ~DeEmphasisBlock();
    };
}
