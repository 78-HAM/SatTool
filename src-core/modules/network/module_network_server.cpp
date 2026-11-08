#include "module_network_server.h"
#include "logger.h"
#include "imgui/imgui.h"
#include "common/utils.h"
#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>
#include "common/net/udp.h"
#include "core/exception.h"
#include "common/mpeg_ts/ts_pacer.h"
#include <thread>

namespace network
{
    NetworkServerModule::NetworkServerModule(std::string input_file, std::string output_file_hint, nlohmann::json parameters)
        : ProcessingModule(input_file, output_file_hint, parameters)
    {
        if (parameters.contains("server_mode"))
            mode = parameters["server_mode"].get<std::string>();

        if (parameters.count("pkt_size") > 0)
            pkt_size = parameters["pkt_size"].get<int>();
        else
            throw satdump_exception("pkt_size parameter must be present!");

        if (parameters.count("server_address") > 0)
            address = parameters["server_address"].get<std::string>();
        else
            throw satdump_exception("server_address parameter must be present!");

        if (parameters.count("server_port") > 0)
            port = parameters["server_port"].get<int>();
        else
            throw satdump_exception("server_port parameter must be present!");

        if (pkt_size <= 0 || (mode == "udp_send" && pkt_size > 65507))
            throw satdump_exception("Invalid network packet size");
        if (port < 1 || port > 65535)
            throw satdump_exception("Network port must be between 1 and 65535");
        ts_pacing = parameters.value("udp_ts_pacing", false);
        udp_bitrate = parameters.value("udp_bitrate", 0.0);
        if (ts_pacing && pkt_size % 188 != 0)
            throw satdump_exception("UDP TS packet size must be a multiple of 188");
        mpeg_ts::TSPacer validate_pacing(ts_pacing, udp_bitrate);
        buffer = new uint8_t[pkt_size];
    }

    std::vector<ModuleDataType> NetworkServerModule::getInputTypes()
    {
        return {DATA_FILE, DATA_STREAM};
    }

    std::vector<ModuleDataType> NetworkServerModule::getOutputTypes()
    {
        return {DATA_FILE};
    }

    NetworkServerModule::~NetworkServerModule()
    {
        delete[] buffer;
    }

    void NetworkServerModule::process()
    {
        if (input_data_type == DATA_FILE)
            filesize = getFilesize(d_input_file);
        else
            filesize = 0;
        if (input_data_type == DATA_FILE)
        {
            data_in = std::ifstream(d_input_file, std::ios::binary);
            if (!data_in)
                throw satdump_exception("Could not open network input file");
        }

        logger->info("Using input frames " + d_input_file);

        if (mode == "default")
        {
            nng_socket sock;
            nng_listener listener;

            logger->info("Opening TCP socket on " + std::string("tcp://" + address + ":" + std::to_string(port)));

            nng_pub0_open_raw(&sock);
            nng_listener_create(&listener, sock, std::string("tcp://" + address + ":" + std::to_string(port)).c_str());
            nng_listener_start(listener, NNG_FLAG_NONBLOCK);

            while (input_data_type == DATA_FILE ? !data_in.eof() : input_active.load())
            {
                // Read a buffer
                if (input_data_type == DATA_FILE)
                    data_in.read((char *)buffer, pkt_size);
                else
                    input_fifo->read((uint8_t *)buffer, pkt_size);

                nng_send(sock, buffer, pkt_size, NNG_FLAG_NONBLOCK);

                if (input_data_type == DATA_FILE)
                    progress = data_in.tellg();
            }

            nng_listener_close(listener);
        }
        else if (mode == "udp_send")
        {
            net::UDPClient udp_send(address.c_str(), port);
            mpeg_ts::TSPacer pacer(ts_pacing, udp_bitrate);
            auto last_error = std::chrono::steady_clock::time_point{};
            bool error_logged = false;

            while (input_data_type == DATA_FILE ? !data_in.eof() : input_active.load())
            {
                // Read a buffer
                int received = pkt_size;
                if (input_data_type == DATA_FILE)
                {
                    data_in.read((char *)buffer, pkt_size);
                    received = (int)data_in.gcount();
                }
                else if (input_fifo->read(buffer, pkt_size) != pkt_size)
                    break;
                if (received == 0)
                    break;
                if (ts_pacing && received % 188 != 0)
                {
                    logger->warn("Ignoring incomplete TS packet at end of input");
                    received -= received % 188;
                    if (received == 0)
                        break;
                }

                std::this_thread::sleep_until(pacer.schedule(buffer, received, mpeg_ts::TSPacer::Clock::now()));
                try
                {
                    udp_send.send(buffer, received);
                }
                catch (const std::runtime_error &error)
                {
                    ++send_errors;
                    const auto now = std::chrono::steady_clock::now();
                    if (!error_logged || now - last_error >= std::chrono::seconds(1))
                    {
                        logger->warn("%s; UDP output will continue", error.what());
                        last_error = now;
                        error_logged = true;
                    }
                }

                if (input_data_type == DATA_FILE)
                    progress += received;
            }
        }

        if (input_data_type == DATA_FILE)
            data_in.close();
    }

    void NetworkServerModule::drawUI(bool window)
    {
        ImGui::Begin("Network Server", NULL, window ? 0 : NOWINDOW_FLAGS);

        ImGui::Text("Address  : ");
        ImGui::SameLine();
        ImGui::TextColored(style::theme.green, "%s", address.c_str());

        ImGui::Text("Port    : ");
        ImGui::SameLine();
        ImGui::TextColored(style::theme.green, UITO_C_STR(port));

        if (mode == "udp_send")
            ImGui::Text("UDP send errors: %s", std::to_string(send_errors.load()).c_str());

        if (!streamingInput && filesize > 0)
            ImGui::ProgressBar((double)progress / (double)filesize, ImVec2(ImGui::GetContentRegionAvail().x, 20 * ui_scale));

        ImGui::End();
    }

    std::string NetworkServerModule::getID()
    {
        return "network_server";
    }

    std::vector<std::string> NetworkServerModule::getParameters()
    {
        return {"server_mode", "server_address", "server_port", "pkt_size", "udp_ts_pacing", "udp_bitrate"};
    }

    std::shared_ptr<ProcessingModule> NetworkServerModule::getInstance(std::string input_file, std::string output_file_hint, nlohmann::json parameters)
    {
        return std::make_shared<NetworkServerModule>(input_file, output_file_hint, parameters);
    }
}
