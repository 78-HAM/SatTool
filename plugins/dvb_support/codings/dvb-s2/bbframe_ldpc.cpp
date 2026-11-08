/* -*- c++ -*- */
/*
 * Copyright 2018 Ron Economos.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "bbframe_ldpc.h"
#include "codings/dvb-s2/ldpc/dvb_s2_tables.hh"
#include "codings/dvb-s2/ldpc/dvb_s2x_tables.hh"
#include <cstring>
#include <stdexcept>

namespace dvbs2
{
    BBFrameLDPC::BBFrameLDPC(dvbs2_framesize_t framesize, dvbs2_code_rate_t rate, bool s2x)
    {
        if (s2x && framesize == FECFRAME_NORMAL)
        {
            // DVB-S2X normal-frame parity check matrices.  The table
            // definitions are copied from the public ETSI-based tables in
            // gr-dvbs2rx and use the same LDPCInterface contract as the
            // original SatDump decoder.
            switch (rate)
            {
            case C2_9_VLSNR: ldpc = new LDPC<DVB_S2X_TABLE_B1>(); break;
            case C13_45: ldpc = new LDPC<DVB_S2X_TABLE_B2>(); break;
            case C9_20: ldpc = new LDPC<DVB_S2X_TABLE_B3>(); break;
            case C11_20: ldpc = new LDPC<DVB_S2X_TABLE_B4>(); break;
            case C26_45: ldpc = new LDPC<DVB_S2X_TABLE_B5>(); break;
            case C28_45: ldpc = new LDPC<DVB_S2X_TABLE_B6>(); break;
            case C23_36: ldpc = new LDPC<DVB_S2X_TABLE_B7>(); break;
            case C25_36: ldpc = new LDPC<DVB_S2X_TABLE_B8>(); break;
            case C13_18: ldpc = new LDPC<DVB_S2X_TABLE_B9>(); break;
            case C7_9: ldpc = new LDPC<DVB_S2X_TABLE_B10>(); break;
            case C90_180: ldpc = new LDPC<DVB_S2X_TABLE_B11>(); break;
            case C96_180: ldpc = new LDPC<DVB_S2X_TABLE_B12>(); break;
            case C100_180: ldpc = new LDPC<DVB_S2X_TABLE_B13>(); break;
            case C104_180: ldpc = new LDPC<DVB_S2X_TABLE_B14>(); break;
            case C116_180: ldpc = new LDPC<DVB_S2X_TABLE_B15>(); break;
            case C124_180: ldpc = new LDPC<DVB_S2X_TABLE_B16>(); break;
            case C128_180: ldpc = new LDPC<DVB_S2X_TABLE_B17>(); break;
            case C132_180: ldpc = new LDPC<DVB_S2X_TABLE_B18>(); break;
            case C135_180: ldpc = new LDPC<DVB_S2X_TABLE_B19>(); break;
            case C140_180: ldpc = new LDPC<DVB_S2X_TABLE_B20>(); break;
            case C154_180: ldpc = new LDPC<DVB_S2X_TABLE_B21>(); break;
            case C18_30: ldpc = new LDPC<DVB_S2X_TABLE_B22>(); break;
            case C20_30: ldpc = new LDPC<DVB_S2X_TABLE_B23>(); break;
            case C22_30: ldpc = new LDPC<DVB_S2X_TABLE_B24>(); break;
            case C2_3: ldpc = new LDPC<DVB_S2_TABLE_B6>(); break;
            case C3_5: ldpc = new LDPC<DVB_S2_TABLE_B5>(); break;
            default: break;
            }
        }
        else if (s2x && framesize == FECFRAME_SHORT)
        {
            switch (rate)
            {
            case C11_45: ldpc = new LDPC<DVB_S2X_TABLE_C1>(); break;
            case C4_15: ldpc = new LDPC<DVB_S2X_TABLE_C2>(); break;
            case C14_45: ldpc = new LDPC<DVB_S2X_TABLE_C3>(); break;
            case C7_15: ldpc = new LDPC<DVB_S2X_TABLE_C4>(); break;
            case C8_15: ldpc = new LDPC<DVB_S2X_TABLE_C5>(); break;
            case C26_45: ldpc = new LDPC<DVB_S2X_TABLE_C6>(); break;
            case C32_45: ldpc = new LDPC<DVB_S2X_TABLE_C7>(); break;
            case C2_3: ldpc = new LDPC<DVB_S2_TABLE_C6>(); break;
            case C3_5: ldpc = new LDPC<DVB_S2_TABLE_C5>(); break;
            case C1_5_VLSNR_SF2: ldpc = new LDPC<DVB_S2X_TABLE_C1>(); break;
            case C11_45_VLSNR_SF2: ldpc = new LDPC<DVB_S2X_TABLE_C1>(); break;
            default: break;
            }
        }
        else if (s2x && framesize == FECFRAME_MEDIUM)
        {
            switch (rate)
            {
            case C1_5_MEDIUM: ldpc = new LDPC<DVB_S2X_TABLE_C8>(); break;
            case C11_45_MEDIUM: ldpc = new LDPC<DVB_S2X_TABLE_C9>(); break;
            case C1_3_MEDIUM: ldpc = new LDPC<DVB_S2X_TABLE_C10>(); break;
            default: break;
            }
        }
        else if (framesize == FECFRAME_NORMAL)
        {
            switch (rate)
            {
            case C1_4:
                ldpc = new LDPC<DVB_S2_TABLE_B1>();
                break;
            case C1_3:
                ldpc = new LDPC<DVB_S2_TABLE_B2>();
                break;
            case C2_5:
                ldpc = new LDPC<DVB_S2_TABLE_B3>();
                break;
            case C1_2:
                ldpc = new LDPC<DVB_S2_TABLE_B4>();
                break;
            case C3_5:
                ldpc = new LDPC<DVB_S2_TABLE_B5>();
                break;
            case C2_3:
                ldpc = new LDPC<DVB_S2_TABLE_B6>();
                break;
            case C3_4:
                ldpc = new LDPC<DVB_S2_TABLE_B7>();
                break;
            case C4_5:
                ldpc = new LDPC<DVB_S2_TABLE_B8>();
                break;
            case C5_6:
                ldpc = new LDPC<DVB_S2_TABLE_B9>();
                break;
            case C8_9:
                ldpc = new LDPC<DVB_S2_TABLE_B10>();
                break;
            case C9_10:
                ldpc = new LDPC<DVB_S2_TABLE_B11>();
                break;
            default:
                break;
            }
        }
        else if (framesize == FECFRAME_SHORT)
        {
            switch (rate)
            {
            case C1_4:
                ldpc = new LDPC<DVB_S2_TABLE_C1>();
                break;
            case C1_3:
                ldpc = new LDPC<DVB_S2_TABLE_C2>();
                break;
            case C2_5:
                ldpc = new LDPC<DVB_S2_TABLE_C3>();
                break;
            case C1_2:
                ldpc = new LDPC<DVB_S2_TABLE_C4>();
                break;
            case C3_5:
                ldpc = new LDPC<DVB_S2_TABLE_C5>();
                break;
            case C2_3:
                ldpc = new LDPC<DVB_S2_TABLE_C6>();
                break;
            case C3_4:
                ldpc = new LDPC<DVB_S2_TABLE_C7>();
                break;
            case C4_5:
                ldpc = new LDPC<DVB_S2_TABLE_C8>();
                break;
            case C5_6:
                ldpc = new LDPC<DVB_S2_TABLE_C9>();
                break;
            case C8_9:
                ldpc = new LDPC<DVB_S2_TABLE_C10>();
                break;
            default:
                break;
            }
        }

        if (ldpc == nullptr)
            throw std::runtime_error("Unsupported DVB-S2/S2X LDPC frame or code rate");

        decoder.init(ldpc);
        encoder.init(ldpc);

        aligned_buffer = new simd_type[68400];
    }

    BBFrameLDPC::~BBFrameLDPC()
    {
        delete[] aligned_buffer;
        delete ldpc;
    }

    int BBFrameLDPC::decode(int8_t *frame, int max_trials)
    {
        int trials = decoder((void *)aligned_buffer, (code_type *)frame, max_trials);

        if (trials < 0)
            return trials;
        else
            return max_trials - trials;
    }

    void BBFrameLDPC::encode(uint8_t *frame)
    {
        int8_t *soft_buffer = new int8_t[ldpc->code_len()];

        // Convert to soft
        for (int i = 0; i < ldpc->data_len(); i++)
            // LDPCEncoder uses positive soft values for bit zero and
            // negative values for bit one. Keep the information prefix and
            // generated parity in the same convention so encode() produces
            // a valid codeword for non-zero payloads too.
            soft_buffer[i] = ((frame[i / 8] >> (7 - (i % 8))) & 1) ? -127 : 127;

        encoder(soft_buffer, &soft_buffer[ldpc->data_len()]);

        // Repack to bytes
        memset(&frame[ldpc->data_len() / 8], 0, (ldpc->code_len() - ldpc->data_len()) / 8);
        for (int i = 0; i < (ldpc->code_len() - ldpc->data_len()); i++)
            frame[(ldpc->data_len() / 8) + i / 8] = frame[(ldpc->data_len() / 8) + i / 8] << 1 | (soft_buffer[ldpc->data_len() + i] < 0);

        delete[] soft_buffer;
    }
}
