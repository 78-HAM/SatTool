#include "module_s2_ts_extractor.h"
#include "logger.h"
#include "imgui/imgui.h"
#include "common/codings/dvb-s2/bbframe_ts_parser.h"
#include "common/utils.h"
#include "core/exception.h"

#include "codings/dvb-s2/bbframe_bch.h"
#include "codings/dvb-s2/modcod_to_cfg.h"
#include <cctype>

namespace dvbs2
{
    S2TStoTCPModule::S2TStoTCPModule(std::string input_file, std::string output_file_hint, nlohmann::json parameters) : ProcessingModule(input_file, output_file_hint, parameters)
    {
        if (d_parameters.contains("bb_size"))
        {
            bbframe_size = d_parameters["bb_size"].get<int>();
        }
        else
        {
            int d_modcod = -1;
            bool d_shortframes = false;
            bool d_pilots = false;
            bool d_pilots_set = false;
            bool d_s2x = false;

            if (parameters.count("modcod") > 0)
                d_modcod = parameters["modcod"].get<int>();
            else
                throw satdump_exception("MODCOD parameter must be present!");

            if (parameters.count("shortframes") > 0)
                d_shortframes = parameters["shortframes"].get<bool>();

            if (parameters.count("pilots") > 0)
            {
                d_pilots = parameters["pilots"].get<bool>();
                d_pilots_set = true;
            }

            if (parameters.count("standard") > 0)
            {
                std::string standard = parameters["standard"].get<std::string>();
                for (char &c : standard)
                    c = (char)std::tolower((unsigned char)c);
                d_s2x = standard == "dvb-s2x" || standard == "s2x";
                if (!d_s2x && standard != "dvb-s2" && standard != "s2")
                    throw satdump_exception("standard must be DVB-S2 or DVB-S2X");
            }
            if (parameters.count("s2x") > 0)
                d_s2x = parameters["s2x"].get<bool>();
            if (parameters.count("standard") == 0 && parameters.count("s2x") == 0 && d_modcod >= 128)
                d_s2x = true;

            // Parse modcod number
            const bool extended_s2x = d_s2x && d_modcod >= 128;
            auto cfg = extended_s2x ? dvbs2::get_dvbs2x_cfg(d_modcod, d_pilots_set ? int(d_pilots) : -1)
                                    : dvbs2::get_dvbs2_cfg(d_modcod, d_shortframes, d_pilots);

            dvbs2::BBFrameBCH bch_decoder(cfg.framesize, cfg.coderate, extended_s2x);
            bbframe_size = bch_decoder.dataSize();
        }
        if (bbframe_size < 80 || bbframe_size > FRAME_SIZE_NORMAL || bbframe_size % 8 != 0)
            throw satdump_exception("Invalid BBFRAME bit length");
    }

    std::vector<ModuleDataType> S2TStoTCPModule::getInputTypes()
    {
        return {DATA_FILE, DATA_STREAM};
    }

    std::vector<ModuleDataType> S2TStoTCPModule::getOutputTypes()
    {
        return {DATA_FILE, DATA_STREAM};
    }

    S2TStoTCPModule::~S2TStoTCPModule()
    {
    }

    void S2TStoTCPModule::process()
    {
        if (input_data_type == DATA_FILE)
            filesize = getFilesize(d_input_file);
        else
            filesize = 0;
        if (input_data_type == DATA_FILE)
        {
            data_in = std::ifstream(d_input_file, std::ios::binary);
            if (!data_in)
                throw satdump_exception("Could not open BBFRAME input file");
        }
        if (output_data_type == DATA_FILE)
        {
            data_out = std::ofstream(d_output_file_hint + ".ts", std::ios::binary);
            if (!data_out)
                throw satdump_exception("Could not open TS output file");
            d_output_files.push_back(d_output_file_hint + ".ts");
        }

        logger->info("Using input bbframes " + d_input_file);

        time_t lastTime = 0;

        std::vector<uint8_t> bb_buffer(bbframe_size / 8);
        uint8_t ts_frames[188 * 1000];

        dvbs2::BBFrameTSParser ts_extractor(bbframe_size);

        while (input_data_type == DATA_FILE ? !data_in.eof() : input_active.load())
        {
            // Read buffer
            if (input_data_type == DATA_FILE)
            {
                data_in.read((char *)bb_buffer.data(), bb_buffer.size());
                if (data_in.gcount() != (std::streamsize)bb_buffer.size())
                    break;
                progress += bb_buffer.size();
            }
            else if (input_fifo->read(bb_buffer.data(), bb_buffer.size()) != (int)bb_buffer.size())
                break;

            int ts_cnt = ts_extractor.work(bb_buffer.data(), 1, ts_frames, sizeof(ts_frames));

            if (ts_cnt > 0)
            {
                if (output_data_type == DATA_FILE)
                {
                    data_out.write((char *)ts_frames, ts_cnt * 188);
                    if (!data_out)
                        throw satdump_exception("Error writing TS output file");
                }
                else if (output_fifo->write(ts_frames, ts_cnt * 188) < 0)
                    break;
                ts_packets += ts_cnt;
            }

            if (input_data_type == DATA_FILE && filesize > 0 && time(NULL) % 10 == 0 && lastTime != time(NULL))
            {
                lastTime = time(NULL);
                logger->info("Progress " + std::to_string(round(((double)progress / (double)filesize) * 1000.0) / 10.0) + "%%");
            }
        }

        data_out.close();
        if (input_data_type == DATA_FILE)
            data_in.close();
    }

    void S2TStoTCPModule::drawUI(bool window)
    {
        ImGui::Begin("DVB-S2/S2X TS Extractor", NULL, window ? 0 : NOWINDOW_FLAGS);
        ImGui::BeginGroup();
        {
            ImGui::Text("TS packets: %s", std::to_string(ts_packets.load()).c_str());
        }
        ImGui::EndGroup();

        if (!streamingInput && filesize > 0)
            ImGui::ProgressBar((double)progress / (double)filesize, ImVec2(ImGui::GetContentRegionAvail().x, 20 * ui_scale));

        ImGui::End();
    }

    std::string S2TStoTCPModule::getID()
    {
        return "dvbs2_ts_extractor";
    }

    std::vector<std::string> S2TStoTCPModule::getParameters()
    {
        return {"bb_size", "modcod", "shortframes", "pilots", "standard", "s2x"};
    }

    std::shared_ptr<ProcessingModule> S2TStoTCPModule::getInstance(std::string input_file, std::string output_file_hint, nlohmann::json parameters)
    {
        return std::make_shared<S2TStoTCPModule>(input_file, output_file_hint, parameters);
    }
}
