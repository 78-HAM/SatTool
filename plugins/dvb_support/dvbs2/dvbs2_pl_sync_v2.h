#pragma once

#include "common/dsp/block.h"
#include "s2_defs.h"
#include <atomic>

namespace dvbs2
{
    // Frame synchronizer with a complete correlation search.  The legacy
    // S2PLSyncBlock is intentionally kept for existing pipelines.
    class S2PLSyncBlockV2 : public dsp::Block<complex_t, complex_t>
    {
    private:
        dsp::RingBuffer<complex_t> ring_buffer;
        std::thread d_thread2;
        std::atomic<bool> should_run2{false};
        complex_t *correlation_buffer = nullptr;
        int correlation_capacity = 0;
        int buffered = 0;
        float timing_sps = 1.0f;
        s2_sof sof;
        s2_plscodes pls;

        void work();
        void run2()
        {
            while (should_run2.load())
                work2();
        }
        void work2();
        void work2_oversampled();
        complex_t correlate_sof_diff(complex_t *diffs);
        complex_t correlate_plscode_diff(complex_t *diffs);

    public:
        int slot_number;
        int raw_frame_size;
        // Search and sample at fractional symbol offsets for improved timing.
        // Legacy and existing callers keep the integer-only behavior.
        bool fractional_timing = false;
        int current_position = -1;
        float thresold = 0.6f;
        std::atomic<bool> synchronized{false};
        std::atomic<float> last_match{0.0f};
        std::atomic<uint64_t> processed_frames{0};
        // Input samples per symbol.  The default keeps the original
        // one-sample-per-symbol V2 behavior.  Improved mode can feed the
        // matched-filtered oversampled stream directly for fractional timing.
        float input_sps = 1.0f;

        S2PLSyncBlockV2(std::shared_ptr<dsp::stream<complex_t>> input, int slot_number, bool pilots, float samples_per_symbol = 1.0f);
        ~S2PLSyncBlockV2();

        void start()
        {
            Block::start();
            should_run2 = true;
            d_thread2 = std::thread(&S2PLSyncBlockV2::run2, this);
        }
        void stop()
        {
            should_run2 = false;
            ring_buffer.stopReader();
            ring_buffer.stopWriter();
            Block::stop();
            if (d_thread2.joinable())
                d_thread2.join();
        }
    };
}
