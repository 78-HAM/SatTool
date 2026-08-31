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
        bool should_run2 = false;
        complex_t *correlation_buffer = nullptr;
        s2_sof sof;
        s2_plscodes pls;

        void work();
        void work2();
        complex_t correlate_sof_diff(complex_t *diffs);
        complex_t correlate_plscode_diff(complex_t *diffs);

    public:
        int slot_number;
        int raw_frame_size;
        int current_position = -1;
        float thresold = 0.6f;
        std::atomic<bool> synchronized{false};
        std::atomic<float> last_match{0.0f};
        std::atomic<uint64_t> processed_frames{0};

        S2PLSyncBlockV2(std::shared_ptr<dsp::stream<complex_t>> input, int slot_number, bool pilots);
        ~S2PLSyncBlockV2();

        void start()
        {
            Block::start();
            should_run2 = true;
            d_thread2 = std::thread(&S2PLSyncBlockV2::work2, this);
        }
        void stop()
        {
            Block::stop();
            should_run2 = false;
            ring_buffer.stopReader();
            ring_buffer.stopWriter();
            if (d_thread2.joinable())
                d_thread2.join();
        }
    };
}
