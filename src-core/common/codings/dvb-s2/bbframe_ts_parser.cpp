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

/*
 * This file has been adapted from https://github.com/drmpeg/gr-dvbs2rx
 */

#include "bbframe_ts_parser.h"
#include <cstring>
#include <stdexcept>

namespace dvbs2
{
    BBFrameTSParser::BBFrameTSParser(int bbframe_size)
    {
        if (bbframe_size < 80 || bbframe_size > FRAME_SIZE_NORMAL || bbframe_size % 8 != 0)
            throw std::invalid_argument("Invalid BBFRAME bit length");
        kbch = bbframe_size;
        max_dfl = kbch - 80;
        build_crc8_table();
        reset();
    }

    BBFrameTSParser::~BBFrameTSParser() {}

    void BBFrameTSParser::reset()
    {
        synched = false;
        index = 1;
        packet[0] = 0x47;
        crc = 0;
    }

    void BBFrameTSParser::build_crc8_table()
    {
        for (int i = 0; i < 256; ++i)
        {
            unsigned int value = i;
            for (int bit = 0; bit < 8; ++bit)
                value = (value << 1) ^ ((value & 0x80) ? 0xd5 : 0);
            crc_tab[i] = value;
        }
    }

    unsigned int BBFrameTSParser::check_crc8(const uint8_t *in, int length)
    {
        unsigned int value = 0;
        for (int bit = 0; bit < length; ++bit)
        {
            const bool feedback = ((in[bit / 8] >> (7 - bit % 8)) & 1) ^ (value & 1);
            value >>= 1;
            if (feedback)
                value ^= 0xab;
        }
        return value;
    }

    int BBFrameTSParser::work(uint8_t *bbframe, int cnt, uint8_t *tsframes, int buffer_outsize)
    {
        int produced = 0;
        for (int frame = 0; frame < cnt; ++frame)
        {
            uint8_t *bbf = bbframe + (kbch / 8) * frame;
            BBHeader header(bbf);
            // Normal-mode, byte-aligned MPEG-TS. ISSY/NPD and generic streams
            // need different adaptation rules and must not be emitted as TS.
            if (check_crc8(bbf, 80) != 0 || header.ts_gs != 3 ||
                header.upl != TS_SIZE * 8 || header.sync != 0x47 ||
                header.issyi || header.npd || header.dfl > max_dfl || header.dfl % 8 != 0)
            {
                reset();
                continue;
            }

            const unsigned int length = header.dfl / 8;
            const bool has_sync = header.syncd != 0xffff;
            if (has_sync && (header.syncd % 8 != 0 || header.syncd / 8 >= length))
            {
                reset();
                continue;
            }
            if (length == 0)
                continue;

            if (synched && (single_stream != header.sis_mis || stream_id != header.isi))
                reset();
            single_stream = header.sis_mis;
            stream_id = header.isi;

            // SYNCD addresses the CRC byte that replaces the next TS sync byte.
            // A mismatch indicates a lost frame: discard the unfinished packet
            // and reacquire at the advertised marker within this same frame.
            const unsigned int expected = TS_SIZE - index;
            if (synched && (has_sync ? header.syncd / 8 != expected : expected < length))
                reset();

            const uint8_t *data = bbf + 10;
            unsigned int pos = 0;
            if (!synched)
            {
                // 0xffff means there is no packet start in this DATAFIELD.
                if (!has_sync)
                    continue;
                pos = header.syncd / 8 + 1;
                synched = true;
            }

            for (; pos < length; ++pos)
            {
                const uint8_t byte = data[pos];
                if (index == TS_SIZE)
                {
                    // Retain the whole packet until its CRC arrives, including
                    // across BBFRAME and work() boundaries. Never emit fragments.
                    if (produced + TS_SIZE > buffer_outsize)
                        throw std::length_error("TS output buffer too small");
                    if (byte != crc)
                        packet[1] |= TS_ERROR_INDICATOR;
                    std::memcpy(tsframes + produced, packet, TS_SIZE);
                    produced += TS_SIZE;
                    index = 1;
                    crc = 0;
                }
                else
                {
                    packet[index++] = byte;
                    crc = crc_tab[byte ^ crc];
                }
            }
        }
        return produced / TS_SIZE;
    }
}
