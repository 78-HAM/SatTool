#include "common/net/udp.h"
#include "common/codings/dvb-s2/bbframe_ts_parser.h"
#include "common/mpeg_ts/ts_pacer.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>

using Bytes = std::vector<uint8_t>;
static void require(bool ok, const char *message)
{
    if (!ok) throw std::runtime_error(message);
}

// Reference transmitter: MSB-first TS CRC and LSB-first BBHEADER CRC.
static uint8_t ts_crc(const uint8_t *data, size_t size)
{
    uint8_t crc = 0;
    for (size_t n = 0; n < size; ++n)
        for (int bit = 7; bit >= 0; --bit)
        {
            const bool feedback = (crc >> 7) ^ ((data[n] >> bit) & 1);
            crc <<= 1;
            if (feedback) crc ^= 0xd5;
        }
    return crc;
}

static void header_crc(Bytes &frame)
{
    uint8_t crc = 0;
    for (int bit = 0; bit < 72; ++bit)
    {
        const bool feedback = (crc & 1) ^ ((frame[bit / 8] >> (7 - bit % 8)) & 1);
        crc >>= 1;
        if (feedback) crc ^= 0xab;
    }
    frame[9] = 0;
    for (int bit = 0; bit < 8; ++bit)
        frame[9] |= ((crc >> bit) & 1) << (7 - bit);
}

static Bytes make_ts(int count)
{
    Bytes ts(count * 188);
    for (int n = 0; n < count; ++n)
    {
        uint8_t *p = ts.data() + n * 188;
        for (int i = 0; i < 188; ++i) p[i] = uint8_t(n * 31 + i * 13);
        p[0] = 0x47; p[1] = 0x01; p[2] = 0; p[3] = 0x10 | (n & 15);
    }
    return ts;
}

static Bytes adapt(const Bytes &ts)
{
    Bytes wire;
    uint8_t previous_crc = 0;
    for (size_t n = 0; n < ts.size(); n += 188)
    {
        wire.push_back(previous_crc);
        wire.insert(wire.end(), ts.begin() + n + 1, ts.begin() + n + 188);
        previous_crc = ts_crc(ts.data() + n + 1, 187);
    }
    wire.push_back(previous_crc); // Next packet marker validates the last packet.
    return wire;
}

static Bytes bbframe(const Bytes &wire, size_t offset, int length, int frame_bytes)
{
    Bytes frame(frame_bytes, 0xee); // Padding must not reach TS output.
    frame[0] = 0xf0; frame[1] = 0; // TS, SIS, CCM, no ISSY/NPD.
    frame[2] = 1504 >> 8; frame[3] = 1504 & 255;
    frame[4] = (length * 8) >> 8; frame[5] = (length * 8) & 255;
    frame[6] = 0x47;
    size_t next_marker = (188 - offset % 188) % 188;
    const unsigned syncd = next_marker < size_t(length) ? unsigned(next_marker * 8) : 0xffff;
    frame[7] = syncd >> 8; frame[8] = syncd & 255;
    header_crc(frame);
    std::copy_n(wire.begin() + offset, length, frame.begin() + 10);
    return frame;
}

static Bytes parse(dvbs2::BBFrameTSParser &parser, Bytes frames, int count = 1)
{
    Bytes output(frames.size() + 376, 0xa5);
    const int packets = parser.work(frames.data(), count, output.data(), int(output.size()) - 188);
    require(std::all_of(output.end() - 188, output.end(), [](uint8_t b) { return b == 0xa5; }), "Output canary overwritten");
    output.resize(packets * 188);
    return output;
}

