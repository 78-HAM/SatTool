#pragma once

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace mpeg_ts
{
    // Spread LDPC batch output over the TS clock instead of flooding the
    // player's UDP receive buffer. Only the first PCR PID controls the clock.
    class TSPacer
    {
    public:
        using Clock = std::chrono::steady_clock;

        TSPacer(bool use_pcr, double bitrate = 0) : use_pcr(use_pcr), fixed_bitrate(bitrate), rate(bitrate)
        {
            if (!std::isfinite(bitrate) || bitrate < 0 || bitrate > 1e9 || (bitrate > 0 && bitrate < 64000))
                throw std::invalid_argument("UDP TS bitrate must be 0 (PCR auto) or 64000..1000000000 bit/s");
        }

        Clock::time_point schedule(const uint8_t *data, size_t length, Clock::time_point now)
        {
            if (use_pcr && fixed_bitrate == 0 && length % 188 == 0)
            {
                for (size_t pos = 0; pos < length; pos += 188)
                {
                    const uint8_t *ts = data + pos;
                    if (ts[0] != 0x47 || (ts[1] & 0x80) || !(ts[3] & 0x20) || ts[4] < 7 || ts[4] > 183 || !(ts[5] & 0x10))
                        continue;
                    const int pid = ((ts[1] & 0x1f) << 8) | ts[2];
                    if (pcr_pid != -1 && pid != pcr_pid)
                        continue;
                    pcr_pid = pid;
                    const uint64_t base = (uint64_t(ts[6]) << 25) | (uint64_t(ts[7]) << 17) |
                                          (uint64_t(ts[8]) << 9) | (uint64_t(ts[9]) << 1) | (ts[10] >> 7);
                    const uint64_t pcr = base * 300 + ((ts[10] & 1) << 8) + ts[11];
                    const uint64_t offset = stream_bytes + pos;
                    constexpr uint64_t wrap = (uint64_t(1) << 33) * 300;
                    const uint64_t ticks = (pcr + wrap - last_pcr) % wrap;
                    if (have_pcr && !(ts[5] & 0x80) && ticks > 0 && ticks <= 27000000 * 2)
                    {
                        const double sample = double(offset - last_offset) * 8 * 27000000 / ticks;
                        if (sample >= 64000 && sample <= 1e9)
                            rate = rate == 0 ? sample : rate * 0.75 + sample * 0.25;
                    }
                    else if (!have_pcr || (ts[5] & 0x80) || ticks > 27000000 * 2)
                    {
                        rate = 0;
                        scheduled = false;
                    }
                    last_pcr = pcr;
                    last_offset = offset;
                    have_pcr = true;
                }
            }
            stream_bytes += length;

            if (rate == 0)
                return now;
            // An actual input pause/retune must not cause a catch-up burst.
            if (!scheduled || now - next_send > std::chrono::milliseconds(100))
                next_send = now;
            const auto target = next_send;
            next_send += std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(length * 8 / rate));
            scheduled = true;
            return target;
        }

        double bitrate() const { return rate; }

    private:
        bool use_pcr;
        double fixed_bitrate, rate;
        int pcr_pid = -1;
        bool have_pcr = false, scheduled = false;
        uint64_t stream_bytes = 0, last_pcr = 0, last_offset = 0;
        Clock::time_point next_send{};
    };
}
