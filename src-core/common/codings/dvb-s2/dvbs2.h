#pragma once

// Frame sizes
#define FRAME_SIZE_NORMAL 64800
#define FRAME_SIZE_MEDIUM 32400
#define FRAME_SIZE_SHORT 16200

namespace dvbs2
{
    enum dvbs2_code_rate_t
    {
        C1_4,
        C1_3,
        C2_5,
        C1_2,
        C3_5,
        C2_3,
        C3_4,
        C4_5,
        C5_6,
        C7_8,
        C8_9,
        C9_10,
        // DVB-S2X normal-frame rates (EN 302 307-2)
        C13_45,
        C9_20,
        C90_180,
        C96_180,
        C11_20,
        C100_180,
        C104_180,
        C26_45,
        C18_30,
        C28_45,
        C23_36,
        C116_180,
        C20_30,
        C124_180,
        C25_36,
        C128_180,
        C13_18,
        C132_180,
        C22_30,
        C135_180,
        C140_180,
        C7_9,
        C154_180,
        // DVB-S2X short-frame rates
        C11_45,
        C4_15,
        C14_45,
        C7_15,
        C8_15,
        C32_45,
        // S2X VLSNR and medium-frame profiles.  VLSNR physical headers
        // are intentionally kept distinct so callers can reject them
        // instead of silently decoding them as ordinary frames.
        C2_9_VLSNR,
        C1_5_MEDIUM,
        C11_45_MEDIUM,
        C1_3_MEDIUM,
        C1_5_VLSNR_SF2,
        C11_45_VLSNR_SF2,
        C1_5_VLSNR,
        C4_15_VLSNR,
        C1_3_VLSNR,
    };

    enum dvbs2_framesize_t
    {
        FECFRAME_NORMAL,
        FECFRAME_SHORT,
        FECFRAME_MEDIUM,
    };

    enum dvbs2_constellation_t
    {
        MOD_QPSK,
        MOD_8PSK,
        MOD_16APSK,
        MOD_32APSK,
        // Keep the original DVB-S2 numeric values above stable for saved
        // tags/configurations; DVB-S2X constellations follow afterwards.
        MOD_8APSK,
        MOD_8_8APSK,
        MOD_4_12_16APSK,
        MOD_4_8_4_16APSK,
        MOD_BPSK,
        MOD_BPSK_SF2,
    };
}
