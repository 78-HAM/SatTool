#pragma once

#include <stdexcept>

#include "common/codings/dvb-s2/dvbs2.h"
#include "common/dsp/demod/constellation.h"

namespace dvbs2
{
    struct dvb_cgf_holder
    {
        int frame_slot_count;
        dvbs2::dvbs2_constellation_t constellation;
        dsp::constellation_type_t constel_obj_type;
        dvbs2::dvbs2_framesize_t framesize;
        dvbs2::dvbs2_code_rate_t coderate;
        bool pilots;
        float g1, g2;
    };

    inline dvb_cgf_holder get_dvbs2_cfg(int modcod, bool shortframes, bool pilots)
    {
        dvb_cgf_holder cfg;

        cfg.pilots = pilots;
        cfg.g1 = 0;
        cfg.g2 = 0;

        if (modcod <= 0)
            throw std::runtime_error("MODCOD cannot be <= 0!");
        else if (modcod < 12) // QPSK Modcods
        {
            cfg.frame_slot_count = shortframes ? 90 : 360;
            cfg.constellation = dvbs2::MOD_QPSK;
            cfg.constel_obj_type = dsp::QPSK;

            if (modcod == 1)
                cfg.coderate = dvbs2::C1_4;
            else if (modcod == 2)
                cfg.coderate = dvbs2::C1_3;
            else if (modcod == 3)
                cfg.coderate = dvbs2::C2_5;
            else if (modcod == 4)
                cfg.coderate = dvbs2::C1_2;
            else if (modcod == 5)
                cfg.coderate = dvbs2::C3_5;
            else if (modcod == 6)
                cfg.coderate = dvbs2::C2_3;
            else if (modcod == 7)
                cfg.coderate = dvbs2::C3_4;
            else if (modcod == 8)
                cfg.coderate = dvbs2::C4_5;
            else if (modcod == 9)
                cfg.coderate = dvbs2::C5_6;
            else if (modcod == 10)
                cfg.coderate = dvbs2::C8_9;
            else if (modcod == 11)
                cfg.coderate = dvbs2::C9_10;
        }
        else if (modcod < 18) // 8-PSK Modcods
        {
            cfg.frame_slot_count = shortframes ? 60 : 240;
            cfg.constellation = dvbs2::MOD_8PSK;
            cfg.constel_obj_type = dsp::PSK8;

            if (modcod == 12)
                cfg.coderate = dvbs2::C3_5;
            else if (modcod == 13)
                cfg.coderate = dvbs2::C2_3;
            else if (modcod == 14)
                cfg.coderate = dvbs2::C3_4;
            else if (modcod == 15)
                cfg.coderate = dvbs2::C5_6;
            else if (modcod == 16)
                cfg.coderate = dvbs2::C8_9;
            else if (modcod == 17)
                cfg.coderate = dvbs2::C9_10;
        }
        else if (modcod < 24) // 16-APSK Modcods
        {
            cfg.frame_slot_count = shortframes ? 45 : 180;
            cfg.constellation = dvbs2::MOD_16APSK;
            cfg.constel_obj_type = dsp::APSK16;

            if (modcod == 18)
            {
                cfg.coderate = dvbs2::C2_3;
                cfg.g1 = 3.15;
            }
            else if (modcod == 19)
            {
                cfg.coderate = dvbs2::C3_4;
                cfg.g1 = 2.85;
            }
            else if (modcod == 20)
            {
                cfg.coderate = dvbs2::C4_5;
                cfg.g1 = 2.75;
            }
            else if (modcod == 21)
            {
                cfg.coderate = dvbs2::C5_6;
                cfg.g1 = 2.70;
            }
            else if (modcod == 22)
            {
                cfg.coderate = dvbs2::C8_9;
                cfg.g1 = 2.60;
            }
            else if (modcod == 23)
            {
                cfg.coderate = dvbs2::C9_10;
                cfg.g1 = 2.57;
            }
        }
        else if (modcod < 29) // 32-APSK Modcods
        {
            cfg.frame_slot_count = shortframes ? 36 : 144;
            cfg.constellation = dvbs2::MOD_32APSK;
            cfg.constel_obj_type = dsp::APSK32;

            if (modcod == 24)
            {
                cfg.coderate = dvbs2::C3_4;
                cfg.g1 = 2.84;
                cfg.g2 = 5.27;
            }
            else if (modcod == 25)
            {
                cfg.coderate = dvbs2::C4_5;
                cfg.g1 = 2.72;
                cfg.g2 = 4.87;
            }
            else if (modcod == 26)
            {
                cfg.coderate = dvbs2::C5_6;
                cfg.g1 = 2.64;
                cfg.g2 = 4.64;
            }
            else if (modcod == 27)
            {
                cfg.coderate = dvbs2::C8_9;
                cfg.g1 = 2.54;
                cfg.g2 = 4.33;
            }
            else if (modcod == 28)
            {
                cfg.coderate = dvbs2::C9_10;
                cfg.g1 = 2.53;
                cfg.g2 = 4.30;
            }
        }
        else
            throw std::runtime_error("MODCOD not (yet?) supported!");

        cfg.framesize = shortframes ? dvbs2::FECFRAME_SHORT : dvbs2::FECFRAME_NORMAL;

        return cfg;
    }

