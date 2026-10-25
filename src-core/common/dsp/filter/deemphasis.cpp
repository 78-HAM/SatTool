#include "common/dsp/filter/deemphasis.h"
#include <memory>
#include <type_traits>

namespace dsp
{
    template <typename T>
    DeEmphasisBlock<T>::DeEmphasisBlock(std::shared_ptr<dsp::stream<T>> input, double quad_sampl_rate, double tau) : Block<T, T>(input)
    {
        // Precompute the one-pole coefficient: alpha = dt / (tau + dt)
        float dt = 1.0f / quad_sampl_rate;
        alpha = dt / (tau + dt);
    }

    template <typename T>
    DeEmphasisBlock<T>::~DeEmphasisBlock()
    {
    }

    template <typename T>
    void DeEmphasisBlock<T>::work()
    {
        int nsamples = Block<T, T>::input_stream->read();
        if (nsamples <= 0)
        {
            Block<T, T>::input_stream->flush();
            return;
        }

        // One-pole IIR applied to the freshly read samples. The state is kept
        // in lastSamp* so the filter stays continuous across buffer boundaries.
        if constexpr (std::is_same_v<T, float>)
        {
            for (int i = 0; i < nsamples; i++)
            {
                lastSamp = (alpha * Block<T, T>::input_stream->readBuf[i]) + ((1 - alpha) * lastSamp);
                Block<T, T>::output_stream->writeBuf[i] = lastSamp;
            }
        }
        else if constexpr (std::is_same_v<T, complex_t>)
        {
            for (int i = 0; i < nsamples; i++)
            {
                const complex_t in = Block<T, T>::input_stream->readBuf[i];
                lastSampReal = (alpha * in.real) + ((1 - alpha) * lastSampReal);
                lastSampImag = (alpha * in.imag) + ((1 - alpha) * lastSampImag);
                Block<T, T>::output_stream->writeBuf[i] = complex_t(lastSampReal, lastSampImag);
            }
        }

        Block<T, T>::input_stream->flush();
        Block<T, T>::output_stream->swap(nsamples);
    }

    template class DeEmphasisBlock<complex_t>;
    template class DeEmphasisBlock<float>;
}
