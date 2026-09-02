#include "dvbs2_pll_v2.h"

#include <limits>

namespace dvbs2
{
    S2PLLBlockV2::S2PLLBlockV2(std::shared_ptr<dsp::stream<complex_t>> input, float bw)
        : Block(input), loop_bw(std::max(1.0e-5f, bw))
    {
        const float damping = sqrtf(2.0f) / 2.0f;
        const float denom = 1.0f + 2.0f * damping * loop_bw + loop_bw * loop_bw;
        alpha = (4.0f * damping * loop_bw) / denom;
        beta = (4.0f * loop_bw * loop_bw) / denom;
    }

    S2PLLBlockV2::~S2PLLBlockV2() {}

    void S2PLLBlockV2::update()
    {
        pilot_cnt = pilots ? (frame_slot_count - 1) / 16 : 0;
    }

    void S2PLLBlockV2::coarse_lock_header(const complex_t *samples)
    {
        complex_t expected[90];
        for (int i = 0; i < 26; ++i)
            expected[i] = sof.symbols[i];
        for (int i = 0; i < 64; ++i)
            expected[26 + i] = pls.symbols[pls_code][i];

        // Differential correlation removes the unknown initial phase and
        // estimates the carrier rotation per symbol over the known header.
        complex_t freq_corr = 0;
        for (int i = 0; i < 89; ++i)
        {
            complex_t e0 = expected[i];
            complex_t e1 = expected[i + 1];
            complex_t r0 = samples[i];
            complex_t r1 = samples[i + 1];
            complex_t expected_step = e0.conj() * e1;
            complex_t received_step = r0.conj() * r1;
            freq_corr += expected_step.conj() * received_step;
        }

        const float coarse_freq = freq_corr.arg();
        if (std::isfinite(coarse_freq))
            freq = std::max(-0.5f, std::min(0.5f, coarse_freq));

        // Remove the estimated ramp and correlate once more for the phase at
        // the first header symbol.  This gives the narrow tracking loop a
        // stable starting point instead of asking it to acquire from zero.
        complex_t phase_corr = 0;
        for (int i = 0; i < 90; ++i)
        {
            complex_t raw = samples[i];
            complex_t received = raw * complex_t(cosf(-freq * i), sinf(-freq * i));
            phase_corr += received * expected[i].conj();
        }
        const float coarse_phase = phase_corr.arg();
        if (std::isfinite(coarse_phase))
            phase = coarse_phase;
    }

    void S2PLLBlockV2::estimate_frame_carrier(const complex_t *samples, int count)
    {
        // The header gives an absolute phase and a coarse frequency.  Pilot
        // blocks then refine the frequency over the complete frame.  This is
        // the look-ahead part of the SDRangel carrier recovery: all symbols
        // in the current frame start with the best available frequency
        // estimate instead of waiting for the next pilot.
        complex_t expected[90];
        for (int i = 0; i < 26; ++i)
            expected[i] = sof.symbols[i];
        for (int i = 0; i < 64; ++i)
            expected[26 + i] = pls.symbols[pls_code][i];

        complex_t freq_corr = 0;
        for (int i = 0; i < 89 && i + 1 < count; ++i)
        {
            const complex_t expected_step = expected[i].conj() * expected[i + 1];
            const complex_t received_step = samples[i].conj() * samples[i + 1];
            freq_corr += expected_step.conj() * received_step;
        }

        float frame_freq = freq;
        if (freq_corr.norm() > 1.0e-4f)
            frame_freq = std::max(-0.5f, std::min(0.5f, freq_corr.arg()));

        struct PilotMeasurement
        {
            int center;
            complex_t correlation;
        };
        std::vector<PilotMeasurement> measurements;

        if (pilots && count > 90)
        {
            // Re-run the physical scrambler so pilot correlations use the
            // same phase rotation as the normal soft-demodulation pass.
            scrambling.reset();
            int data_seen = 0;
            int next_pilot_data = 16 * 90;
            int physical = 90;
            const complex_t pilot_symbol(0.70710678f, 0.70710678f);

            while (physical < count)
            {
                if (data_seen == next_pilot_data)
                {
                    complex_t correlation = 0;
                    int used = 0;
                    for (int p = 0; p < 36 && physical + p < count; ++p)
                    {
                        complex_t sample = samples[physical + p];
                        complex_t descrambled = scrambling.descramble(sample);
                        correlation += descrambled * pilot_symbol.conj();
                        ++used;
                    }
                    if (used >= 24 && correlation.norm() > 4.0f)
                        measurements.push_back({physical + 17, correlation});
                    physical += 36;
                    next_pilot_data += 16 * 90;
                    continue;
                }

                complex_t sample = samples[physical++];
                (void)scrambling.descramble(sample);
                ++data_seen;
            }
        }

        if (measurements.size() >= 2)
        {
            float previous_phase = measurements.front().correlation.arg();
            int previous_center = measurements.front().center;
            float slope_sum = 0.0f;
            int slope_count = 0;

            for (size_t i = 1; i < measurements.size(); ++i)
            {
                const int span = measurements[i].center - previous_center;
                if (span <= 0)
                    continue;

                float phase_delta = measurements[i].correlation.arg() - previous_phase;
                // Unwrap around the expected phase advance from the current
                // frequency estimate before averaging pilot intervals.
                const float expected_delta = frame_freq * static_cast<float>(span);
                while (phase_delta - expected_delta > static_cast<float>(M_PI))
                    phase_delta -= 2.0f * static_cast<float>(M_PI);
                while (phase_delta - expected_delta < -static_cast<float>(M_PI))
                    phase_delta += 2.0f * static_cast<float>(M_PI);

                slope_sum += phase_delta / static_cast<float>(span);
                ++slope_count;
                previous_phase = measurements[i].correlation.arg();
                previous_center = measurements[i].center;
            }

            if (slope_count > 0)
                frame_freq = slope_sum / static_cast<float>(slope_count);
        }

        // Estimate the phase at the first header symbol using the refined
        // frequency.  This makes the correction retroactive for the whole
        // frame while retaining the existing decision-directed PLL below.
        complex_t phase_corr = 0;
        for (int i = 0; i < 90 && i < count; ++i)
        {
            complex_t raw = samples[i];
            complex_t dechirped = raw * complex_t(cosf(-frame_freq * i), sinf(-frame_freq * i));
            phase_corr += dechirped * expected[i].conj();
        }

        if (std::isfinite(frame_freq))
            freq = frame_freq;
        if (phase_corr.norm() > 1.0e-4f && std::isfinite(phase_corr.arg()))
            phase = phase_corr.arg();
    }

