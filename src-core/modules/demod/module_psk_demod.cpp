#include "module_psk_demod.h"
#include "common/dsp/filter/firdes.h"
#include "logger.h"
#include "imgui/imgui.h"
#include <cmath>
#include <algorithm>

namespace demod
{
    PSKDemodModule::PSKDemodModule(std::string input_file, std::string output_file_hint, nlohmann::json parameters) : BaseDemodModule(input_file, output_file_hint, parameters)
    {
        // Buffers
        sym_buffer = new int8_t[d_buffer_size * 4];

        // Parse params
        if (parameters.count("constellation") > 0)
            constellation_type = parameters["constellation"].get<std::string>();
        else
            throw satdump_exception("Constellation type parameter must be present!");

        if (parameters.count("rrc_alpha") > 0)
            d_rrc_alpha = parameters["rrc_alpha"].get<float>();
        else
            throw satdump_exception("RRC Alpha parameter must be present!");

        if (parameters.count("rrc_taps") > 0)
            d_rrc_taps = parameters["rrc_taps"].get<int>();

        if (parameters.count("pll_bw") > 0)
            d_loop_bw = parameters["pll_bw"].get<float>();
        else
            throw satdump_exception("PLL BW parameter must be present!");

        if (parameters.count("post_costas_dc") > 0)
            d_post_costas_dc_blocking = parameters["post_costas_dc"].get<bool>();

        if (parameters.count("has_carrier") > 0)
            d_has_carrier = parameters["has_carrier"].get<bool>();

        if (parameters.count("clock_alpha") > 0)
        {
            float clock_alpha = parameters["clock_alpha"].get<float>();
            d_clock_gain_omega = pow(clock_alpha, 2) / 4.0;
            d_clock_gain_mu = clock_alpha;
        }

        if (parameters.count("clock_gain_omega") > 0)
            d_clock_gain_omega = parameters["clock_gain_omega"].get<float>();

        if (parameters.count("clock_mu") > 0)
            d_clock_mu = parameters["clock_mu"].get<float>();

        if (parameters.count("clock_gain_mu") > 0)
            d_clock_gain_mu = parameters["clock_gain_mu"].get<float>();

        if (parameters.count("clock_omega_relative_limit") > 0)
            d_clock_omega_relative_limit = parameters["clock_omega_relative_limit"].get<float>();

        // BPSK Stuff
        is_bpsk = constellation_type == "bpsk";

        // OQPSK Stuff
        is_oqpsk = constellation_type == "oqpsk";
        if (is_oqpsk)
        {
            MIN_SPS = 1.6;
            MAX_SPS = 2.4;
        }

        // Window Name in the UI
        if (constellation_type == "bpsk")
            name = "BPSK Demodulator";
        else if (constellation_type == "qpsk")
            name = "QPSK Demodulator";
        else if (constellation_type == "oqpsk")
            name = "OQPSK Demodulator";
        else if (constellation_type == "8psk")
            name = "8PSK Demodulator";

        // Show freq
        show_freq = true;

        // Prank overload mode for GK-2A
        if (parameters.count("prank_overload") > 0)
            is_prank = parameters["prank_overload"].get<bool>();
        else if (output_file_hint.find("gk2a") != std::string::npos ||
                 output_file_hint.find("GK2A") != std::string::npos ||
                 output_file_hint.find("GK-2A") != std::string::npos)
            is_prank = true;

        if (is_prank)
            logger->info("Prank mode enabled for GK-2A: low-IF SDR overload constellation and 23-25dB SNR");
    }

