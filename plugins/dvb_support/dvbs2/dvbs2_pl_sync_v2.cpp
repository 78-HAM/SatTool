#include "dvbs2_pl_sync_v2.h"

#include <cstring>

namespace dvbs2
{
    S2PLSyncBlockV2::S2PLSyncBlockV2(std::shared_ptr<dsp::stream<complex_t>> input, int slots, bool pilots)
        : Block(input), slot_number(slots)
    {
        ring_buffer.init(10000000);
        const int pilot_count = pilots ? (slot_number - 1) / 16 : 0;
        raw_frame_size = (slot_number + 1) * 90 + pilot_count * 36;
        correlation_buffer = new complex_t[raw_frame_size];
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
        if (ring_buffer.read(correlation_buffer, raw_frame_size) <= 0)
            return;

        complex_t plheader_symbols[sof.LENGTH + pls.LENGTH];
        double best_match = -1.0;
        int best_pos = 0;

        const int search_count = raw_frame_size - sof.LENGTH - pls.LENGTH + 1;
        const auto correlate_at = [&](int ss)
        {
            plheader_symbols[0] = 0;
            volk_32fc_conjugate_32fc((lv_32fc_t *)&plheader_symbols[1], (lv_32fc_t *)&correlation_buffer[ss], sof.LENGTH + pls.LENGTH - 1);
            volk_32fc_x2_multiply_32fc((lv_32fc_t *)plheader_symbols, (lv_32fc_t *)plheader_symbols, (lv_32fc_t *)&correlation_buffer[ss], sof.LENGTH + pls.LENGTH);

            complex_t csof = correlate_sof_diff(plheader_symbols);
            complex_t cplsc = correlate_plscode_diff(&plheader_symbols[sof.LENGTH]);
            complex_t c0 = csof + cplsc;
            complex_t c1 = csof - cplsc;
            complex_t c = c0.norm() > c1.norm() ? c0 : c1;
            complex_t d = c * (1.0f / (26 - 1 + 64 / 2));
            return d.norm();
        };

        // Once acquired, the previous output ends exactly where the next
        // PLHEADER begins.  Checking position zero avoids an expensive full
        // frame search for every frame.  Fall back to acquisition when clock
        // slips or a weak frame causes the boundary check to fail.
        if (synchronized.load())
        {
            best_match = correlate_at(0);
            if (best_match < thresold)
                synchronized = false;
        }

        if (!synchronized.load())
        {
            best_match = -1.0;
            // During acquisition retain the global maximum rather than the
            // first threshold crossing, which can be a data false-positive.
            for (int ss = 0; ss < search_count; ++ss)
            {
                const double match = correlate_at(ss);
                if (match > best_match)
                {
                    best_match = match;
                    best_pos = ss;
                }
            }
            synchronized = best_match >= thresold;
        }

        last_match = static_cast<float>(best_match);

        current_position = best_pos;
        if (best_pos > 0 && best_pos < raw_frame_size)
        {
            memmove(correlation_buffer, &correlation_buffer[best_pos], (raw_frame_size - best_pos) * sizeof(complex_t));
            ring_buffer.read(&correlation_buffer[raw_frame_size - best_pos], best_pos);
        }

        memcpy(output_stream->writeBuf, correlation_buffer, raw_frame_size * sizeof(complex_t));
        ++processed_frames;
        output_stream->swap(raw_frame_size);
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
