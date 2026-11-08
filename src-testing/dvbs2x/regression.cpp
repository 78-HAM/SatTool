#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
#include "codings/dvb-s2/modcod_to_cfg.h"
#include "codings/dvb-s2/bbframe_ldpc.h"
#include "codings/dvb-s2/bbframe_bch.h"
#include "codings/dvb-s2/bbframe_descramble.h"
#include "dvbs2/dvbs2_pl_sync.h"
#include "dvbs2/dvbs2_pll.h"
#include "dvbs2/dvbs2_bb_to_soft.h"
#include "reference_constellations.h"

// logger.cpp normally obtains this from SatDump's application initialization.
namespace satdump { std::string user_path; }

struct Profile
{
    int pls, kbch, nbch, mapping, variant;
    const char *order;
};

// EN 302 307-2 / gr-dvbs2 physical_cc, interleaver_bb and modulator_bc.
// Keep expected frame sizes and column orders independent of get_dvbs2x_cfg.
static const Profile profiles[] = {
    {132,18528,18720,0,0,"01"}, {134,28968,29160,0,0,"01"}, {136,35448,35640,0,0,"01"},
    {138,35808,36000,2,0,"012"}, {140,37248,37440,2,1,"012"},
    {142,41208,41400,1,0,"012"}, {144,44808,45000,1,0,"102"}, {146,46608,46800,1,0,"102"},
    {148,32208,32400,4,2,"3210"}, {150,34368,34560,4,2,"2310"}, {152,35808,36000,4,2,"2301"},
    {154,37248,37440,3,0,"3201"}, {156,38688,38880,3,0,"3210"}, {158,38688,38880,4,0,"0123"},
    {160,40128,40320,3,2,"3012"}, {162,41208,41400,3,6,"3021"}, {164,43008,43200,4,1,"0123"},
    {166,44808,45000,3,6,"2310"}, {168,46608,46800,3,7,"3021"},
    {170,50208,50400,3,1,"3210"}, {172,55248,55440,3,4,"0321"},
    {174,43040,43200,5,0,"21430"}, {178,45888,46080,6,0,"40312"},
    {180,47328,47520,6,1,"40312"}, {182,50208,50400,6,2,"40213"},
    {216,3792,3960,0,0,"01"}, {218,4152,4320,0,0,"01"}, {220,4872,5040,0,0,"01"},
    {222,7392,7560,0,0,"01"}, {224,8472,8640,0,0,"01"}, {226,11352,11520,0,0,"01"},
    {228,7392,7560,1,0,"102"}, {230,8472,8640,1,0,"102"},
    {232,9192,9360,1,0,"102"}, {234,11352,11520,1,0,"012"},
    {236,7392,7560,3,3,"2103"}, {238,8472,8640,3,2,"2103"},
    {240,9192,9360,3,0,"2130"}, {242,9552,9720,3,0,"3201"}, {244,11352,11520,3,7,"0123"},
    {246,10632,10800,5,1,"41230"}, {248,11352,11520,5,2,"10423"},
};

static void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

static std::vector<complex_t> reference_points(const Profile &p)
{
    static ReferenceConstellations r;
    std::vector<complex_t> points(1 << std::strlen(p.order));
    for (size_t i = 0; i < points.size(); ++i)
        switch (p.mapping)
        {
        case 0: points[i] = r.m_qpsk[i]; break;
        case 1: points[i] = r.m_8psk[i]; break;
        case 2: points[i] = r.m_8apsk[i][p.variant]; break;
        case 3: points[i] = r.m_16apsk[i][p.variant]; break;
        case 4: points[i] = r.m_8_8apsk[i][p.variant]; break;
        case 5: points[i] = r.m_4_12_16apsk[i][p.variant]; break;
        case 6: points[i] = r.m_4_8_4_16apsk[i][p.variant]; break;
        case 7: points[i] = r.m_32apsk[i][p.variant]; break;
        }
    // The receiver's AGC targets average absolute amplitude, not outer radius.
    float mean = 0;
    for (auto point : points) mean += point.norm() / points.size();
    for (auto &point : points) point = point / mean;
    return points;
}

static int parity(uint32_t x)
{
    x ^= x >> 16; x ^= x >> 8; x ^= x >> 4; x ^= x >> 2; x ^= x >> 1;
    return x & 1;
}

