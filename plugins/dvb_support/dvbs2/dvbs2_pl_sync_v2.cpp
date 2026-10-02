#include "dvbs2_pl_sync_v2.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace dvbs2
{
    S2PLSyncBlockV2::S2PLSyncBlockV2(std::shared_ptr<dsp::stream<complex_t>> input, int slots, bool pilots, float samples_per_symbol)
        : Block(input), slot_number(slots), timing_sps(std::max(1.0f, samples_per_symbol)), input_sps(std::max(1.0f, samples_per_symbol))
    {
        ring_buffer.init(10000000);
        const int pilot_count = pilots ? (slot_number - 1) / 16 : 0;
        raw_frame_size = (slot_number + 1) * 90 + pilot_count * 36;
        // Leave enough headroom for the oversampled look-ahead path.  The
        // default one-sample path uses only a small prefix of this buffer.
        // Support the normal 2..6 samples/symbol range with room for a
        // bounded clock-rate excursion and the acquisition look-ahead.
        correlation_capacity = (raw_frame_size * 2 + 512) * 12 + 256;
        correlation_buffer = new complex_t[correlation_capacity];
    }

    S2PLSyncBlockV2::~S2PLSyncBlockV2()
    {
        delete[] correlation_buffer;
    }

    void S2PLSyncBlockV2::work()
    {
        const int nsamples = input_stream->read();
        if (nsamples <= 0)
        {
            input_stream->flush();
            return;
        }
        ring_buffer.write(input_stream->readBuf, nsamples);
        input_stream->flush();
    }

    void S2PLSyncBlockV2::work2()
    {
        if (input_sps > 1.01f)
        {
            work2_oversampled();
            return;
        }

        const int window_size = raw_frame_size + 90;
        if (ring_buffer.read(&correlation_buffer[buffered], window_size - buffered) <= 0)
            return;

        complex_t plheader_symbols[sof.LENGTH + pls.LENGTH];
        double best_match = -1.0;
        int best_pos = 0;
        int best_fraction = 0;

        const int search_count = raw_frame_size - sof.LENGTH - pls.LENGTH + 1;
        const int fraction_count = fractional_timing ? 8 : 1;
        const auto interpolate = [&](float position)
        {
            const int index = static_cast<int>(position);
            const float fraction = position - index;
            if (index < 0)
                return correlation_buffer[0];
            if (index + 1 >= window_size)
                return correlation_buffer[std::min(index, window_size - 1)];
            return correlation_buffer[index] * (1.0f - fraction) + correlation_buffer[index + 1] * fraction;
        };
        const auto correlate_at = [&](int ss, int fraction_index)
        {
            const float fraction = static_cast<float>(fraction_index) / fraction_count;
            complex_t previous = interpolate(ss + fraction);
            plheader_symbols[0] = 0;
            for (int i = 1; i < sof.LENGTH + pls.LENGTH; ++i)
            {
                const complex_t current = interpolate(ss + fraction + i);
                plheader_symbols[i] = previous.conj() * current;
                previous = current;
            }

            complex_t csof = correlate_sof_diff(plheader_symbols);
            complex_t cplsc = correlate_plscode_diff(&plheader_symbols[sof.LENGTH]);
            complex_t c0 = csof + cplsc;
            complex_t c1 = csof - cplsc;
            complex_t c = c0.norm() > c1.norm() ? c0 : c1;
            float energy = 0.0f;
            for (int i = 1; i < sof.LENGTH; ++i)
                energy += plheader_symbols[i].norm();
            for (int i = sof.LENGTH + 1; i < sof.LENGTH + pls.LENGTH; i += 2)
                energy += plheader_symbols[i].norm();
            return static_cast<double>(c.norm() / std::max(energy, 1.0e-9f));
        };

        // Once acquired, the previous output ends exactly where the next
        // PLHEADER begins.  Checking position zero avoids an expensive full
        // frame search for every frame.  Fall back to acquisition when clock
        // slips or a weak frame causes the boundary check to fail.
        if (synchronized.load())
        {
            best_match = -1.0;
            for (int fraction = 0; fraction < fraction_count; ++fraction)
            {
                const double match = correlate_at(0, fraction);
                if (match > best_match)
                {
                    best_match = match;
                    best_fraction = fraction;
                }
            }
            if (best_match < thresold)
                synchronized = false;
        }

        if (!synchronized.load())
        {
            best_match = -1.0;
            // During acquisition retain the global maximum rather than the
            // first threshold crossing, which can be a data false-positive.
            for (int ss = 0; ss < search_count; ++ss)
                for (int fraction = 0; fraction < fraction_count; ++fraction)
                {
                    const double match = correlate_at(ss, fraction);
                    if (match > best_match)
                    {
                        best_match = match;
                        best_pos = ss;
                        best_fraction = fraction;
                    }
                }
            synchronized = best_match >= thresold;
        }

        last_match = static_cast<float>(best_match);

        current_position = best_pos;
        if (best_pos > 0 && best_pos < raw_frame_size)
        {
            memmove(correlation_buffer, &correlation_buffer[best_pos], (window_size - best_pos) * sizeof(complex_t));
            if (ring_buffer.read(&correlation_buffer[window_size - best_pos], best_pos) <= 0)
                return;
        }

        // Look ahead without consuming the next frame's header. The PLL uses
        // its SOF to constrain the last data block and non-pilot frames.
        const float sample_start = static_cast<float>(best_fraction) / fraction_count;
        for (int i = 0; i < window_size; ++i)
            output_stream->writeBuf[i] = interpolate(sample_start + i);
        memmove(correlation_buffer, &correlation_buffer[raw_frame_size], 90 * sizeof(complex_t));
        buffered = 90;
        ++processed_frames;
        output_stream->swap(window_size);
    }

    void S2PLSyncBlockV2::work2_oversampled()
    {
        const int output_symbols = raw_frame_size + 90;
        const float nominal_sps = std::clamp(input_sps, 1.01f, 8.0f);
        timing_sps = std::clamp(timing_sps, nominal_sps * 0.9f, nominal_sps * 1.1f);

        // Keep one complete frame, the next SOF and interpolation margin in
        // the local buffer.  The next SOF is what makes the timing estimate
        // frame-local instead of relying on a long-running TED state.
        const int search_margin = synchronized.load() ? 0 : raw_frame_size;
        const int needed = static_cast<int>(std::ceil((raw_frame_size + search_margin + 180) * timing_sps)) + 16;
        if (needed >= correlation_capacity)
            return;
        if (buffered < needed)
        {
            const int got = ring_buffer.read(&correlation_buffer[buffered], needed - buffered);
            if (got <= 0)
                return;
            buffered += got;
        }

        const int fraction_count = 32;
        const int header_length = sof.LENGTH + pls.LENGTH;
        complex_t plheader_symbols[sof.LENGTH + pls.LENGTH];

        const auto interpolate = [&](float position)
        {
            if (position < 1.0f || position + 2.0f >= buffered)
                return complex_t(0.0f, 0.0f);
            const int index = static_cast<int>(position);
            const float fraction = position - index;
            // Catmull-Rom interpolation is a compact approximation of the
            // windowed-sinc sampler used by SDRangel and preserves the
            // fractional timing correction without adding a large FIR bank.
            complex_t p0 = correlation_buffer[index - 1];
            complex_t p1 = correlation_buffer[index];
            complex_t p2 = correlation_buffer[index + 1];
            complex_t p3 = correlation_buffer[index + 2];
            const float a0 = -0.5f * fraction * fraction * fraction + fraction * fraction - 0.5f * fraction;
            const float a1 = 1.5f * fraction * fraction * fraction - 2.5f * fraction * fraction + 1.0f;
            const float a2 = -1.5f * fraction * fraction * fraction + 2.0f * fraction * fraction + 0.5f * fraction;
            const float a3 = 0.5f * fraction * fraction * fraction - 0.5f * fraction * fraction;
            return p0 * a0 + p1 * a1 + p2 * a2 + p3 * a3;
        };

        const auto correlate_at = [&](float start, float step, bool full_header)
        {
            complex_t previous = interpolate(start);
            plheader_symbols[0] = 0;
            float energy = 0.0f;
            const int correlation_length = full_header ? header_length : sof.LENGTH;
            for (int i = 1; i < correlation_length; ++i)
            {
                const complex_t current = interpolate(start + i * step);
                plheader_symbols[i] = previous.conj() * current;
                previous = current;
            }

            complex_t csof = correlate_sof_diff(plheader_symbols);
            complex_t c = csof;
            if (full_header)
            {
                complex_t cplsc = correlate_plscode_diff(&plheader_symbols[sof.LENGTH]);
                complex_t c0 = csof + cplsc;
                complex_t c1 = csof - cplsc;
                c = c0.norm() > c1.norm() ? c0 : c1;
            }
            for (int i = 1; i < sof.LENGTH; ++i)
                energy += plheader_symbols[i].norm();
            if (full_header)
                for (int i = sof.LENGTH + 1; i < header_length; i += 2)
                    energy += plheader_symbols[i].norm();
            return static_cast<double>(c.norm() / std::max(energy, 1.0e-9f));
        };

        const auto search_header = [&](float first, float last, float step, int fractions, bool full_header)
        {
            float best_position = first;
            double best_match = -1.0;
            const float increment = step / fractions;
            for (float position = first; position <= last; position += increment)
            {
                const double match = correlate_at(position, step, full_header);
                if (match > best_match)
                {
                    best_match = match;
                    best_position = position;
                }
            }
            return std::pair<float, double>(best_position, best_match);
        };

        float best_start = 0.0f;
        double best_match = -1.0;

        // A locked stream should be aligned to the first few fractional
        // phases.  If the SOF weakened or a clock slip occurred, reacquire
        // over the whole frame just like the integer V2 path.
        if (synchronized.load())
        {
            const auto locked = search_header(0.0f, timing_sps, timing_sps, fraction_count, true);
            best_start = locked.first;
            best_match = locked.second;
            if (best_match < thresold)
                synchronized = false;
        }

        if (!synchronized.load())
        {
            const float last_start = std::max(0.0f, (raw_frame_size - header_length) * timing_sps);
            // A short SOF-only pass finds the symbol region cheaply.  Refine
            // that candidate with the complete SOF+PLS correlation.
            const auto coarse = search_header(0.0f, last_start, timing_sps, 4, false);
            const auto acquired = search_header(std::max(0.0f, coarse.first - timing_sps),
                                                std::min(last_start, coarse.first + timing_sps),
                                                timing_sps, fraction_count, true);
            best_start = acquired.first;
            best_match = acquired.second;
            synchronized = best_match >= thresold;
        }

        if (best_match < thresold)
        {
            last_match = static_cast<float>(best_match);
            // A failed wide search must still advance.  Keeping only the
            // interpolation/header margin prevents a weak frame from
            // repeatedly rescanning the same buffer and freezing the live
            // constellation view.
            const int keep = std::min(buffered, static_cast<int>(std::ceil((header_length + 8) * timing_sps)));
            const int discard = buffered - keep;
            if (discard > 0)
            {
                memmove(correlation_buffer, &correlation_buffer[discard], keep * sizeof(complex_t));
                buffered = keep;
            }
            return;
        }

        // Look for the next SOF around the nominal frame boundary.  Its
        // distance provides a fractional samples-per-symbol estimate that is
        // valid for the entire current frame.
        const float expected_next = best_start + raw_frame_size * timing_sps;
        const float radius = std::max(4.0f * timing_sps, raw_frame_size * timing_sps * 0.003f);
        const auto next = search_header(std::max(0.0f, expected_next - radius), expected_next + radius, timing_sps, fraction_count, true);
        const bool have_next = next.second >= thresold && next.first > best_start + timing_sps;
        const float next_start = have_next ? next.first : expected_next;
        const float frame_step = have_next ? (next_start - best_start) / raw_frame_size : timing_sps;
        timing_sps = std::clamp(frame_step, nominal_sps * 0.9f, nominal_sps * 1.1f);

        for (int i = 0; i < output_symbols; ++i)
            output_stream->writeBuf[i] = interpolate(best_start + i * frame_step);

        // Keep two raw samples before the next SOF.  Catmull-Rom needs both
        // neighbours; dropping them would force the first symbols of every
        // locked frame through the zero-padding edge case in interpolate().
        int consume = static_cast<int>(std::floor(next_start)) - 2;
        if (consume <= 0)
            consume = std::max(1, static_cast<int>(std::floor(best_start + raw_frame_size * frame_step)));
        consume = std::min(consume, buffered);
        const int remaining = buffered - consume;
        if (remaining > 0)
            memmove(correlation_buffer, &correlation_buffer[consume], remaining * sizeof(complex_t));
        buffered = remaining;

        current_position = static_cast<int>(std::lround(best_start / std::max(timing_sps, 1.0e-6f)));
        last_match = static_cast<float>(best_match);
        ++processed_frames;
        output_stream->swap(output_symbols);
    }

    complex_t S2PLSyncBlockV2::correlate_sof_diff(complex_t *diffs)
    {
        complex_t c = 0;
        const uint32_t dsof = sof.VALUE ^ (sof.VALUE >> 1);
        for (int i = 0; i < sof.LENGTH; ++i)
            c += (((dsof >> (sof.LENGTH - 1 - i)) ^ i) & 1) ? diffs[i] : diffs[i] * -1.0f;
        return c;
    }

    complex_t S2PLSyncBlockV2::correlate_plscode_diff(complex_t *diffs)
    {
        complex_t c = 0;
        const uint64_t dscr = pls.SCRAMBLING ^ (pls.SCRAMBLING >> 1);
        for (int i = 1; i < pls.LENGTH; i += 2)
            c += ((dscr >> (pls.LENGTH - 1 - i)) & 1) ? diffs[i] * -1.0f : diffs[i];
        return c;
    }
}