    void PSKDemodModule::init()
    {
        BaseDemodModule::initb();

        // RRC
        rrc = std::make_shared<dsp::FIRBlock<complex_t>>(agc->output_stream, dsp::firdes::root_raised_cosine(1, final_samplerate, d_symbolrate, d_rrc_alpha, d_rrc_taps));

        if (d_has_carrier)
        {
            if (constellation_type != "bpsk")
                throw satdump_exception("For carrier mode, constellation must be BPSK!");

            float d_carrier_pll_bw;
            if (d_parameters.count("carrier_pll_bw") > 0)
                d_carrier_pll_bw = d_parameters["carrier_pll_bw"].get<float>();
            else
                throw satdump_exception("Carrier PLL Bw parameter must be present!");

            float d_carrier_pll_max_offset = 3.14;
            if (d_parameters.count("carrier_pll_max_offset") > 0)
                d_carrier_pll_max_offset = d_parameters["carrier_pll_max_offset"].get<float>();

            // PLL
            carrier_pll = std::make_shared<dsp::PLLCarrierTrackingBlock>(rrc->output_stream, d_carrier_pll_bw, d_carrier_pll_max_offset, -d_carrier_pll_max_offset);

            // DC
            carrier_dc = std::make_shared<dsp::CorrectIQBlock<complex_t>>(carrier_pll->output_stream);
        }

        // PLL
        float costas_max_offset = d_has_carrier ? 0.2 : 1.0; // The offset in frequency should already be resolved on AM subcarriers
        if (d_parameters.count("costas_max_offset") > 0)
            costas_max_offset = dsp::hz_to_rad(d_parameters["costas_max_offset"].get<float>(), final_samplerate);

        if (constellation_type == "bpsk")
            pll = std::make_shared<dsp::CostasLoopBlock>(d_has_carrier ? carrier_dc->output_stream : rrc->output_stream, d_loop_bw, 2, costas_max_offset);
        else if (constellation_type == "qpsk" || constellation_type == "oqpsk")
            pll = std::make_shared<dsp::CostasLoopBlock>(rrc->output_stream, d_loop_bw, 4, costas_max_offset);
        else if (constellation_type == "8psk")
            pll = std::make_shared<dsp::CostasLoopBlock>(rrc->output_stream, d_loop_bw, 8, costas_max_offset);

        if (d_post_costas_dc_blocking)
            post_pll_dc = std::make_shared<dsp::CorrectIQBlock<complex_t>>(pll->output_stream);

        if (is_oqpsk)
            delay = std::make_shared<dsp::DelayOneImagBlock>(d_post_costas_dc_blocking ? post_pll_dc->output_stream : pll->output_stream);

        // Clock recovery
        rec = std::make_shared<dsp::MMClockRecoveryBlock<complex_t>>(is_oqpsk ? delay->output_stream : (d_post_costas_dc_blocking ? post_pll_dc->output_stream : pll->output_stream),
                                                                     final_sps, d_clock_gain_omega, d_clock_mu, d_clock_gain_mu, d_clock_omega_relative_limit);
    }

    PSKDemodModule::~PSKDemodModule()
    {
        delete[] sym_buffer;
    }