static void parser_tests()
{
    const auto ts = make_ts(4000);
    const auto wire = adapt(ts);
    std::mt19937 random(0x5a2);
    for (int frame_bytes : {384, 879, 2316, 7274})
    {
        dvbs2::BBFrameTSParser parser(frame_bytes * 8);
        Bytes result;
        size_t offset = 0;
        while (offset < wire.size())
        {
            const int length = int(std::min<size_t>(1 + random() % (frame_bytes - 10), wire.size() - offset));
            auto output = parse(parser, bbframe(wire, offset, length, frame_bytes));
            result.insert(result.end(), output.begin(), output.end());
            offset += length;
        }
        require(result == ts, "TS changed across BBFRAME boundaries, short DATAFIELDs or padding");
    }

    // Multi-frame calls must restart the input byte offset for each BBFRAME.
    dvbs2::BBFrameTSParser batch(384 * 8);
    Bytes frames, expected;
    for (int n = 0; n < 80; ++n)
    {
        auto frame = bbframe(wire, n * 374, 374, 384);
        frames.insert(frames.end(), frame.begin(), frame.end());
    }
    expected.assign(ts.begin(), ts.begin() + ((80 * 374 - 1) / 188) * 188);
    require(parse(batch, frames, 80) == expected, "Batched BBFRAME parsing corrupted TS");

    // A packet completed exactly at the frame edge waits for its CRC in the next call.
    dvbs2::BBFrameTSParser edge(384 * 8);
    require(parse(edge, bbframe(wire, 0, 188, 384)).empty(), "Packet emitted before CRC arrived");
    auto crc_frame = bbframe(wire, 188, 1, 384);
    crc_frame[10] ^= 1;
    auto error = parse(edge, crc_frame);
    auto marked = Bytes(ts.begin(), ts.begin() + 188); marked[1] |= 0x80;
    require(error == marked, "CRC failure did not mark retained packet TEI");

    // The old parser underflowed on SYNCD=0xffff and on out-of-range markers.
    for (unsigned syncd : {0xffffu, 3000u, 1u})
    {
        dvbs2::BBFrameTSParser parser(384 * 8);
        auto invalid = bbframe(wire, 0, 40, 384);
        invalid[7] = syncd >> 8; invalid[8] = syncd & 255; header_crc(invalid);
        require(parse(parser, invalid).empty(), "Invalid/no marker emitted TS");
        require(parse(parser, bbframe(wire, 0, 374, 384)) == Bytes(ts.begin(), ts.begin() + 188), "Failed to recover after invalid frame");
    }
    dvbs2::BBFrameTSParser loss(384 * 8);
    parse(loss, bbframe(wire, 0, 374, 384));
    auto corrupt = bbframe(wire, 374, 374, 384); corrupt[9] ^= 1;
    require(parse(loss, corrupt).empty(), "Bad BBHEADER was accepted");
    auto recovered = parse(loss, bbframe(wire, 748, 374, 384));
    require(recovered == Bytes(ts.begin() + 4 * 188, ts.begin() + 5 * 188), "Lost-frame recovery joined unrelated fragments");
    dvbs2::BBFrameTSParser dropped(384 * 8);
    parse(dropped, bbframe(wire, 0, 374, 384));
    require(parse(dropped, bbframe(wire, 748, 374, 384)) == recovered, "SYNCD mismatch did not recover a missing frame");

    dvbs2::BBFrameTSParser tiny(384 * 8);
    auto frame = bbframe(wire, 0, 374, 384);
    uint8_t guard[188]{};
    bool rejected = false;
    try { tiny.work(frame.data(), 1, guard, 187); } catch (const std::length_error &) { rejected = true; }
    require(rejected, "Undersized output was not rejected");
    std::cout << "TS: 16000 reference packets, batched frames, CRC/TEI, marker bounds and lost-frame recovery passed\n";
}

static void put_pcr(uint8_t *p, uint64_t ticks, int pid = 256, bool discontinuity = false)
{
    constexpr uint64_t wrap = (uint64_t(1) << 33) * 300;
    ticks %= wrap;
    const uint64_t base = ticks / 300;
    const unsigned ext = ticks % 300;
    p[0] = 0x47; p[1] = (pid >> 8) & 31; p[2] = pid & 255;
    p[3] = 0x30; p[4] = 7; p[5] = 0x10 | (discontinuity ? 0x80 : 0);
    p[6] = base >> 25; p[7] = base >> 17; p[8] = base >> 9; p[9] = base >> 1;
    p[10] = ((base & 1) << 7) | 0x7e | (ext >> 8); p[11] = ext & 255;
}

