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
        double best_positive_match = -1.0;
        int best_pos = 0;

        // Search the entire available frame.  The old decoder stopped at the
        // first threshold crossing, which made a strong false peak win.
        const int search_count = raw_frame_size - sof.LENGTH - pls.LENGTH + 1;
        for (int ss = 0; ss < search_count; ++ss)
        {
            plheader_symbols[0] = 0;
            volk_32fc_conjugate_32fc((lv_32fc_t *)&plheader_symbols[1], (lv_32fc_t *)&correlation_buffer[ss], sof.LENGTH + pls.LENGTH - 1);
            volk_32fc_x2_multiply_32fc((lv_32fc_t *)plheader_symbols, (lv_32fc_t *)plheader_symbols, (lv_32fc_t *)&correlation_buffer[ss], sof.LENGTH + pls.LENGTH);

            const complex_t csof = correlate_sof_diff(plheader_symbols);
            const complex_t cplsc = correlate_plscode_diff(&plheader_symbols[sof.LENGTH]);
            const complex_t c0 = csof + cplsc;
            const complex_t c1 = csof - cplsc;
            const complex_t c = c0.norm() > c1.norm() ? c0 : c1;
            const complex_t d = c * (1.0f / (26 - 1 + 64 / 2));
            const double match = d.norm();
            if (match > best_match)
            {
                best_match = match;
                best_pos = ss;
            }
            if (d.imag > 0 && match > best_positive_match)
                best_positive_match = match;
        }

        // Prefer the normal positive-frequency solution, but never fail to
        // produce a frame when the residual frequency is outside that range.
        if (best_positive_match >= 0)
        {
            best_match = best_positive_match;
            for (int ss = 0; ss < search_count; ++ss)
            {
                plheader_symbols[0] = 0;
                volk_32fc_conjugate_32fc((lv_32fc_t *)&plheader_symbols[1], (lv_32fc_t *)&correlation_buffer[ss], sof.LENGTH + pls.LENGTH - 1);
                volk_32fc_x2_multiply_32fc((lv_32fc_t *)plheader_symbols, (lv_32fc_t *)plheader_symbols, (lv_32fc_t *)&correlation_buffer[ss], sof.LENGTH + pls.LENGTH);
                const complex_t csof = correlate_sof_diff(plheader_symbols);
                const complex_t cplsc = correlate_plscode_diff(&plheader_symbols[sof.LENGTH]);
                const complex_t c0 = csof + cplsc;
                const complex_t c1 = csof - cplsc;
                const complex_t c = c0.norm() > c1.norm() ? c0 : c1;
                const complex_t d = c * (1.0f / (26 - 1 + 64 / 2));
                if (d.imag > 0 && d.norm() >= best_match)
                    best_pos = ss;
            }
        }

        current_position = best_pos;
        if (best_pos > 0 && best_pos < raw_frame_size)
        {
            memmove(correlation_buffer, &correlation_buffer[best_pos], (raw_frame_size - best_pos) * sizeof(complex_t));
            ring_buffer.read(&correlation_buffer[raw_frame_size - best_pos], best_pos);
        }

        memcpy(output_stream->writeBuf, correlation_buffer, raw_frame_size * sizeof(complex_t));
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
