#pragma once

#include "core/module.h"
#include <complex>
#include <fstream>

namespace network
{
    class NetworkServerModule : public ProcessingModule
    {
    protected:
        uint8_t *buffer;

        std::string mode = "default";

        std::ifstream data_in;
        std::atomic<uint64_t> filesize{0};
        std::atomic<uint64_t> progress{0};
        std::atomic<uint64_t> send_errors{0};
        bool ts_pacing = false;
        double udp_bitrate = 0;

        int pkt_size;
        std::string address;
        int port;

    public:
        NetworkServerModule(std::string input_file, std::string output_file_hint, nlohmann::json parameters);
        ~NetworkServerModule();
        void process();
        void drawUI(bool window);
        std::vector<ModuleDataType> getInputTypes();
        std::vector<ModuleDataType> getOutputTypes();

    public:
        static std::string getID();
        virtual std::string getIDM() { return getID(); };
        static std::vector<std::string> getParameters();
        static std::shared_ptr<ProcessingModule> getInstance(std::string input_file, std::string output_file_hint, nlohmann::json parameters);
    };
}
