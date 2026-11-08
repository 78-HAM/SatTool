/* -*- c++ -*- */
/*
 * Copyright 2014,2016,2017,2020 Ron Economos.
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

// Reference transmitter geometry, retained independently of the decoder.
// https://github.com/drmpeg/gr-dvbs2/blob/99a2ac5592fa41e9c0f8f757995f3451ab8184a6/lib/modulator_bc_impl.cc
#pragma once
#include "common/dsp/complex.h"

struct ReferenceConstellations
{
    complex_t m_bpsk[2][2], m_qpsk[4], m_8psk[8];
    complex_t m_8apsk[8][2], m_16apsk[16][12], m_8_8apsk[16][3];
    complex_t m_32apsk[32][5], m_4_12_16apsk[32][3], m_4_8_4_16apsk[32][3];
    ReferenceConstellations()
    {
        using gr_complex = complex_t;
      double r0, r1, r2, r3, r4;
      double m = 1.0;
      r1 = m;
      m_bpsk[0][0] = gr_complex((r1 * cos(M_PI / 4.0)), (r1 * sin(M_PI / 4.0)));
      m_bpsk[0][1] = gr_complex((r1 * cos(5.0 * M_PI / 4.0)), (r1 * sin(5.0 * M_PI / 4.0)));
      m_bpsk[1][0] = gr_complex((r1 * cos(5.0 * M_PI / 4.0)), (r1 * sin(M_PI / 4.0)));
      m_bpsk[1][1] = gr_complex((r1 * cos(M_PI / 4.0)), (r1 * sin(5.0 * M_PI /4.0)));

      m_qpsk[0] = gr_complex((r1 * cos(M_PI / 4.0)), (r1 * sin(M_PI / 4.0)));
      m_qpsk[1] = gr_complex((r1 * cos(7 * M_PI / 4.0)), (r1 * sin(7 * M_PI / 4.0)));
      m_qpsk[2] = gr_complex((r1 * cos(3 * M_PI / 4.0)), (r1 * sin(3 * M_PI / 4.0)));
      m_qpsk[3] = gr_complex((r1 * cos(5 * M_PI / 4.0)), (r1 * sin(5 * M_PI / 4.0)));

      m_8psk[0] = gr_complex((r1 * cos(M_PI / 4.0)), (r1 * sin(M_PI / 4.0)));
      m_8psk[1] = gr_complex((r1 * cos(0.0)), (r1 * sin(0.0)));
      m_8psk[2] = gr_complex((r1 * cos(4 * M_PI / 4.0)), (r1 * sin(4 * M_PI / 4.0)));
      m_8psk[3] = gr_complex((r1 * cos(5 * M_PI / 4.0)), (r1 * sin(5 * M_PI / 4.0)));
      m_8psk[4] = gr_complex((r1 * cos(2 * M_PI / 4.0)), (r1 * sin(2 * M_PI / 4.0)));
      m_8psk[5] = gr_complex((r1 * cos(7 * M_PI / 4.0)), (r1 * sin(7 * M_PI / 4.0)));
      m_8psk[6] = gr_complex((r1 * cos(3 * M_PI / 4.0)), (r1 * sin(3 * M_PI / 4.0)));
      m_8psk[7] = gr_complex((r1 * cos(6 * M_PI / 4.0)), (r1 * sin(6 * M_PI / 4.0)));

      float m_8apsk_radius[2][2] = {{6.8, 5.32}, {8.0, 6.39}};
      r3 = m;
      for (int i = 0; i < 2; i++) {
        r1 = r3 / m_8apsk_radius[i][0];
        r2 = r1 * m_8apsk_radius[i][1];
        m_8apsk[0][i] = gr_complex((r1 * cos(M_PI)), (r1 * sin(M_PI)));
        m_8apsk[1][i] = gr_complex((r2 * cos(M_PI * 1.352)), (r2 * sin(M_PI * 1.352)));
        m_8apsk[2][i] = gr_complex((r2 * cos(M_PI * -1.352)), (r2 * sin(M_PI * -1.352)));
        m_8apsk[3][i] = gr_complex((r3 * cos(M_PI)), (r3 * sin(M_PI)));
        m_8apsk[4][i] = gr_complex((r1 * cos(0.0)), (r1 * sin(0.0)));
        m_8apsk[5][i] = gr_complex((r2 * cos(M_PI * -0.352)), (r2 * sin(M_PI * -0.352)));
        m_8apsk[6][i] = gr_complex((r2 * cos(M_PI * 0.352)), (r2 * sin(M_PI * 0.352)));
        m_8apsk[7][i] = gr_complex((r3 * cos(0.0)), (r3 * sin(0.0)));
      }

      float m_16apsk_radius[12][1] = {{3.70}, {3.60}, {3.50}, {3.32}, {3.20}, {3.15}, {3.10}, {2.85}, {2.75}, {2.70}, {2.60}, {2.57}};
      r2 = m;
      for (int i = 0; i < 12; i++) {
        r1 = r2 / m_16apsk_radius[i][0];
        r0 = sqrt(4.0 / ((r1 * r1) + 3.0 * (r2 * r2)));
        r1 *= r0;
        r2 *= r0;
        m_16apsk[0][i] = gr_complex((r2 * cos(M_PI / 4.0)), (r2 * sin(M_PI / 4.0)));
        m_16apsk[1][i] = gr_complex((r2 * cos(-M_PI / 4.0)), (r2 * sin(-M_PI / 4.0)));
        m_16apsk[2][i] = gr_complex((r2 * cos(3 * M_PI / 4.0)), (r2 * sin(3 * M_PI / 4.0)));
        m_16apsk[3][i] = gr_complex((r2 * cos(-3 * M_PI / 4.0)), (r2 * sin(-3 * M_PI / 4.0)));
        m_16apsk[4][i] = gr_complex((r2 * cos(M_PI / 12.0)), (r2 * sin(M_PI / 12.0)));
        m_16apsk[5][i] = gr_complex((r2 * cos(-M_PI / 12.0)), (r2 * sin(-M_PI / 12.0)));
        m_16apsk[6][i] = gr_complex((r2 * cos(11 * M_PI / 12.0)), (r2 * sin(11 * M_PI / 12.0)));
        m_16apsk[7][i] = gr_complex((r2 * cos(-11 * M_PI / 12.0)), (r2 * sin(-11 * M_PI / 12.0)));
        m_16apsk[8][i] = gr_complex((r2 * cos(5 * M_PI / 12.0)), (r2 * sin(5 * M_PI / 12.0)));
        m_16apsk[9][i] = gr_complex((r2 * cos(-5 * M_PI / 12.0)), (r2 * sin(-5 * M_PI / 12.0)));
        m_16apsk[10][i] = gr_complex((r2 * cos(7 * M_PI / 12.0)), (r2 * sin(7 * M_PI / 12.0)));
        m_16apsk[11][i] = gr_complex((r2 * cos(-7 * M_PI / 12.0)), (r2 * sin(-7 * M_PI / 12.0)));
        m_16apsk[12][i] = gr_complex((r1 * cos(M_PI / 4.0)), (r1 * sin(M_PI / 4.0)));
        m_16apsk[13][i] = gr_complex((r1 * cos(-M_PI / 4.0)), (r1 * sin(-M_PI / 4.0)));
        m_16apsk[14][i] = gr_complex((r1 * cos(3 * M_PI / 4.0)), (r1 * sin(3 * M_PI / 4.0)));
        m_16apsk[15][i] = gr_complex((r1 * cos(-3 * M_PI / 4.0)), (r1 * sin(-3 * M_PI / 4.0)));
      }

      m_8_8apsk[0][0] = gr_complex(0.4718, 0.2606);
      m_8_8apsk[1][0] = gr_complex(0.2606, 0.4718);
      m_8_8apsk[2][0] = gr_complex(-0.4718, 0.2606);
      m_8_8apsk[3][0] = gr_complex(-0.2606, 0.4718);
      m_8_8apsk[4][0] = gr_complex(0.4718, -0.2606);
      m_8_8apsk[5][0] = gr_complex(0.2606, -0.4718);
      m_8_8apsk[6][0] = gr_complex(-0.4718, -0.2606);
      m_8_8apsk[7][0] = gr_complex(-0.2606, -0.4718);
      m_8_8apsk[8][0] = gr_complex(1.2088, 0.4984);
      m_8_8apsk[9][0] = gr_complex(0.4984, 1.2088);
      m_8_8apsk[10][0] = gr_complex(-1.2088, 0.4984);
      m_8_8apsk[11][0] = gr_complex(-0.4984, 1.2088);
      m_8_8apsk[12][0] = gr_complex(1.2088, -0.4984);
      m_8_8apsk[13][0] = gr_complex(0.4984, -1.2088);
      m_8_8apsk[14][0] = gr_complex(-1.2088, -0.4984);
      m_8_8apsk[15][0] = gr_complex(-0.4984, -1.2088);

      m_8_8apsk[0][1] = gr_complex(0.5061, 0.2474);
      m_8_8apsk[1][1] = gr_complex(0.2474, 0.5061);
      m_8_8apsk[2][1] = gr_complex(-0.5061, 0.2474);
      m_8_8apsk[3][1] = gr_complex(-0.2474, 0.5061);
      m_8_8apsk[4][1] = gr_complex(0.5061, -0.2474);
      m_8_8apsk[5][1] = gr_complex(0.2474, -0.5061);
      m_8_8apsk[6][1] = gr_complex(-0.5061, -0.2474);
      m_8_8apsk[7][1] = gr_complex(-0.2474, -0.5061);
      m_8_8apsk[8][1] = gr_complex(1.2007, 0.4909);
      m_8_8apsk[9][1] = gr_complex(0.4909, 1.2007);
      m_8_8apsk[10][1] = gr_complex(-1.2007, 0.4909);
      m_8_8apsk[11][1] = gr_complex(-0.4909, 1.2007);
      m_8_8apsk[12][1] = gr_complex(1.2007, -0.4909);
      m_8_8apsk[13][1] = gr_complex(0.4909, -1.2007);
      m_8_8apsk[14][1] = gr_complex(-1.2007, -0.4909);
      m_8_8apsk[15][1] = gr_complex(-0.4909, -1.2007);

      r2 = m;
      r1 = r2 / 2.19;
      m_8_8apsk[0][2] = gr_complex((r1 * cos(M_PI / 8.0)), (r1 * sin(M_PI / 8.0)));
      m_8_8apsk[1][2] = gr_complex((r1 * cos(3 * M_PI / 8.0)), (r1 * sin(3 * M_PI / 8.0)));
      m_8_8apsk[2][2] = gr_complex((r1 * cos(7 * M_PI / 8.0)), (r1 * sin(7 * M_PI / 8.0)));
      m_8_8apsk[3][2] = gr_complex((r1 * cos(5 * M_PI / 8.0)), (r1 * sin(5 * M_PI / 8.0)));
      m_8_8apsk[4][2] = gr_complex((r1 * cos(15 * M_PI / 8.0)), (r1 * sin(15 * M_PI / 8.0)));
      m_8_8apsk[5][2] = gr_complex((r1 * cos(13 * M_PI / 8.0)), (r1 * sin(13 * M_PI / 8.0)));
      m_8_8apsk[6][2] = gr_complex((r1 * cos(9 * M_PI / 8.0)), (r1 * sin(9 * M_PI / 8.0)));
      m_8_8apsk[7][2] = gr_complex((r1 * cos(11 * M_PI / 8.0)), (r1 * sin(11 * M_PI / 8.0)));
      m_8_8apsk[8][2] = gr_complex((r2 * cos(M_PI / 8.0)), (r2 * sin(M_PI / 8.0)));
      m_8_8apsk[9][2] = gr_complex((r2 * cos(3 * M_PI / 8.0)), (r2 * sin(3 * M_PI / 8.0)));
      m_8_8apsk[10][2] = gr_complex((r2 * cos(7 * M_PI / 8.0)), (r2 * sin(7 * M_PI / 8.0)));
      m_8_8apsk[11][2] = gr_complex((r2 * cos(5 * M_PI / 8.0)), (r2 * sin(5 * M_PI / 8.0)));
      m_8_8apsk[12][2] = gr_complex((r2 * cos(15 * M_PI / 8.0)), (r2 * sin(15 * M_PI / 8.0)));
      m_8_8apsk[13][2] = gr_complex((r2 * cos(13 * M_PI / 8.0)), (r2 * sin(13 * M_PI / 8.0)));
      m_8_8apsk[14][2] = gr_complex((r2 * cos(9 * M_PI / 8.0)), (r2 * sin(9 * M_PI / 8.0)));
      m_8_8apsk[15][2] = gr_complex((r2 * cos(11 * M_PI / 8.0)), (r2 * sin(11 * M_PI / 8.0)));


      float m_32apsk_radius[5][2] = {{5.27, 2.84}, {4.87, 2.72}, {4.64, 2.64}, {4.33, 2.54}, {4.30, 2.53}};
      r3 = m;
      for (int i = 0; i < 5; i++) {
        r1 = r3 / m_32apsk_radius[i][0];
        r2 = r1 * m_32apsk_radius[i][1];
        r0 = sqrt(8.0 / ((r1 * r1) + 3.0 * (r2 * r2) + 4.0 * (r3 * r3)));
        r1 *= r0;
        r2 *= r0;
        r3 *= r0;
        m_32apsk[0][i] = gr_complex((r2 * cos(M_PI / 4.0)), (r2 * sin(M_PI / 4.0)));
        m_32apsk[1][i] = gr_complex((r2 * cos(5 * M_PI / 12.0)), (r2 * sin(5 * M_PI / 12.0)));
        m_32apsk[2][i] = gr_complex((r2 * cos(-M_PI / 4.0)), (r2 * sin(-M_PI / 4.0)));
        m_32apsk[3][i] = gr_complex((r2 * cos(-5 * M_PI / 12.0)), (r2 * sin(-5 * M_PI / 12.0)));
        m_32apsk[4][i] = gr_complex((r2 * cos(3 * M_PI / 4.0)), (r2 * sin(3 * M_PI / 4.0)));
        m_32apsk[5][i] = gr_complex((r2 * cos(7 * M_PI / 12.0)), (r2 * sin(7 * M_PI / 12.0)));
        m_32apsk[6][i] = gr_complex((r2 * cos(-3 * M_PI / 4.0)), (r2 * sin(-3 * M_PI / 4.0)));
        m_32apsk[7][i] = gr_complex((r2 * cos(-7 * M_PI / 12.0)), (r2 * sin(-7 * M_PI / 12.0)));
        m_32apsk[8][i] = gr_complex((r3 * cos(M_PI / 8.0)), (r3 * sin(M_PI / 8.0)));
        m_32apsk[9][i] = gr_complex((r3 * cos(3 * M_PI / 8.0)), (r3 * sin(3 * M_PI / 8.0)));
        m_32apsk[10][i] = gr_complex((r3 * cos(-M_PI / 4.0)), (r3 * sin(-M_PI / 4.0)));
        m_32apsk[11][i] = gr_complex((r3 * cos(-M_PI / 2.0)), (r3 * sin(-M_PI / 2.0)));
        m_32apsk[12][i] = gr_complex((r3 * cos(3 * M_PI / 4.0)), (r3 * sin(3 * M_PI / 4.0)));
        m_32apsk[13][i] = gr_complex((r3 * cos(M_PI / 2.0)), (r3 * sin(M_PI / 2.0)));
        m_32apsk[14][i] = gr_complex((r3 * cos(-7 * M_PI / 8.0)), (r3 * sin(-7 * M_PI / 8.0)));
        m_32apsk[15][i] = gr_complex((r3 * cos(-5 * M_PI / 8.0)), (r3 * sin(-5 * M_PI / 8.0)));
        m_32apsk[16][i] = gr_complex((r2 * cos(M_PI / 12.0)), (r2 * sin(M_PI / 12.0)));
        m_32apsk[17][i] = gr_complex((r1 * cos(M_PI / 4.0)), (r1 * sin(M_PI / 4.0)));
        m_32apsk[18][i] = gr_complex((r2 * cos(-M_PI / 12.0)), (r2 * sin(-M_PI / 12.0)));
        m_32apsk[19][i] = gr_complex((r1 * cos(-M_PI / 4.0)), (r1 * sin(-M_PI / 4.0)));
        m_32apsk[20][i] = gr_complex((r2 * cos(11 * M_PI / 12.0)), (r2 * sin(11 * M_PI / 12.0)));
        m_32apsk[21][i] = gr_complex((r1 * cos(3 * M_PI / 4.0)), (r1 * sin(3 * M_PI / 4.0)));
        m_32apsk[22][i] = gr_complex((r2 * cos(-11 * M_PI / 12.0)), (r2 * sin(-11 * M_PI / 12.0)));
        m_32apsk[23][i] = gr_complex((r1 * cos(-3 * M_PI / 4.0)), (r1 * sin(-3 * M_PI / 4.0)));
        m_32apsk[24][i] = gr_complex((r3 * cos(0.0)), (r3 * sin(0.0)));
        m_32apsk[25][i] = gr_complex((r3 * cos(M_PI / 4.0)), (r3 * sin(M_PI / 4.0)));
        m_32apsk[26][i] = gr_complex((r3 * cos(-M_PI / 8.0)), (r3 * sin(-M_PI / 8.0)));
        m_32apsk[27][i] = gr_complex((r3 * cos(-3 * M_PI / 8.0)), (r3 * sin(-3 * M_PI / 8.0)));
        m_32apsk[28][i] = gr_complex((r3 * cos(7 * M_PI / 8.0)), (r3 * sin(7 * M_PI / 8.0)));
        m_32apsk[29][i] = gr_complex((r3 * cos(5 * M_PI / 8.0)), (r3 * sin(5 * M_PI / 8.0)));
        m_32apsk[30][i] = gr_complex((r3 * cos(M_PI)), (r3 * sin(M_PI)));
        m_32apsk[31][i] = gr_complex((r3 * cos(-3 * M_PI / 4.0)), (r3 * sin(-3 * M_PI / 4.0)));
      }

      float m_4_12_16apsk_radius[3][2] = {{5.55, 2.85}, {5.54, 2.84}, {5.26, 2.84}};
      r3 = m;
      for (int i = 0; i < 3; i++) {
        r1 = r3 / m_4_12_16apsk_radius[i][0];
        r2 = r1 * m_4_12_16apsk_radius[i][1];
        m_4_12_16apsk[0][i] = gr_complex((r3 * cos(11 * M_PI / 16.0)), (r3 * sin(11 * M_PI / 16.0)));
        m_4_12_16apsk[1][i] = gr_complex((r3 * cos(9 * M_PI / 16.0)), (r3 * sin(9 * M_PI / 16.0)));
        m_4_12_16apsk[2][i] = gr_complex((r3 * cos(5 * M_PI / 16.0)), (r3 * sin(5 * M_PI / 16.0)));
        m_4_12_16apsk[3][i] = gr_complex((r3 * cos(7 * M_PI / 16.0)), (r3 * sin(7 * M_PI / 16.0)));
        m_4_12_16apsk[4][i] = gr_complex((r2 * cos(3 * M_PI / 4.0)), (r2 * sin(3 * M_PI / 4.0)));
        m_4_12_16apsk[5][i] = gr_complex((r2 * cos(7 * M_PI / 12.0)), (r2 * sin(7 * M_PI / 12.0)));
        m_4_12_16apsk[6][i] = gr_complex((r2 * cos(M_PI / 4.0)), (r2 * sin(M_PI / 4.0)));
        m_4_12_16apsk[7][i] = gr_complex((r2 * cos(5 * M_PI / 12.0)), (r2 * sin(5 * M_PI / 12.0)));
        m_4_12_16apsk[8][i] = gr_complex((r3 * cos(13 * M_PI / 16.0)), (r3 * sin(13 * M_PI / 16.0)));
        m_4_12_16apsk[9][i] = gr_complex((r3 * cos(15 * M_PI / 16.0)), (r3 * sin(15 * M_PI / 16.0)));
        m_4_12_16apsk[10][i] = gr_complex((r3 * cos(3 * M_PI / 16.0)), (r3 * sin(3 * M_PI / 16.0)));
        m_4_12_16apsk[11][i] = gr_complex((r3 * cos(M_PI / 16.0)), (r3 * sin(M_PI / 16.0)));
        m_4_12_16apsk[12][i] = gr_complex((r2 * cos(11 * M_PI / 12.0)), (r2 * sin(11 * M_PI / 12.0)));
        m_4_12_16apsk[13][i] = gr_complex((r1 * cos(3 * M_PI / 4.0)), (r1 * sin(3 * M_PI / 4.0)));
        m_4_12_16apsk[14][i] = gr_complex((r2 * cos(M_PI / 12.0)), (r2 * sin(M_PI / 12.0)));
        m_4_12_16apsk[15][i] = gr_complex((r1 * cos(M_PI / 4.0)), (r1 * sin(M_PI / 4.0)));
        m_4_12_16apsk[16][i] = gr_complex((r3 * cos(21 * M_PI / 16.0)), (r3 * sin(21 * M_PI / 16.0)));
        m_4_12_16apsk[17][i] = gr_complex((r3 * cos(23 * M_PI / 16.0)), (r3 * sin(23 * M_PI / 16.0)));
        m_4_12_16apsk[18][i] = gr_complex((r3 * cos(27 * M_PI / 16.0)), (r3 * sin(27 * M_PI / 16.0)));
        m_4_12_16apsk[19][i] = gr_complex((r3 * cos(25 * M_PI / 16.0)), (r3 * sin(25 * M_PI / 16.0)));
        m_4_12_16apsk[20][i] = gr_complex((r2 * cos(5 * M_PI / 4.0)), (r2 * sin(5 * M_PI / 4.0)));
        m_4_12_16apsk[21][i] = gr_complex((r2 * cos(17 * M_PI / 12.0)), (r2 * sin(17 * M_PI / 12.0)));
        m_4_12_16apsk[22][i] = gr_complex((r2 * cos(7 * M_PI / 4.0)), (r2 * sin(7 * M_PI / 4.0)));
        m_4_12_16apsk[23][i] = gr_complex((r2 * cos(19 * M_PI / 12.0)), (r2 * sin(19 * M_PI / 12.0)));
        m_4_12_16apsk[24][i] = gr_complex((r3 * cos(19 * M_PI / 16.0)), (r3 * sin(19 * M_PI / 16.0)));
        m_4_12_16apsk[25][i] = gr_complex((r3 * cos(17 * M_PI / 16.0)), (r3 * sin(17 * M_PI / 16.0)));
        m_4_12_16apsk[26][i] = gr_complex((r3 * cos(29 * M_PI / 16.0)), (r3 * sin(29 * M_PI / 16.0)));
        m_4_12_16apsk[27][i] = gr_complex((r3 * cos(31 * M_PI / 16.0)), (r3 * sin(31 * M_PI / 16.0)));
        m_4_12_16apsk[28][i] = gr_complex((r2 * cos(13 * M_PI / 12.0)), (r2 * sin(13 * M_PI / 12.0)));
        m_4_12_16apsk[29][i] = gr_complex((r1 * cos(5 * M_PI / 4.0)), (r1 * sin(5 * M_PI / 4.0)));
        m_4_12_16apsk[30][i] = gr_complex((r2 * cos(23 * M_PI / 12.0)), (r2 * sin(23 * M_PI / 12.0)));
        m_4_12_16apsk[31][i] = gr_complex((r1 * cos(7 * M_PI / 4.0)), (r1 * sin(7 * M_PI / 4.0)));
      }

      float m_4_8_4_16apsk_radius[3][3] = {{5.60, 2.99, 2.60}, {5.60, 2.86, 2.60}, {5.60, 3.08, 2.60}};
      r4 = m;
      for (int i = 0; i < 3; i++) {
        r1 = r4 / m_4_8_4_16apsk_radius[i][0];
        r3 = r1 * m_4_8_4_16apsk_radius[i][1];
        r2 = r1 * m_4_8_4_16apsk_radius[i][2];
        m_4_8_4_16apsk[0][i] = gr_complex((r1 * cos(M_PI / 4.0)), (r1 * sin(M_PI / 4.0)));
        m_4_8_4_16apsk[1][i] = gr_complex((r4 * cos(7 * M_PI / 16.0)), (r4 * sin(7 * M_PI / 16.0)));
        m_4_8_4_16apsk[2][i] = gr_complex((r1 * cos(7 * M_PI / 4.0)), (r1 * sin(7 * M_PI / 4.0)));
        m_4_8_4_16apsk[3][i] = gr_complex((r4 * cos(25 * M_PI / 16.0)), (r4 * sin(25 * M_PI / 16.0)));
        m_4_8_4_16apsk[4][i] = gr_complex((r1 * cos(3 * M_PI / 4.0)), (r1 * sin(3 * M_PI / 4.0)));
        m_4_8_4_16apsk[5][i] = gr_complex((r4 * cos(9 * M_PI / 16.0)), (r4 * sin(9 * M_PI / 16.0)));
        m_4_8_4_16apsk[6][i] = gr_complex((r1 * cos(5 * M_PI / 4.0)), (r1 * sin(5 * M_PI / 4.0)));
        m_4_8_4_16apsk[7][i] = gr_complex((r4 * cos(23 * M_PI / 16.0)), (r4 * sin(23 * M_PI / 16.0)));
        m_4_8_4_16apsk[8][i] = gr_complex((r2 * cos(M_PI / 12.0)), (r2 * sin(M_PI / 12.0)));
        m_4_8_4_16apsk[9][i] = gr_complex((r4 * cos(M_PI / 16.0)), (r4 * sin(M_PI / 16.0)));
        m_4_8_4_16apsk[10][i] = gr_complex((r2 * cos(23 * M_PI / 12.0)), (r2 * sin(23 * M_PI / 12.0)));
        m_4_8_4_16apsk[11][i] = gr_complex((r4 * cos(31 * M_PI / 16.0)), (r4 * sin(31 * M_PI / 16.0)));
        m_4_8_4_16apsk[12][i] = gr_complex((r2 * cos(11 * M_PI / 12.0)), (r2 * sin(11 * M_PI / 12.0)));
        m_4_8_4_16apsk[13][i] = gr_complex((r4 * cos(15 * M_PI / 16.0)), (r4 * sin(15 * M_PI / 16.0)));
        m_4_8_4_16apsk[14][i] = gr_complex((r2 * cos(13 * M_PI / 12.0)), (r2 * sin(13 * M_PI / 12.0)));
        m_4_8_4_16apsk[15][i] = gr_complex((r4 * cos(17 * M_PI / 16.0)), (r4 * sin(17 * M_PI / 16.0)));
        m_4_8_4_16apsk[16][i] = gr_complex((r2 * cos(5 * M_PI / 12.0)), (r2 * sin(5 * M_PI / 12.0)));
        m_4_8_4_16apsk[17][i] = gr_complex((r4 * cos(5 * M_PI / 16.0)), (r4 * sin(5 * M_PI / 16.0)));
        m_4_8_4_16apsk[18][i] = gr_complex((r2 * cos(19 * M_PI / 12.0)), (r2 * sin(19 * M_PI / 12.0)));
        m_4_8_4_16apsk[19][i] = gr_complex((r4 * cos(27 * M_PI / 16.0)), (r4 * sin(27 * M_PI / 16.0)));
        m_4_8_4_16apsk[20][i] = gr_complex((r2 * cos(7 * M_PI / 12.0)), (r2 * sin(7 * M_PI / 12.0)));
        m_4_8_4_16apsk[21][i] = gr_complex((r4 * cos(11 * M_PI / 16.0)), (r4 * sin(11 * M_PI / 16.0)));
        m_4_8_4_16apsk[22][i] = gr_complex((r2 * cos(17 * M_PI / 12.0)), (r2 * sin(17 * M_PI / 12.0)));
        m_4_8_4_16apsk[23][i] = gr_complex((r4 * cos(21 * M_PI / 16.0)), (r4 * sin(21 * M_PI / 16.0)));
        m_4_8_4_16apsk[24][i] = gr_complex((r3 * cos(M_PI / 4.0)), (r3 * sin(M_PI / 4.0)));
        m_4_8_4_16apsk[25][i] = gr_complex((r4 * cos(3 * M_PI / 16.0)), (r4 * sin(3 * M_PI / 16.0)));
        m_4_8_4_16apsk[26][i] = gr_complex((r3 * cos(7 * M_PI / 4.0)), (r3 * sin(7 * M_PI / 4.0)));
        m_4_8_4_16apsk[27][i] = gr_complex((r4 * cos(29 * M_PI / 16.0)), (r4 * sin(29 * M_PI / 16.0)));
        m_4_8_4_16apsk[28][i] = gr_complex((r3 * cos(3 * M_PI / 4.0)), (r3 * sin(3 * M_PI / 4.0)));
        m_4_8_4_16apsk[29][i] = gr_complex((r4 * cos(13 * M_PI / 16.0)), (r4 * sin(13 * M_PI / 16.0)));
        m_4_8_4_16apsk[30][i] = gr_complex((r3 * cos(5 * M_PI / 4.0)), (r3 * sin(5 * M_PI / 4.0)));
        m_4_8_4_16apsk[31][i] = gr_complex((r4 * cos(19 * M_PI / 16.0)), (r4 * sin(19 * M_PI / 16.0)));
      }

    }
};