static void pacing_tests()
{
    using Pacer = mpeg_ts::TSPacer;
    auto now = Pacer::Clock::now();
    Bytes data(1316, 0xff);
    Pacer pacer(true);
    constexpr uint64_t wrap = (uint64_t(1) << 33) * 300;
    put_pcr(data.data(), wrap - 4 * 5076);
    require(pacer.schedule(data.data(), data.size(), now) == now, "First PCR should acquire clock");
    put_pcr(data.data(), wrap + 3 * 5076);
    pacer.schedule(data.data(), data.size(), now);
    require(std::abs(pacer.bitrate() - 8000000) < 1, "PCR wrap broke rate estimation");
    auto next = pacer.schedule(data.data(), data.size(), now);
    require(std::abs(std::chrono::duration<double>(next - now).count() - 0.001316) < 1e-6, "Datagrams were not spaced at TS rate");
    put_pcr(data.data(), 10, 300);
    pacer.schedule(data.data(), data.size(), now);
    require(std::abs(pacer.bitrate() - 8000000) < 1, "Other program PCR changed selected clock");
    require(pacer.schedule(data.data(), data.size(), now + std::chrono::seconds(2)) == now + std::chrono::seconds(2), "Input pause caused catch-up burst");
    put_pcr(data.data(), 100, 256, true);
    pacer.schedule(data.data(), data.size(), now);
    require(pacer.bitrate() == 0, "PCR discontinuity did not reacquire clock");
    Pacer fixed(false, 4000000);
    fixed.schedule(data.data(), data.size(), now);
    require(fixed.schedule(data.data(), data.size(), now) > now, "Fixed-rate pacing missing");
    std::cout << "Pacing: PCR wrap, PID selection, discontinuity, input pause and fixed rate passed\n";
}

static void udp_tests()
{
    // Bind an OS-selected loopback port, then reopen that same port after sending
    // while no listener exists. Exercise the real Windows socket handle and ICMP path.
    net::UDPClient startup("127.0.0.1", 9);
#ifdef _WIN32
    using Socket = SOCKET;
    const Socket invalid = INVALID_SOCKET;
    auto close_receiver = [](Socket s) { closesocket(s); };
#else
    using Socket = int;
    const Socket invalid = -1;
    auto close_receiver = [](Socket s) { close(s); };
#endif
    auto make_receiver = [&](int port)
    {
        Socket s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        require(s != invalid, "Receiver socket failed");
        sockaddr_in address{}; address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); address.sin_port = htons(port);
        require(bind(s, (sockaddr *)&address, sizeof(address)) == 0, "Receiver bind failed");
        int buffer = 1024 * 1024;
        setsockopt(s, SOL_SOCKET, SO_RCVBUF, (const char *)&buffer, sizeof(buffer));
#ifdef _WIN32
        DWORD timeout = 3000;
#else
        timeval timeout{3, 0};
#endif
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
        return s;
    };
    Socket receiver = make_receiver(0);
    sockaddr_in address{};
#ifdef _WIN32
    int address_size = sizeof(address);
#else
    socklen_t address_size = sizeof(address);
#endif
    require(getsockname(receiver, (sockaddr *)&address, &address_size) == 0, "Receiver port lookup failed");
    const int port = ntohs(address.sin_port);
    net::UDPClient sender("127.0.0.1", port);
    close_receiver(receiver);
    Bytes data(1316, 0xff);
    for (int n = 0; n < 16; ++n) sender.send(data.data(), int(data.size()));
    receiver = make_receiver(port);
    constexpr int count = 1000;
    std::exception_ptr receive_error;
    std::thread listener([&]
    {
        try
        {
            Bytes received(2048);
            for (int n = 0; n < count; ++n)
            {
                const int len = recvfrom(receiver, (char *)received.data(), int(received.size()), 0, nullptr, nullptr);
                require(len == 1316, "UDP datagram lost, truncated or wrong size");
                uint32_t serial = 0; std::memcpy(&serial, received.data() + 20, sizeof(serial));
                require(serial == unsigned(n), "UDP datagram lost, duplicated or reordered");
            }
        }
        catch (...) { receive_error = std::current_exception(); }
    });
    const auto start = mpeg_ts::TSPacer::Clock::now();
    mpeg_ts::TSPacer pacer(true);
    for (uint32_t n = 0; n < count; ++n)
    {
        put_pcr(data.data(), uint64_t(n) * 7 * 5076);
        std::memcpy(data.data() + 20, &n, sizeof(n));
        std::this_thread::sleep_until(pacer.schedule(data.data(), data.size(), mpeg_ts::TSPacer::Clock::now()));
        sender.send(data.data(), int(data.size()));
    }
    listener.join();
    close_receiver(receiver);
    if (receive_error) std::rethrow_exception(receive_error);
    const double seconds = std::chrono::duration<double>(mpeg_ts::TSPacer::Clock::now() - start).count();
    require(seconds >= 1.0 && seconds < 10, "PCR pacing did not spread batch over media time");
    std::cout << "UDP: receiver reopen and 1000 ordered 1316-byte datagrams without loss passed (" << seconds << " s)\n";
}

int main()
{
    try { parser_tests(); pacing_tests(); udp_tests(); }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