// gr-dvbs2 physical_cc's independent parallel-tap scrambler, as opposed to
// the receiver's precomputed 2^17-delayed sequence.
struct ReferenceScrambler
{
    uint32_t x = 1, y = 0x3ffff;
    explicit ReferenceScrambler(int gold)
    {
        while (gold--) x = (((x ^ (x >> 7)) & 1) << 17) | (x >> 1);
    }
    complex_t next(complex_t v)
    {
        int angle = ((parity(x & 0x8050) ^ parity(y & 0xff60)) << 1) | ((x ^ y) & 1);
        x = (x >> 1) | (parity(x & 0x81) << 17);
        y = (y >> 1) | (parity(y & 0x4a1) << 17);
        switch (angle)
        {
        case 1: return {-v.imag, v.real};
        case 2: return {-v.real, -v.imag};
        case 3: return {v.imag, -v.real};
        default: return v;
        }
    }
};

static uint64_t reference_pls(int code)
{
    static const uint32_t g[] = {0x90ac2ddd,0x55555555,0x33333333,0x0f0f0f0f,0x00ff00ff,0x0000ffff,0xffffffff};
    static const int scramble[] = {
        0,1,1,1,0,0,0,1,1,0,0,1,1,1,0,1,1,0,0,0,0,0,1,1,1,1,0,0,1,0,0,1,
        0,1,0,1,0,0,1,1,0,1,0,0,0,0,1,0,0,0,1,0,1,1,0,1,1,1,1,1,1,0,1,0};
    uint32_t temp = 0;
    for (int i = 0; i < 7; ++i) if (code & (128 >> i)) temp ^= g[i];
    uint64_t result = 0;
    for (int i = 0; i < 64; ++i)
    {
        int bit = (temp >> (31 - i / 2)) & 1;
        if (i & 1) bit ^= code & 1;
        result = (result << 1) | (bit ^ scramble[i]);
    }
    return result;
}

static complex_t header_symbol(int position, int code)
{
    static const int sof[] = {0,1,1,0,0,0,1,1,0,1,0,0,1,0,1,1,1,0,1,0,0,0,0,0,1,0};
    const float a = std::sqrt(0.5f);
    if (position < 26)
    {
        int sign = 1 - 2 * sof[position];
        return {(position & 1) ? -a * sign : a * sign, a * sign};
    }
    int bit = (reference_pls(code) >> (89 - position)) & 1;
    int sign = 1 - 2 * bit;
    if (code & 128) return {-a * sign, (position & 1) ? -a * sign : a * sign};
    return {(position & 1) ? -a * sign : a * sign, a * sign};
}

static void scramble_bb(std::vector<uint8_t> &frame, int bits)
{
    uint16_t sr = 0x4a80;
    for (int i = 0; i < bits; ++i)
    {
        int b = (sr ^ (sr >> 1)) & 1;
        frame[i / 8] ^= b << (7 - i % 8);
        sr = (sr >> 1) | (b << 14);
    }
}

static std::vector<complex_t> transmit(const Profile &p, const std::vector<uint8_t> &codeword, bool pilots, int gold, int pls_code)
{
    const int bits = std::strlen(p.order), n = codeword.size() * 8, rows = n / bits;
    const auto points = reference_points(p);
    std::vector<complex_t> result;
    for (int i = 0; i < 90; ++i) result.push_back(header_symbol(i, pls_code));
    ReferenceScrambler scramble(gold);
    for (int row = 0; row < rows; ++row)
    {
        if (pilots && row && row % 1440 == 0)
            for (int i = 0; i < 36; ++i) result.push_back(scramble.next({std::sqrt(0.5f),std::sqrt(0.5f)}));
        int label = 0;
        for (int bit = 0; bit < bits; ++bit)
        {
            // QPSK serializes adjacent bits; other modulations read bit planes.
            int index = bits == 2 ? row * 2 + bit : (p.order[bit] - '0') * rows + row;
            label = (label << 1) | ((codeword[index / 8] >> (7 - index % 8)) & 1);
        }
        result.push_back(scramble.next(points[label]));
    }
    return result;
}

static void check_headers_and_scrambling()
{
    dvbs2::s2_plscodes pls;
    for (int code = 0; code < 256; ++code)
    {
        require(pls.codewords[code] == reference_pls(code), "PLS codeword mismatch");
        for (int i = 0; i < 64; ++i)
            require((pls.symbols[code][i] - header_symbol(26 + i, code)).norm() < 1e-6f, "PLS symbol mismatch");
    }
    for (int gold : {0,1,12345,262141})
    {
        ReferenceScrambler tx(gold);
        dvbs2::S2Scrambling by_gold(gold), by_root(dvbs2::S2Scrambling::gold_to_root(gold), true);
        for (int i = 0; i < 40000; ++i)
        {
            auto v = tx.next({1,0});
            require((by_gold.descramble(v) - complex_t(1,0)).norm() == 0, "Gold descrambler mismatch");
            require((by_root.descramble(v) - complex_t(1,0)).norm() == 0, "Root descrambler mismatch");
        }
    }
    for (int invalid : {128,130,176,184,214,250,254,127,256})
    {
        bool rejected = false;
        try { dvbs2::get_dvbs2x_cfg(invalid); } catch (const std::runtime_error &) { rejected = true; }
        require(rejected, "Unsupported PLS was accepted");
    }
    require(!dvbs2::get_dvbs2x_cfg(133,0).pilots && dvbs2::get_dvbs2x_cfg(132,1).pilots,"Pilot override");
}