    // Decode the regular DVB-S2X extended PLS code points (8-bit input,
    // excluding the pilot flag in bit zero).  VLSNR profiles need the
    // additional 900-symbol physical header and puncturing rules, so they
    // are rejected explicitly until that receive path is implemented.
    inline dvb_cgf_holder get_dvbs2x_cfg(int pls_code, int pilot_override = -1)
    {
        if (pls_code < 128 || pls_code > 255)
            throw std::runtime_error("DVB-S2X MODCOD must be an extended PLS code from 128 to 255");

        const int code = pls_code & ~1;
        dvb_cgf_holder cfg{};
        cfg.framesize = dvbs2::FECFRAME_NORMAL;
        cfg.pilots = pilot_override < 0 ? (pls_code & 1) != 0 : pilot_override != 0;
        cfg.g1 = 0;
        cfg.g2 = 0;
        int bits_per_symbol = 0;

        switch (code)
        {
        // QPSK normal frame
        case 132: cfg.constellation = MOD_QPSK; cfg.constel_obj_type = dsp::QPSK; cfg.coderate = C13_45; bits_per_symbol = 2; break;
        case 134: cfg.constellation = MOD_QPSK; cfg.constel_obj_type = dsp::QPSK; cfg.coderate = C9_20; bits_per_symbol = 2; break;
        case 136: cfg.constellation = MOD_QPSK; cfg.constel_obj_type = dsp::QPSK; cfg.coderate = C11_20; bits_per_symbol = 2; break;
        case 216: cfg.constellation = MOD_QPSK; cfg.constel_obj_type = dsp::QPSK; cfg.coderate = C11_45; bits_per_symbol = 2; break;
        case 218: cfg.constellation = MOD_QPSK; cfg.constel_obj_type = dsp::QPSK; cfg.coderate = C4_15; bits_per_symbol = 2; break;
        case 220: cfg.constellation = MOD_QPSK; cfg.constel_obj_type = dsp::QPSK; cfg.coderate = C14_45; bits_per_symbol = 2; break;
        case 222: cfg.constellation = MOD_QPSK; cfg.constel_obj_type = dsp::QPSK; cfg.coderate = C7_15; bits_per_symbol = 2; break;
        case 224: cfg.constellation = MOD_QPSK; cfg.constel_obj_type = dsp::QPSK; cfg.coderate = C8_15; bits_per_symbol = 2; break;
        case 226: cfg.constellation = MOD_QPSK; cfg.constel_obj_type = dsp::QPSK; cfg.coderate = C32_45; bits_per_symbol = 2; break;

        // 8PSK
        case 142: cfg.constellation = MOD_8PSK; cfg.constel_obj_type = dsp::PSK8; cfg.coderate = C23_36; bits_per_symbol = 3; break;
        case 144: cfg.constellation = MOD_8PSK; cfg.constel_obj_type = dsp::PSK8; cfg.coderate = C25_36; bits_per_symbol = 3; break;
        case 146: cfg.constellation = MOD_8PSK; cfg.constel_obj_type = dsp::PSK8; cfg.coderate = C13_18; bits_per_symbol = 3; break;
        case 228: cfg.constellation = MOD_8PSK; cfg.constel_obj_type = dsp::PSK8; cfg.coderate = C7_15; bits_per_symbol = 3; break;
        case 230: cfg.constellation = MOD_8PSK; cfg.constel_obj_type = dsp::PSK8; cfg.coderate = C8_15; bits_per_symbol = 3; break;
        case 232: cfg.constellation = MOD_8PSK; cfg.constel_obj_type = dsp::PSK8; cfg.coderate = C26_45; bits_per_symbol = 3; break;
        case 234: cfg.constellation = MOD_8PSK; cfg.constel_obj_type = dsp::PSK8; cfg.coderate = C32_45; bits_per_symbol = 3; break;

        // 8APSK, and 8+8APSK profiles
        case 138: cfg.constellation = MOD_8APSK; cfg.constel_obj_type = dsp::APSK8; cfg.coderate = C100_180; cfg.g1 = 0; bits_per_symbol = 3; break;
        case 140: cfg.constellation = MOD_8APSK; cfg.constel_obj_type = dsp::APSK8; cfg.coderate = C104_180; cfg.g1 = 1; bits_per_symbol = 3; break;
        case 148: cfg.constellation = MOD_8_8APSK; cfg.constel_obj_type = dsp::APSK88; cfg.coderate = C90_180; cfg.g1 = 2; bits_per_symbol = 4; break;
        case 150: cfg.constellation = MOD_8_8APSK; cfg.constel_obj_type = dsp::APSK88; cfg.coderate = C96_180; cfg.g1 = 2; bits_per_symbol = 4; break;
        case 152: cfg.constellation = MOD_8_8APSK; cfg.constel_obj_type = dsp::APSK88; cfg.coderate = C100_180; cfg.g1 = 2; bits_per_symbol = 4; break;
        case 158: cfg.constellation = MOD_8_8APSK; cfg.constel_obj_type = dsp::APSK88; cfg.coderate = C18_30; cfg.g1 = 0; bits_per_symbol = 4; break;
        case 164: cfg.constellation = MOD_8_8APSK; cfg.constel_obj_type = dsp::APSK88; cfg.coderate = C20_30; cfg.g1 = 1; bits_per_symbol = 4; break;

        // 16APSK normal and short frame profiles
        case 154: cfg.constellation = MOD_16APSK; cfg.constel_obj_type = dsp::APSK16; cfg.coderate = C26_45; cfg.g1 = 3.70f; bits_per_symbol = 4; break;
        case 156: cfg.constellation = MOD_16APSK; cfg.constel_obj_type = dsp::APSK16; cfg.coderate = C3_5; cfg.g1 = 3.70f; bits_per_symbol = 4; break;
        case 160: cfg.constellation = MOD_16APSK; cfg.constel_obj_type = dsp::APSK16; cfg.coderate = C28_45; cfg.g1 = 3.50f; bits_per_symbol = 4; break;
        case 162: cfg.constellation = MOD_16APSK; cfg.constel_obj_type = dsp::APSK16; cfg.coderate = C23_36; cfg.g1 = 3.10f; bits_per_symbol = 4; break;
        case 166: cfg.constellation = MOD_16APSK; cfg.constel_obj_type = dsp::APSK16; cfg.coderate = C25_36; cfg.g1 = 3.10f; bits_per_symbol = 4; break;
        case 168: cfg.constellation = MOD_16APSK; cfg.constel_obj_type = dsp::APSK16; cfg.coderate = C13_18; cfg.g1 = 2.85f; bits_per_symbol = 4; break;
        case 170: cfg.constellation = MOD_16APSK; cfg.constel_obj_type = dsp::APSK16; cfg.coderate = C140_180; cfg.g1 = 3.60f; bits_per_symbol = 4; break;
        case 172: cfg.constellation = MOD_16APSK; cfg.constel_obj_type = dsp::APSK16; cfg.coderate = C154_180; cfg.g1 = 3.20f; bits_per_symbol = 4; break;
        case 236: cfg.framesize = FECFRAME_SHORT; cfg.constellation = MOD_16APSK; cfg.constel_obj_type = dsp::APSK16; cfg.coderate = C7_15; cfg.g1 = 3.32f; bits_per_symbol = 4; break;
        case 238: cfg.framesize = FECFRAME_SHORT; cfg.constellation = MOD_16APSK; cfg.constel_obj_type = dsp::APSK16; cfg.coderate = C8_15; cfg.g1 = 3.50f; bits_per_symbol = 4; break;
        case 240: cfg.framesize = FECFRAME_SHORT; cfg.constellation = MOD_16APSK; cfg.constel_obj_type = dsp::APSK16; cfg.coderate = C26_45; cfg.g1 = 3.70f; bits_per_symbol = 4; break;
        case 242: cfg.framesize = FECFRAME_SHORT; cfg.constellation = MOD_16APSK; cfg.constel_obj_type = dsp::APSK16; cfg.coderate = C3_5; cfg.g1 = 3.70f; bits_per_symbol = 4; break;
        case 244: cfg.framesize = FECFRAME_SHORT; cfg.constellation = MOD_16APSK; cfg.constel_obj_type = dsp::APSK16; cfg.coderate = C32_45; cfg.g1 = 2.85f; bits_per_symbol = 4; break;

        // Composite 32-point mappings.
        case 174: cfg.constellation = MOD_4_12_16APSK; cfg.constel_obj_type = dsp::APSK4_12_16; cfg.coderate = C2_3; cfg.g1 = 0; bits_per_symbol = 5; break;
        case 248: cfg.framesize = FECFRAME_SHORT; cfg.constellation = MOD_4_12_16APSK; cfg.constel_obj_type = dsp::APSK4_12_16; cfg.coderate = C32_45; cfg.g1 = 2; bits_per_symbol = 5; break;
        case 178: cfg.constellation = MOD_4_8_4_16APSK; cfg.constel_obj_type = dsp::APSK4_8_4_16; cfg.coderate = C128_180; cfg.g1 = 0; bits_per_symbol = 5; break;
        case 180: cfg.constellation = MOD_4_8_4_16APSK; cfg.constel_obj_type = dsp::APSK4_8_4_16; cfg.coderate = C132_180; cfg.g1 = 1; bits_per_symbol = 5; break;
        case 182: cfg.constellation = MOD_4_8_4_16APSK; cfg.constel_obj_type = dsp::APSK4_8_4_16; cfg.coderate = C140_180; cfg.g1 = 2; bits_per_symbol = 5; break;
        case 246: cfg.framesize = FECFRAME_SHORT; cfg.constellation = MOD_4_12_16APSK; cfg.constel_obj_type = dsp::APSK4_12_16; cfg.coderate = C2_3; cfg.g1 = 1; bits_per_symbol = 5; break;

        case 128:
        case 130:
            throw std::runtime_error("DVB-S2X VLSNR modes use an extended physical header and are not supported yet");
        default:
            throw std::runtime_error("DVB-S2X extended PLS code is reserved or unsupported");
        }

        if ((code >= 216 && code <= 234) && cfg.framesize == FECFRAME_NORMAL)
            cfg.framesize = FECFRAME_SHORT;
        const int frame_bits = cfg.framesize == FECFRAME_NORMAL ? FRAME_SIZE_NORMAL : FRAME_SIZE_SHORT;
        cfg.frame_slot_count = frame_bits / bits_per_symbol / 90;
        return cfg;
    }
}
