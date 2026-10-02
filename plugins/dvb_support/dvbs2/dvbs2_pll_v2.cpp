#include "dvbs2_pll_v2.h"

#include <cmath>

namespace dvbs2
{
    namespace
    {
        constexpr float two_pi = 2.0f * static_cast<float>(M_PI);

        float unwrap_near(float angle, float predicted)
        {
            return predicted + std::remainder(angle - predicted, two_pi);
        }

        complex_t rotation(float angle)
        {
            return complex_t(cosf(angle), sinf(angle));
        }

        float power(complex_t value)
        {
            return value.real * value.real + value.imag * value.imag;
        }
    }

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
        const int count = (frame_slot_count + 1) * 90 + pilot_cnt * 36;
        reference.assign(count + 90, complex_t(0, 0));
        for (int i = 0; i < 26; ++i)
            reference[i] = sof.symbols[i];
        for (int i = 0; i < 26; ++i)
            reference[count + i] = sof.symbols[i];
        for (int i = 0; i < 64; ++i)
        {
            reference[26 + i] = pls.symbols[pls_code][i];
            reference[count + 26 + i] = pls.symbols[pls_code][i];
        }
        scrambling.reset();
        int physical = 90;
        for (int slot = 0; slot < frame_slot_count; ++slot)
        {
            if (pilots && slot > 0 && slot % 16 == 0)
                for (int p = 0; p < 36; ++p)
                {
                    complex_t symbol(0.70710678f, 0.70710678f);
                    reference[physical++] = scrambling.scramble(symbol);
                }
            for (int i = 0; i < 90; ++i)
            {
                complex_t symbol(0, 0);
                (void)scrambling.scramble(symbol);
                ++physical;
            }
        }
        constellation_points.clear();
        if (constellation)
            for (int state = 0; state < (1 << constellation->getBitsCnt()); ++state)
                constellation_points.push_back(constellation->mod(state));
        coarse_acquired = false;
    }

    complex_t S2PLLBlockV2::correlate_known(const complex_t *samples, int start, int length, float center, float frequency)
    {
        complex_t correlation = 0;
        for (int i = start; i < start + length; ++i)
        {
            complex_t sample = samples[i];
            correlation += sample * reference[i].conj() * rotation(-frequency * (i - center));
        }
        return correlation;
    }

    float S2PLLBlockV2::acquire_header_frequency(const complex_t *samples)
    {
        // Coherent header matching is far less noisy than adjacent-symbol
        // frequency estimation. Only acquisition replaces the tracked prior.
        float best_frequency = freq;
        float best_match = -1.0f;
        for (int step = -50; step <= 50; ++step)
        {
            const float candidate = step * 0.01f;
            const float match = power(correlate_known(samples, 0, 90, 44.5f, candidate));
            if (match > best_match)
            {
                best_match = match;
                best_frequency = candidate;
            }
        }
        for (float resolution : {0.001f, 0.0001f})
        {
            const float center = best_frequency;
            for (int step = -10; step <= 10; ++step)
            {
                const float candidate = center + step * resolution;
                const float match = power(correlate_known(samples, 0, 90, 44.5f, candidate));
                if (match > best_match)
                {
                    best_match = match;
                    best_frequency = candidate;
                }
            }
        }
        return best_frequency;
    }

    float S2PLLBlockV2::data_match(const complex_t *samples, float first_phase, float last_phase, float first_center, float last_center, int start, int end)
    {
        if (constellation_points.empty())
            return 0.0f;
        const float slope = (last_phase - first_phase) / (last_center - first_center);
        float error = 0.0f;
        const int stride = pilots ? 12 : std::max(1, (end - start) / 2048);
        S2Scrambling match_scrambler;
        match_scrambler.reset();
        for (int i = 90; i < end; ++i)
        {
            complex_t sample = samples[i];
            complex_t descrambled = match_scrambler.descramble(sample);
            const int data_position = i - 90;
            const int pilot_period = 16 * 90 + 36;
            const bool is_pilot = pilots && data_position >= 16 * 90 && (data_position % pilot_period) >= 16 * 90;
            if (i < start || is_pilot || ((i - start) % stride) != 0)
                continue;

            descrambled = descrambled * rotation(-first_phase - slope * (i - first_center));
            float nearest = std::numeric_limits<float>::max();
            for (complex_t point : constellation_points)
                nearest = std::min(nearest, power(descrambled - point));
            error += nearest;
        }
        return error;
    }

    void S2PLLBlockV2::estimate_frame_carrier(const complex_t *samples, int count, int available)
    {
        anchors.clear();
        float energy = 0.0f;
        for (int i = 0; i < 90; ++i)
            energy += power(samples[i]);
        const float tracked_match = power(correlate_known(samples, 0, 90, 44.5f, freq)) / std::max(90.0f * energy, 1.0e-9f);
        const bool acquiring = !coarse_acquired || tracked_match < 0.5f;
        const float previous_frequency = freq;
        if (acquiring)
            freq = acquire_header_frequency(samples);
        const float frequency_prior = coarse_acquired ? previous_frequency : freq;

        complex_t header = correlate_known(samples, 0, 90, 44.5f, freq);
        anchors.push_back({44.5f, header.arg()});
        if (pilots)
            for (int p = 0; p < pilot_cnt; ++p)
            {
                const int start = 90 + (p + 1) * 16 * 90 + p * 36;
                if (start + 36 <= count)
                {
                    const float center = start + 17.5f;
                    complex_t correlation = correlate_known(samples, start, 36, center, freq);
                    if (power(correlation) > 1.0e-6f)
                        anchors.push_back({center, correlation.arg()});
                }
            }
        if (available >= count + 90)
        {
            const float center = count + 44.5f;
            complex_t next = correlate_known(samples, count, 90, center, freq);
            float next_energy = 0.0f;
            for (int i = count; i < count + 90; ++i)
                next_energy += power(samples[i]);
            if (power(next) / std::max(90.0f * next_energy, 1.0e-9f) > 0.4f)
                anchors.push_back({center, next.arg()});
        }

        if (anchors.size() >= 2)
        {
            // Pilots constrain phase modulo 2*pi, not frequency uniquely.
            // Resolve integer-cycle slips using the data constellation, as
            // LeanDVB's match_frame does, before applying corrections.
            const float span = anchors[1].center - anchors[0].center;
            const float base_phase = unwrap_near(anchors[1].phase, anchors[0].phase + freq * span);
            const int end = anchors[1].center > count ? count : static_cast<int>(anchors[1].center - 17.5f);
            const int slip_range = pilots ? 10 : 50;
            float best_error = std::numeric_limits<float>::max();
            float best_phase = base_phase;
            for (int slip = -slip_range; slip <= slip_range; ++slip)
            {
                const float candidate = base_phase + slip * two_pi;
                const float error = data_match(samples, anchors[0].phase, candidate, anchors[0].center, anchors[1].center, 90, end);
                const float candidate_frequency = (candidate - anchors[0].phase) / span;
                const float continuity_penalty = 2.0e6f * (candidate_frequency - frequency_prior) * (candidate_frequency - frequency_prior);
                if (error + continuity_penalty < best_error)
                {
                    best_error = error + continuity_penalty;
                    best_phase = candidate;
                }
            }
            freq = (best_phase - anchors[0].phase) / span;
            for (size_t i = 1; i < anchors.size(); ++i)
            {
                PhaseAnchor &current = anchors[i];
                const PhaseAnchor &previous = anchors[i - 1];
                current.phase = unwrap_near(current.phase, previous.phase + freq * (current.center - previous.center));
            }
            const PhaseAnchor &first = anchors.front();
            const PhaseAnchor &last = anchors.back();
            freq = (last.phase - first.phase) / (last.center - first.center);
        }
        phase = anchors[0].phase - freq * anchors[0].center;
        coarse_acquired = true;
    }

    float S2PLLBlockV2::phase_at(int symbol) const
    {
        if (anchors.size() < 2)
            return anchors.front().phase + freq * (symbol - anchors.front().center);
        size_t right = 1;
        while (right + 1 < anchors.size() && symbol > anchors[right].center)
            ++right;
        const PhaseAnchor &a = anchors[right - 1];
        const PhaseAnchor &b = anchors[right];
        return a.phase + (b.phase - a.phase) * (symbol - a.center) / (b.center - a.center);
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
        if (nsamples < expected || expected < 90)
        {
            input_stream->flush();
            coarse_acquired = false;
            return;
        }
        estimate_frame_carrier(input_stream->readBuf, expected, nsamples);
        const bool pilot_aided = pilots && anchors.size() >= 2;
        scrambling.reset();
        for (int i = 0; i < expected; ++i)
        {
            // Pilot-aided interpolation is retroactive across each data block;
            // noisy data decisions must not perturb those known-symbol anchors.
            if (pilot_aided)
                phase = phase_at(i);
            complex_t sample = input_stream->readBuf[i];
            complex_t rotated = sample * rotation(-phase);
            float error = 0.0f;
            if (i < 90)
            {
                error = (rotated * reference[i].conj()).arg();
                output_stream->writeBuf[i] = (i & 1) ? complex_t(-rotated.real, rotated.imag) : complex_t(rotated.imag, rotated.real);
            }
            else
            {
                complex_t descrambled = scrambling.descramble(rotated);
                if (!pilot_aided)
                {
                    if (power(reference[i]) > 0.0f)
                        error = (rotated * reference[i].conj()).arg();
                    else if (constellation)
                        constellation->demod_soft_improved(descrambled, nullptr, 0.45f, 1.0f, &error);
                }
                output_stream->writeBuf[i] = rotated;
            }
            if (!pilot_aided)
                update_loop(error);
        }
        reported_freq = freq;
        input_stream->flush();
        output_stream->swap(expected);
    }
}