    void S2PLLBlockV2::work()
    {
        const int nsamples = input_stream->read();
        if (nsamples <= 0)
        {
            input_stream->flush();
            return;
        }

        const int expected = (frame_slot_count + 1) * 90 + pilot_cnt * 36;
        const int count = std::min(nsamples, expected);
        if (count >= 90)
        {
            estimate_frame_carrier(input_stream->readBuf, count);
            coarse_acquired = true;
        }
        scrambling.reset();
        int data_seen = 0;
        int next_pilot_data = pilots ? 16 * 90 : std::numeric_limits<int>::max();

        for (int i = 0; i < count; ++i)
        {
            complex_t rotated = input_stream->readBuf[i] * complex_t(cosf(-phase), sinf(-phase));
            complex_t phase_sample = rotated;
            float error = 0.0f;

            if (i < 26)
            {
                error = (rotated * sof.symbols[i].conj()).arg();
                output_stream->writeBuf[i] = (i & 1) ? complex_t(-rotated.real, rotated.imag) : complex_t(rotated.imag, rotated.real);
            }
            else if (i < 90)
            {
                error = (rotated * pls.symbols[pls_code][i - 26].conj()).arg();
                output_stream->writeBuf[i] = (i & 1) ? complex_t(-rotated.real, rotated.imag) : complex_t(rotated.imag, rotated.real);
            }
            else
            {
                const bool is_pilot = data_seen == next_pilot_data;
                if (is_pilot)
                {
                    // Pilots are part of the physical scrambling sequence but
                    // are not part of the soft-symbol output.
                    for (int p = 0; p < 36 && i + p < count; ++p)
                    {
                        complex_t pilot_rotated = input_stream->readBuf[i + p] * complex_t(cosf(-phase), sinf(-phase));
                        complex_t pilot_sample = pilot_rotated;
                        complex_t pilot_descrambled = scrambling.descramble(pilot_sample);
                        error = (pilot_descrambled * complex_t(0.70710678f, 0.70710678f).conj()).arg();
                        output_stream->writeBuf[i + p] = pilot_rotated;
                        update_loop(error);
                    }
                    i += 35;
                    next_pilot_data += 16 * 90;
                    continue;
                }

                complex_t descrambled = scrambling.descramble(phase_sample);
                if (constellation)
                    constellation->demod_soft_improved(descrambled, nullptr, 0.45f, 1.0f, &error);
                output_stream->writeBuf[i] = rotated;
                ++data_seen;
            }
            update_loop(error);
        }

        input_stream->flush();
        output_stream->swap(count);
    }
}