static void check_profile(const Profile &p, bool pilots, bool s2x = true, bool shortframe = false)
{
    const auto cfg = s2x ? dvbs2::get_dvbs2x_cfg(p.pls | pilots) : dvbs2::get_dvbs2_cfg(p.pls,shortframe,pilots);
    const int n = (s2x ? p.pls >= 216 : shortframe) ? 16200 : 64800, bits = std::strlen(p.order);
    const int pls_code = s2x ? p.pls | pilots : (p.pls << 2) | (shortframe << 1) | pilots;
    require(cfg.pilots == pilots && cfg.frame_slot_count * 90 * bits == n, "PLS frame geometry");
    dvbs2::BBFrameLDPC ldpc(cfg.framesize, cfg.coderate, s2x);
    dvbs2::BBFrameBCH bch(cfg.framesize, cfg.coderate, s2x);
    dvbs2::BBFrameDescrambler bb(cfg.framesize, cfg.coderate, s2x);
    require(bch.dataSize() == p.kbch && ldpc.dataSize() == p.nbch, "FEC lengths");
    std::mt19937 random(p.pls + pilots);
    std::vector<uint8_t> frame(n / 8);
    for (int i = 0; i < p.kbch / 8; ++i) frame[i] = random();
    const auto original = frame;
    scramble_bb(frame, p.kbch);
    bch.encode(frame.data());
    // Exercise BCH correction, including the shortened codeword's last byte.
    auto damaged = frame;
    for (int i = 0; i < 8; ++i) damaged[(i * 401 + 1) % (p.nbch / 8)] ^= 1 << (i % 8);
    require(bch.decode(damaged.data()) == 8 && damaged == frame, "BCH correction");
    ldpc.encode(frame.data());
    const int gold = pilots ? 12345 : 0;
    const int root = dvbs2::S2Scrambling::gold_to_root(gold);
    auto tx = transmit(p, frame, pilots, gold, pls_code);

    auto input = std::make_shared<dsp::stream<complex_t>>();
    dvbs2::S2PLLBlock pll(input, 0.01f, root);
    pll.frame_slot_count = cfg.frame_slot_count;
    pll.pilots = pilots;
    pll.pls_code = pls_code;
    pll.constellation = std::make_shared<dsp::constellation_t>(cfg.constel_obj_type,cfg.g1,cfg.g2);
    pll.constellation->make_lut(256);
    const auto reference = reference_points(p);
    for (size_t label = 0; label < reference.size(); ++label)
    {
        std::array<int8_t,5> decision{};
        pll.constellation->demod_soft_lut(reference[label],decision.data());
        int got_label = 0;
        for (int bit = 0; bit < bits; ++bit) got_label = (got_label << 1) | (decision[bit] < 0);
        // QPSK's adjacent I/Q swap happens in the deinterleaver.
        if (bits == 2) got_label = (got_label >> 1) | ((got_label & 1) << 1);
        if (got_label != int(label))
        {
            auto point = pll.constellation->mod(int(reference.size()) - 1 - label);
            std::cerr << "ref=" << reference[label].real << "," << reference[label].imag << " rx=" << point.real << "," << point.imag
                      << " soft=" << int(decision[0]) << "," << int(decision[1]) << "," << int(decision[2]) << std::endl;
        }
        require(got_label == int(label),"Ideal constellation label " + std::to_string(label) + " became " + std::to_string(got_label));
    }
    pll.update();
    dvbs2::S2BBToSoft demod(pll.output_stream, root);
    demod.frame_slot_count = cfg.frame_slot_count;
    demod.pilots = pilots;
    demod.s2x = s2x;
    demod.constellation = pll.constellation;
    demod.deinterleaver = std::make_shared<dvbs2::S2Deinterleaver>(cfg.constellation,cfg.framesize,cfg.coderate);
    pll.start(); demod.start();
    // Two frames verify per-frame scrambler reset and carrier-loop continuity.
    for (int f = 0; f < 2; ++f)
    {
        for (size_t i = 0; i < tx.size(); ++i)
        {
            float phase = 0.08f + 0.00002f * (i + f * tx.size());
            input->writeBuf[i] = tx[i] * complex_t(std::cos(phase),std::sin(phase));
        }
        input->swap(tx.size());
        int got = demod.output_stream->read();
        std::vector<int8_t> soft(demod.output_stream->readBuf,demod.output_stream->readBuf + got);
        const bool header_ok = demod.detect_modcod == (s2x ? p.pls | pilots : p.pls) && demod.detect_pilots == pilots &&
                               demod.detect_shortframes == (n == 16200);
        demod.output_stream->flush();
        if (got != n || !header_ok)
        {
            demod.stop(); pll.stop();
            require(false,"PLS detection or soft frame size");
        }
        int errors = 0;
        for (int i = 0; i < n; ++i)
            errors += ((soft[i] < 0) != bool((frame[i/8] >> (7-i%8)) & 1));
        if (errors)
        {
            demod.stop(); pll.stop();
            require(false,"Reference transmitter bit mapping: " + std::to_string(errors) + " errors");
        }
        // Supply a full SIMD batch, with different damaged locations per lane.
        std::vector<int8_t> batch(n * dvbs2::simd_type::SIZE);
        for (int lane = 0; lane < dvbs2::simd_type::SIZE; ++lane)
        {
            std::copy(soft.begin(),soft.end(),batch.begin() + lane * n);
            for (int j = 0; j < 3; ++j)
            {
                int index = lane * n + (lane * 101 + j * 991) % n;
                batch[index] = batch[index] < 0 ? 1 : -1;
            }
        }
        int trials = ldpc.decode(batch.data(),25);
        if (trials < 0) { demod.stop(); pll.stop(); require(false,"LDPC correction"); }
        for (int lane = 0; lane < dvbs2::simd_type::SIZE; ++lane)
        {
            std::vector<uint8_t> decoded(p.nbch / 8,0);
            for (int i = 0; i < p.nbch; ++i)
                decoded[i/8] |= (batch[lane*n+i] < 0) << (7-i%8);
            const int corrections = bch.decode(decoded.data());
            bb.work(decoded.data());
            if (corrections < 0 || !std::equal(original.begin(),original.begin()+p.kbch/8,decoded.begin()))
            { demod.stop(); pll.stop(); require(false,"Decoded BB-frame mismatch"); }
        }
    }
    demod.stop(); pll.stop();
}