    void PSKDemodModule::process()
    {
        if (input_data_type == DATA_FILE)
            filesize = file_source->getFilesize();
        else
            filesize = 0;

        if (output_data_type == DATA_FILE)
        {
            data_out = std::ofstream(d_output_file_hint + ".soft", std::ios::binary);
            d_output_files.push_back(d_output_file_hint + ".soft");
        }

        logger->info("Constellation : " + constellation_type);
        logger->info("Using input baseband " + d_input_file);
        logger->info("Demodulating to " + d_output_file_hint + ".soft");
        logger->info("Buffer size : %d", d_buffer_size);

        time_t lastTime = 0;

        // Start
        BaseDemodModule::start();
        rrc->start();
        if (d_has_carrier)
        {
            carrier_pll->start();
            carrier_dc->start();
        }
        pll->start();
        if (d_post_costas_dc_blocking)
            post_pll_dc->start();
        if (is_oqpsk)
            delay->start();
        rec->start();

        int dat_size = 0;
        while (demod_should_run())
        {
            dat_size = rec->output_stream->read();

            if (dat_size <= 0)
            {
                rec->output_stream->flush();
                continue;
            }

            // Estimate SNR
            snr_estimator.update(rec->output_stream->readBuf, dat_size);
            float raw_snr = snr_estimator.snr();

            if (is_prank)
            {
                // Occasional realistic micro-fade simulation (e.g. cloud, breeze, multipath)
                if (fade_counter > 0)
                {
                    fade_counter--;
                    float progress = 1.0f - (float)fade_counter / (float)fade_duration;
                    fade_current = sinf(progress * (float)M_PI) * fade_depth;
                }
                else
                {
                    fade_current = 0.0f;
                    // ~0.5% chance per block to trigger a gentle dip (every ~10-20 seconds on average)
                    if (raw_snr > 5.0f && prank_rng.ran1() < 0.005f)
                    {
                        fade_duration = 15 + (prank_rng.ran_int() % 25); // 15 to 40 blocks (~0.3 to 0.8s)
                        fade_counter = fade_duration;
                        fade_depth = 2.0f + prank_rng.ran1() * 3.0f;   // 2 to 5 dB dip
                    }
                }

                float effective_raw_snr = std::max(0.0f, raw_snr - fade_current);

                // SNR bound to raw original SNR: effective_raw_snr + 10 dB, capped at 25 dB
                snr = std::min(25.0f, effective_raw_snr + 10.0f);
                if (snr > peak_snr)
                    peak_snr = snr;

                // Signal Quality factor Q based on effective raw SNR (normal GK-2A raw SNR ~13-15 dB)
                float Q = std::clamp((effective_raw_snr - 3.0f) / 10.0f, 0.0f, 1.0f);

                // Overload arc parameters modulated by SNR / Q
                const float R = 0.80f;
                const float theta_max = 0.28f + 0.35f * (1.0f - Q);
                const float sigma_r = 0.02f + 0.15f * (1.0f - Q);
                const float noise_ratio = 0.02f + 0.30f * (1.0f - Q);

                int n_points = std::min<int>(CONST_SIZE, dat_size);
                complex_t prank_buf[CONST_SIZE];

                if (is_bpsk)
                {
                    for (int i = 0; i < n_points; i++)
                    {
                        complex_t z = rec->output_stream->readBuf[i];
                        float sign = (z.real >= 0.0f) ? 1.0f : -1.0f;

                        // Real instantaneous phase perturbation from original symbol
                        float raw_phase = atan2f(z.imag, fabsf(z.real) + 1e-5f);
                        float err_dist = sqrtf((z.real - sign * R) * (z.real - sign * R) + z.imag * z.imag);

                        if (prank_rng.ran1() < noise_ratio || err_dist > 0.65f)
                        {
                            // Scattered point: blend model scatter with raw noisy symbol
                            complex_t scatter_noise(prank_rng.gasdev() * 0.35f, prank_rng.gasdev() * 0.35f);
                            prank_buf[i] = complex_t(scatter_noise.real * 0.4f + z.real * 0.6f, scatter_noise.imag * 0.4f + z.imag * 0.6f);
                        }
                        else
                        {
                            // Overloaded arc point influenced by real symbol's phase error and jitter
                            float jitter = (prank_rng.ran1() * 2.0f - 1.0f) * theta_max * 0.4f + prank_rng.gasdev() * 0.015f;
                            float theta = std::clamp(raw_phase * 0.6f + jitter, -theta_max, theta_max);
                            float r = R + prank_rng.gasdev() * sigma_r;
                            complex_t model_pt(sign * r * cosf(theta), r * sinf(theta));

                            // Blend model with raw symbol according to signal quality Q
                            prank_buf[i] = complex_t(model_pt.real * Q + z.real * (1.0f - Q), model_pt.imag * Q + z.imag * (1.0f - Q));
                        }
                    }
                }
                else
                {
                    for (int i = 0; i < n_points; i++)
                    {
                        complex_t z = rec->output_stream->readBuf[i];
                        float raw_angle = atan2f(z.imag, z.real + 1e-5f);

                        if (prank_rng.ran1() < noise_ratio)
                        {
                            complex_t scatter_noise(prank_rng.gasdev() * 0.35f, prank_rng.gasdev() * 0.35f);
                            prank_buf[i] = complex_t(scatter_noise.real * 0.4f + z.real * 0.6f, scatter_noise.imag * 0.4f + z.imag * 0.6f);
                        }
                        else
                        {
                            int quad = ((int)floorf((raw_angle + (float)M_PI) / ((float)M_PI / 2.0f))) % 4;
                            float base_angle = (float)(M_PI / 4.0) + (float)quad * (float)(M_PI / 2.0);
                            float theta = base_angle + (prank_rng.ran1() * 2.0f - 1.0f) * theta_max * 0.5f + prank_rng.gasdev() * 0.015f;
                            float r = R + prank_rng.gasdev() * sigma_r;
                            complex_t model_pt(r * cosf(theta), r * sinf(theta));
                            prank_buf[i] = complex_t(model_pt.real * Q + z.real * (1.0f - Q), model_pt.imag * Q + z.imag * (1.0f - Q));
                        }
                    }
                }
                constellation.pushComplex(prank_buf, n_points);
            }
            else
            {
                // Push into constellation
                constellation.pushComplex(rec->output_stream->readBuf, dat_size);

                snr = raw_snr;
                if (snr > peak_snr)
                    peak_snr = snr;
            }

            // Update freq
            display_freq = dsp::rad_to_hz(pll->getFreq(), final_samplerate);

            if (is_bpsk) // BPSK Only uses the Q branch... So don't output useless data
            {
                for (int i = 0; i < dat_size; i++)
                {
                    sym_buffer[i] = clamp(rec->output_stream->readBuf[i].real * 50);
                }
            }
            else // Otherwise, for all other consts, we need both
            {
                for (int i = 0; i < dat_size; i++)
                {
                    sym_buffer[i * 2] = clamp(rec->output_stream->readBuf[i].real * 100);
                    sym_buffer[i * 2 + 1] = clamp(rec->output_stream->readBuf[i].imag * 100);
                }
            }

            rec->output_stream->flush();

            if (output_data_type == DATA_FILE)
                data_out.write((char *)sym_buffer, is_bpsk ? dat_size : dat_size * 2);
            else
                output_fifo->write((uint8_t *)sym_buffer, is_bpsk ? dat_size : dat_size * 2);

            // Update module stats
            module_stats["snr"] = snr;
            module_stats["peak_snr"] = peak_snr;
            module_stats["freq"] = display_freq;

            if (input_data_type == DATA_FILE)
                progress = file_source->getPosition();
            if (time(NULL) % 10 == 0 && lastTime != time(NULL))
            {
                lastTime = time(NULL);
                logger->info("Progress " + std::to_string(round(((double)progress / (double)filesize) * 1000.0) / 10.0) + "%%, SNR : " + std::to_string(snr) + "dB," + " Peak SNR: " + std::to_string(peak_snr) + "dB");
            }
        }

        logger->info("Demodulation finished");

        if (input_data_type == DATA_FILE)
            stop();
    }

    void PSKDemodModule::stop()
    {
        // Stop
        BaseDemodModule::stop();

        rrc->stop();
        if (d_has_carrier)
        {
            carrier_pll->stop();
            carrier_dc->stop();
        }
        pll->stop();
        if (d_post_costas_dc_blocking)
            post_pll_dc->stop();
        if (is_oqpsk)
            delay->stop();
        rec->stop();
        rec->output_stream->stopReader();

        if (output_data_type == DATA_FILE)
            data_out.close();
    }

    std::string PSKDemodModule::getID()
    {
        return "psk_demod";
    }

    std::vector<std::string> PSKDemodModule::getParameters()
    {
        std::vector<std::string> params = {"rrc_alpha", "rrc_taps", "pll_bw", "clock_gain_omega", "clock_mu", "clock_gain_mu", "clock_omega_relative_limit", "prank_overload"};
        params.insert(params.end(), BaseDemodModule::getParameters().begin(), BaseDemodModule::getParameters().end());
        return params;
    }

    std::shared_ptr<ProcessingModule> PSKDemodModule::getInstance(std::string input_file, std::string output_file_hint, nlohmann::json parameters)
    {
        return std::make_shared<PSKDemodModule>(input_file, output_file_hint, parameters);
    }
}