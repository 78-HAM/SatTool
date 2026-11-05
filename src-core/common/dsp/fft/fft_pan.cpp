#include "fft_pan.h"
#include "logger.h"
#include "common/dsp/window/window.h"
#include "core/config.h"
#include <algorithm>
#include <cmath>

namespace dsp
{
    FFTPanBlock::FFTPanBlock(std::shared_ptr<dsp::stream<complex_t>> input)
        : Block(input)
    {
    }

    FFTPanBlock::~FFTPanBlock()
    {
        if (fft_output_buffer != nullptr)
            destroy_fft();
    }

    void FFTPanBlock::set_fft_settings(int size, uint64_t samplerate, int rate)
    {
        fft_mutex.lock();

        if (rate < 1)
            rate = 1;

        fft_size = size;

        if (fft_output_buffer != nullptr)
            destroy_fft();

        // Compute FFT settings
        rbuffer_rate = (samplerate / rate);
        rbuffer_size = std::min<int>(rbuffer_rate, fft_size);
        rbuffer_skip = rbuffer_rate - rbuffer_size;
        logger->trace("FFT Rate %d, Samplerate %d, Final Size %d, Skip %d", rbuffer_rate, samplerate, rbuffer_size, rbuffer_skip);

        // Init taps, rectangular window
        fft_taps.resize(rbuffer_size);
        for (int i = 0; i < rbuffer_size; i++)
            fft_taps[i] = window::nuttall(i, rbuffer_size - 1) * ((i % 2) ? 1.0f : -1.0f);

        // Init FFTW
        fftw_in = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * fft_size);
        fftw_out = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * fft_size);
        fftw_plan = fftwf_plan_dft_1d(fft_size, fftw_in, fftw_out, FFTW_FORWARD, FFTW_ESTIMATE);

        memset(fftw_in, 0, sizeof(fftwf_complex) * fft_size);
        memset(fftw_out, 0, sizeof(fftwf_complex) * fft_size);

        // Output buffer
        fft_input_buffer = create_volk_buffer<complex_t>(fft_size);
        fft_output_buffer = create_volk_buffer<float>(fft_size);

        reshape_buffer_size = std::max<int>(STREAM_BUFFER_SIZE, rbuffer_rate * 10);
        fft_reshape_buffer = create_volk_buffer<complex_t>(reshape_buffer_size);
        in_reshape_buffer = 0;

        fft_mutex.unlock();
    }

    void FFTPanBlock::destroy_fft()
    {
        fftwf_free(fftw_in);
        fftwf_free(fftw_out);
        fftwf_destroy_plan(fftw_plan);
        volk_free(fft_input_buffer);
        volk_free(fft_output_buffer);
        volk_free(fft_reshape_buffer);
    }

    void FFTPanBlock::work()
    {
        int nsamples = input_stream->read();
        if (nsamples <= 0)
        {
            input_stream->flush();
            return;
        }

        fft_mutex.lock();

        if (in_reshape_buffer + nsamples < reshape_buffer_size)
        {
            memcpy(&fft_reshape_buffer[in_reshape_buffer], input_stream->readBuf, nsamples * sizeof(complex_t));
            in_reshape_buffer += nsamples;
        }
        input_stream->flush();

        if (in_reshape_buffer > rbuffer_rate)
        {
            int pos_in_buffer = 0;
            while (in_reshape_buffer - pos_in_buffer > rbuffer_rate)
            {
                memcpy(fft_input_buffer, &fft_reshape_buffer[pos_in_buffer], rbuffer_size * sizeof(complex_t));
                pos_in_buffer += rbuffer_rate;

                complex_t *buffer_ptr = fft_input_buffer;

                bool active_prank = is_prank;
                if (satdump::config::main_cfg.contains("advanced_settings") &&
                    satdump::config::main_cfg["advanced_settings"].contains("prank_overload_enable"))
                {
                    if (!satdump::config::main_cfg["advanced_settings"]["prank_overload_enable"].get<bool>())
                        active_prank = false;
                }

                if (active_prank)
                {
                    float sync_q = prank_overload_factor.load();
                    if (sync_q < 0.05f)
                        sync_q = 0.05f;

                    // Measure block RMS power
                    float sum_sq = 0.0f;
                    for (int i = 0; i < rbuffer_size; i++)
                        sum_sq += buffer_ptr[i].real * buffer_ptr[i].real + buffer_ptr[i].imag * buffer_ptr[i].imag;
                    float rms = sqrtf(sum_sq / (float)rbuffer_size + 1e-12f);
                    float inv_rms = 1.0f / rms;

                    // Non-linear front-end saturation & 3rd-order intermodulation (IMD3)
                    float gain = 1.0f + 2.4f * sync_q;
                    float alpha3 = 0.28f * sync_q;
                    float clip_val = 2.0f - 0.75f * sync_q;

                    for (int i = 0; i < rbuffer_size; i++)
                    {
                        float nx_i = buffer_ptr[i].real * inv_rms * gain;
                        float nx_q = buffer_ptr[i].imag * inv_rms * gain;
                        float mag2 = nx_i * nx_i + nx_q * nx_q;

                        // 3rd-order intermodulation: y = x - alpha3 * |x|^2 * x
                        float y_i = nx_i - alpha3 * mag2 * nx_i;
                        float y_q = nx_q - alpha3 * mag2 * nx_q;

                        // ADC / LNA clipping
                        y_i = std::clamp(y_i, -clip_val, clip_val);
                        y_q = std::clamp(y_q, -clip_val, clip_val);

                        buffer_ptr[i] = complex_t(y_i * rms, y_q * rms);
                    }
                }

                volk_32fc_32f_multiply_32fc((lv_32fc_t *)fftw_in, (lv_32fc_t *)buffer_ptr, fft_taps.data(), rbuffer_size);

                fftwf_execute(fftw_plan);

                volk_32fc_s32f_power_spectrum_32f(fft_output_buffer, (lv_32fc_t *)fftw_out, fft_size, fft_size);

                if (avg_num < 1)
                    avg_num = 1;
                float avg_rate = 1.0 / avg_num;

                for (int i = 0; i < fft_size; i++)
                    output_stream->writeBuf[i] = output_stream->writeBuf[i] * (1.0f - avg_rate) + fft_output_buffer[i] * avg_rate;

                on_fft(output_stream->writeBuf);
            }

            if (pos_in_buffer < in_reshape_buffer)
            {
                memmove(fft_reshape_buffer, &fft_reshape_buffer[pos_in_buffer], (in_reshape_buffer - pos_in_buffer) * sizeof(complex_t));
                in_reshape_buffer -= pos_in_buffer;
            }
        }

        fft_mutex.unlock();
    }
}