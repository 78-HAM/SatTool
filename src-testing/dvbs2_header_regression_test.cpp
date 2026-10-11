#include "dvbs2/dvbs2_pll_v2.h"
#include "dvbs2/dvbs2_bb_to_soft_v2.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <future>
#include <random>
#include <thread>
#include <vector>

// Exercise the real PLL and soft worker, including a pilots mismatch.
// A rejected PLHEADER must not amplify payload or feed the upstream rotator.
int main()
{
    constexpr int slots = 240;
    constexpr int frame_size = (slots + 1) * 90 + (slots - 1) / 16 * 36;
    constexpr int count = frame_size + 90;
    dvbs2::s2_sof sof;
    dvbs2::s2_plscodes pls;
    dsp::constellation_t constellation(dsp::PSK8);
    std::mt19937 random(42);
    std::normal_distribution<float> noise(0.0f, 0.08f);
    std::vector<complex_t> samples(count);
    dvbs2::S2Scrambling scrambler;
    scrambler.reset();
    int physical = 90;
    for (int slot = 0; slot < slots; ++slot)
    {
        if (slot > 0 && slot % 16 == 0)
            for (int p = 0; p < 36; ++p)
            {
                complex_t pilot(0.70710678f, 0.70710678f);
                samples[physical++] = scrambler.scramble(pilot);
            }
        for (int i = 0; i < 90; ++i)
        {
            complex_t data = constellation.mod(random() % 8);
            samples[physical++] = scrambler.scramble(data);
        }
    }
    for (int i = 0; i < count; ++i)
    {
        complex_t symbol = samples[i];
        if (i < 26 || (i >= frame_size && i < frame_size + 26))
            symbol = sof.symbols[i % frame_size];
        else if (i < 90 || i >= frame_size)
            symbol = pls.symbols[53][i % frame_size - 26];
        const float phase = 0.7f + 0.0264f * i;
        samples[i] = symbol * complex_t(cosf(phase), sinf(phase)) + complex_t(noise(random), noise(random));
    }

    bool passed = true;
    for (bool pilots : {false, true})
    {
        auto input = std::make_shared<dsp::stream<complex_t>>();
        dvbs2::S2PLLBlockV2 pll(input, 0.005f);
        pll.frame_slot_count = slots;
        pll.pilots = pilots;
        pll.pls_code = (13 << 2) | int(pilots);
        pll.constellation = std::make_shared<dsp::constellation_t>(dsp::PSK8);
        pll.update();
        dvbs2::S2BBToSoftV2 soft(pll.output_stream);
        soft.frame_slot_count = slots;
        soft.pilots = pilots;
        soft.constellation = pll.constellation;
        soft.deinterleaver = std::make_shared<dvbs2::S2Deinterleaver>(dvbs2::MOD_8PSK, dvbs2::FECFRAME_NORMAL, dvbs2::C2_3);
        int observed_frames = 0;
        float max_amplitude = 0.0f;
        pll.on_frame = [&](complex_t *frame, int size, float frequency)
        {
            ++observed_frames;
            if (frame != pll.output_stream->writeBuf || size != frame_size || std::abs(frequency - 0.0264f) > 0.001f)
            {
                std::fprintf(stderr, "FAIL: frame observer must receive the owned buffer and its frequency\n");
                passed = false;
            }
            for (int i = 90; i < size; ++i)
                max_amplitude = std::max(max_amplitude, frame[i].norm());
        };
        pll.start();
        soft.start();
        std::memcpy(input->writeBuf, samples.data(), count * sizeof(complex_t));
        input->swap(count);
        auto result = std::async(std::launch::async, [&] { return soft.output_stream->read(); });
        const bool emitted = result.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        if (!emitted)
            soft.output_stream->stopReader();
        const int emitted_size = result.get();
        soft.output_stream->flush();
        std::printf("configured_pilots=%d actual_pilots=1 emitted=%d max_amplitude=%.3f residual_freq=%.5f\n",
                    int(pilots), int(emitted), max_amplitude, pll.getFreq());
        if (!emitted || emitted_size != (pilots ? 64800 : 0) ||
            observed_frames != int(pilots) || pll.header_locked.load() != pilots ||
            (!pilots && (max_amplitude > 0.01f || std::abs(pll.getFreq()) > 0.001f)) ||
            (pilots && (max_amplitude > 1.5f || std::abs(pll.getFreq() - 0.0264f) > 0.001f)))
        {
            std::fprintf(stderr, "FAIL: mismatched headers must be rejected; matching headers must produce a frame\n");
            passed = false;
        }
        soft.stop();
        pll.stop();
    }
    return passed ? 0 : 1;
}