static void check_sync()
{
    for (int code : {132,133,138,139,174,175,216,217,248,249})
    {
        auto cfg = dvbs2::get_dvbs2x_cfg(code);
        auto input = std::make_shared<dsp::stream<complex_t>>();
        dvbs2::S2PLSyncBlock sync(input,cfg.frame_slot_count,cfg.pilots);
        const int expected = 90 + cfg.frame_slot_count * 90 + (cfg.pilots ? (cfg.frame_slot_count-1)/16*36 : 0);
        require(sync.raw_frame_size == expected,"Pilot count");
        constexpr int offset = 31;
        for (int i = 0; i < expected * 2; ++i) input->writeBuf[i] = complex_t(0.1f,-0.1f);
        for (int f = 0; f < 2; ++f)
            for (int i = 0; i < 90; ++i)
                input->writeBuf[offset+f*expected+i] = header_symbol(i,code);
        sync.start(); input->swap(expected * 2);
        int got = sync.output_stream->read();
        bool aligned = got == expected;
        for (int i = 0; i < 90 && aligned; ++i)
            aligned &= (sync.output_stream->readBuf[i] - header_symbol(i,code)).norm() < 1e-6f;
        sync.output_stream->flush(); sync.stop();
        require(aligned,"PL synchronization alignment");
    }
}

static void check_legacy()
{
    const Profile normal[] = {
        {4,32208,32400,0,0,"01"}, {12,38688,38880,1,0,"210"},
        {18,43040,43200,3,5,"0123"}, {24,48408,48600,7,0,"01234"}};
    const Profile shortframes[] = {
        {4,7032,7200,0,0,"01"}, {12,9552,9720,1,0,"210"},
        {18,10632,10800,3,5,"0123"}, {24,11712,11880,7,0,"01234"}};
    for (bool pilots : {false,true})
    {
        for (const auto &p : normal) check_profile(p,pilots,false,false);
        for (const auto &p : shortframes) check_profile(p,pilots,false,true);
    }
}

int main()
{
    initLogger(); completeLoggerInit(); logger->set_level(slog::LOG_OFF);
    dsp::STREAM_BUFFER_SIZE = 100000;
    try
    {
        check_headers_and_scrambling();
        check_sync();
        check_legacy();
        for (const auto &p : profiles)
        {
            for (bool pilots : {false,true})
            {
                std::cout << "PLS " << (p.pls | pilots) << std::endl;
                check_profile(p,pilots);
            }
        }
        std::cout << "PASS: 256 PLS headers, four Gold codes, 10 sync cases, 16 legacy S2 cases, 42 S2X profiles with/without pilots; "
                  << dvbs2::simd_type::SIZE << " LDPC lanes, BCH correction and BB-frame recovery" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }
}
