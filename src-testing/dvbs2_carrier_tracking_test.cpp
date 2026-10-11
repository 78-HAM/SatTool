#include "dvbs2/dvbs2_pll_v2.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

// A changing upstream rotator makes the residual carrier move between
// frames. The last frame's frequency must not pin a wrong pilot-cycle branch.
static int test_tracking(bool weak_qpsk)
{
    const int slots = weak_qpsk ? 360 : 240;
    const int size = (slots + 1) * 90 + (slots - 1) / 16 * 36;
    const int count = size + 90;
    constexpr int frames = 128;
    dvbs2::s2_sof sof;
    dvbs2::s2_plscodes pls;
    std::mt19937 random(weak_qpsk ? 12345 : 42);
    // Also verify continuity at Es/N0=5 dB, where a noisy header frequency
    // must not displace an accurate estimate from the complete pilot frame.
    std::normal_distribution<float> noise(0.0f, weak_qpsk ? std::sqrt(0.5f * std::pow(10.0f, -0.5f)) : 0.21f);
    auto input = std::make_shared<dsp::stream<complex_t>>();
    dvbs2::S2PLLBlockV2 pll(input, 0.005f);
    pll.frame_slot_count = slots;
    pll.pls_code = weak_qpsk ? 17 : 53;
    pll.pilots = true;
    pll.constellation = std::make_shared<dsp::constellation_t>(weak_qpsk ? dsp::QPSK : dsp::PSK8);
    const int states = 1 << pll.constellation->getBitsCnt();
    pll.update();
    pll.start();
    std::vector<complex_t> clean(count);
    int wrong_branches = 0;
    int symbol_errors = 0;
    int data_symbols = 0;
    double frequency_error = 0;
    for (int frame = 0; frame < frames; ++frame)
    {
        dvbs2::S2Scrambling scrambling;
        scrambling.reset();
        int physical = 90;
        for (int slot = 0; slot < slots; ++slot)
        {
            if (slot > 0 && slot % 16 == 0)
                for (int p = 0; p < 36; ++p)
                {
                    complex_t pilot(0.70710678f, 0.70710678f);
                    clean[physical++] = scrambling.scramble(pilot);
                }
            for (int i = 0; i < 90; ++i)
            {
                complex_t symbol = pll.constellation->mod(random() % states);
                if (weak_qpsk)
                    symbol = symbol / symbol.norm();
                clean[physical++] = scrambling.scramble(symbol);
            }
        }
        const float carrier = weak_qpsk ? 0.001f : ((frame / 16) % 2 ? -0.0100f : 0.0264f);
        for (int i = 0; i < count; ++i)
        {
            if (i < 26 || (i >= size && i < size + 26))
                clean[i] = sof.symbols[i % size];
            else if (i < 90 || i >= size)
                clean[i] = pls.symbols[pll.pls_code][i % size - 26];
            const float phase = 0.7f + carrier * i;
            input->writeBuf[i] = clean[i] * complex_t(cosf(phase), sinf(phase)) + complex_t(noise(random), noise(random));
        }
        input->swap(count);
        if (pll.output_stream->read() != size)
        {
            std::fprintf(stderr, "FAIL: matching pilot frame was rejected\n");
            pll.output_stream->flush();
            pll.stop();
            return 1;
        }
        const float error = std::abs(pll.getFreq() - carrier);
        frequency_error += error;
        wrong_branches += error > 0.001f;
        scrambling.reset();
        dvbs2::S2Scrambling expected_scrambler;
        expected_scrambler.reset();
        for (int i = 90; i < size; ++i)
        {
            complex_t actual = scrambling.descramble(pll.output_stream->readBuf[i]);
            complex_t expected = expected_scrambler.descramble(clean[i]);
            const int data_position = i - 90;
            if (data_position >= 1440 && data_position % 1476 >= 1440)
                continue;
            auto nearest = [&](complex_t value)
            {
                int best = 0;
                float distance = 1e9f;
                for (int state = 0; state < states; ++state)
                {
                    auto difference = value - pll.constellation->mod(state);
                    const float candidate = difference.real * difference.real + difference.imag * difference.imag;
                    if (candidate < distance)
                    {
                        distance = candidate;
                        best = state;
                    }
                }
                return best;
            };
            symbol_errors += nearest(actual) != nearest(expected);
            ++data_symbols;
        }
        pll.output_stream->flush();
    }
    pll.stop();
    std::printf("%s: wrong_branches=%d/%d mean_frequency_error=%.7f symbol_error_rate=%.5f\n",
                weak_qpsk ? "5 dB QPSK continuity" : "8PSK frequency steps",
                wrong_branches, frames, frequency_error / frames, double(symbol_errors) / data_symbols);
    if (wrong_branches != 0 || frequency_error / frames > 0.0002 || double(symbol_errors) / data_symbols > 0.1)
    {
        std::fprintf(stderr, "FAIL: PLL must follow the changing residual carrier without pilot-cycle slips\n");
        return 1;
    }
    return 0;
}

int main()
{
    const int tracking = test_tracking(false);
    const int continuity = test_tracking(true);
    return tracking || continuity;
}
