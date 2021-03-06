/* Ogle - A video player
 * Copyright (C) 2000, 2001 Bj�rn Englund, H�kan Hjort
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "video_tables.h"


/* Table B-12 --- Variable length codes for dct_dc_size_luminance */
const vlc_table_t table_b12[] = { 
  {    2,  0x000,     1 },
  {    2,  0x001,     2 },
  {    3,  0x004,     0 },
  {    3,  0x005,     3 },
  {    3,  0x006,     4 },
  {    4,  0x00E,     5 },
  {    5,  0x01E,     6 },
  {    6,  0x03E,     7 },
  {    7,  0x07E,     8 },
  {    8,  0x0FE,     9 },
  {    9,  0x1FE,    10 },
  {    9,  0x1FF,    11 },
  { VLC_FAIL, VLC_FAIL, VLC_FAIL } 
};

/* Table B-13 --- Variable length codes for dct_dc_size_chrominance */
const vlc_table_t table_b13[] = {
  {    2,  0x000,     0 },
  {    2,  0x001,     1 },
  {    2,  0x002,     2 },
  {    3,  0x006,     3 },
  {    4,  0x00E,     4 },
  {    5,  0x01E,     5 },
  {    6,  0x03E,     6 },
  {    7,  0x07E,     7 },
  {    8,  0x0FE,     8 },
  {    9,  0x1FE,     9 },
  {   10,  0x3FE,    10 },
  {   10,  0x3FF,    11 },
  { VLC_FAIL, VLC_FAIL, VLC_FAIL } 
};


/* Table B-14 --- DCT coefficients Table zero */
const vlc_rl_table table_b14[] = {
  /* DCT_DC_FIRST */
  //{	    1,	       0x1,         0,      1   },
  
  /* DCT_DC_SUBSEQUENT */
  {	    2,	       0x2,VLC_END_OF_BLOCK, VLC_END_OF_BLOCK},
  {	    2,	       0x3,         0,	    1	},// NOTE 2, 3
  
  {	    3,	       0x3,	    1,	    1	},
  {	    4,	       0x4,	    0,	    2	},
  {	    4,	       0x5,	    2,	    1	},
  {	    5,	       0x5,	    0,	    3	},
  {	    5,	       0x6,	    4,	    1	},
  {	    5,	       0x7,	    3,	    1	},
  {	    6,	       0x4,	    7,	    1	},
  {	    6,	       0x5,	    6,	    1	},
  {	    6,	       0x6,	    1,	    2	},
  {	    6,	       0x7,	    5,	    1	},
#if 1 /* Normal Escape */
  {	    6,	       0x1,VLC_ESCAPE,VLC_ESCAPE},  
#else /* Expanded Escape */
  {	    12,	      0x40,         0,VLC_ESCAPE},
  {	    12,	      0x41,         1,VLC_ESCAPE},
  {	    12,	      0x42,         2,VLC_ESCAPE},
  {	    12,	      0x43,         3,VLC_ESCAPE},
  {	    12,	      0x44,         4,VLC_ESCAPE},
  {	    12,	      0x45,         5,VLC_ESCAPE},
  {	    12,	      0x46,         6,VLC_ESCAPE},
  {	    12,	      0x47,         7,VLC_ESCAPE},
  {	    12,	      0x48,         8,VLC_ESCAPE},
  {	    12,	      0x49,         9,VLC_ESCAPE},
  {	    12,	      0x4a,        10,VLC_ESCAPE},
  {	    12,	      0x4b,        11,VLC_ESCAPE},
  {	    12,	      0x4c,        12,VLC_ESCAPE},
  {	    12,	      0x4d,        13,VLC_ESCAPE},
  {	    12,	      0x4e,        14,VLC_ESCAPE},
  {	    12,	      0x4f,        15,VLC_ESCAPE},
  {	    12,	      0x50,        16,VLC_ESCAPE},
  {	    12,	      0x51,        17,VLC_ESCAPE},
  {	    12,	      0x52,        18,VLC_ESCAPE},
  {	    12,	      0x53,        19,VLC_ESCAPE},
  {	    12,	      0x54,        20,VLC_ESCAPE},
  {	    12,	      0x55,        21,VLC_ESCAPE},
  {	    12,	      0x56,        22,VLC_ESCAPE},
  {	    12,	      0x57,        23,VLC_ESCAPE},
  {	    12,	      0x58,        24,VLC_ESCAPE},
  {	    12,	      0x59,        25,VLC_ESCAPE},
  {	    12,	      0x5a,        26,VLC_ESCAPE},
  {	    12,	      0x5b,        27,VLC_ESCAPE},
  {	    12,	      0x5c,        28,VLC_ESCAPE},
  {	    12,	      0x5d,        29,VLC_ESCAPE},
  {	    12,	      0x5e,        30,VLC_ESCAPE},
  {	    12,	      0x5f,        31,VLC_ESCAPE},
  {	    12,	      0x60,        32,VLC_ESCAPE},
  {	    12,	      0x61,        33,VLC_ESCAPE},
  {	    12,	      0x62,        34,VLC_ESCAPE},
  {	    12,	      0x63,        35,VLC_ESCAPE},
  {	    12,	      0x64,        36,VLC_ESCAPE},
  {	    12,	      0x65,        37,VLC_ESCAPE},
  {	    12,	      0x66,        38,VLC_ESCAPE},
  {	    12,	      0x67,        39,VLC_ESCAPE},
  {	    12,	      0x68,        40,VLC_ESCAPE},
  {	    12,	      0x69,        41,VLC_ESCAPE},
  {	    12,	      0x6a,        42,VLC_ESCAPE},
  {	    12,	      0x6b,        43,VLC_ESCAPE},
  {	    12,	      0x6c,        44,VLC_ESCAPE},
  {	    12,	      0x6d,        45,VLC_ESCAPE},
  {	    12,	      0x6e,        46,VLC_ESCAPE},
  {	    12,	      0x6f,        47,VLC_ESCAPE},
  {	    12,	      0x70,        48,VLC_ESCAPE},
  {	    12,	      0x71,        49,VLC_ESCAPE},
  {	    12,	      0x72,        50,VLC_ESCAPE},
  {	    12,	      0x73,        51,VLC_ESCAPE},
  {	    12,	      0x74,        52,VLC_ESCAPE},
  {	    12,	      0x75,        53,VLC_ESCAPE},
  {	    12,	      0x76,        54,VLC_ESCAPE},
  {	    12,	      0x77,        55,VLC_ESCAPE},
  {	    12,	      0x78,        56,VLC_ESCAPE},
  {	    12,	      0x79,        57,VLC_ESCAPE},
  {	    12,	      0x7a,        58,VLC_ESCAPE},
  {	    12,	      0x7b,        59,VLC_ESCAPE},
  {	    12,	      0x7c,        60,VLC_ESCAPE},
  {	    12,	      0x7d,        61,VLC_ESCAPE},
  {	    12,	      0x7e,        62,VLC_ESCAPE},
  {	    12,	      0x7f,        63,VLC_ESCAPE},
#endif
  {	    7,	       0x4,	    2,	    2	},
  {	    7,	       0x5,	    9,	    1	},
  {	    7,	       0x6,	    0,	    4	},
  {	    7,	       0x7,	    8,	    1	},
  {	    8,	      0x20,	   13,	    1	},
  {	    8,	      0x21,	    0,	    6	},
  {	    8,	      0x22,	   12,	    1	},
  {	    8,	      0x23,	   11,	    1	},
  {	    8,	      0x24,	    3,	    2	},
  {	    8,	      0x25,	    1,	    3	},
  {	    8,	      0x26,	    0,	    5	},
  {	    8,	      0x27,	   10,	    1	},
  {	   10,	       0x8,	   16,	    1	},
  {	   10,	       0x9,	    5,	    2	},
  {	   10,	       0xA,	    0,	    7	},
  {	   10,	       0xB,	    2,	    3	},
  {	   10,	       0xC,	    1,	    4	},
  {	   10,	       0xD,	   15,	    1	},
  {	   10,	       0xE,	   14,	    1	},
  {	   10,	       0xF,	    4,	    2	},
  {	   12,	      0x10,	    0,	   11	},
  {	   12,	      0x11,	    8,	    2	},
  {	   12,	      0x12,	    4,	    3	},
  {	   12,	      0x13,	    0,	   10	},
  {	   12,	      0x14,	    2,	    4	},
  {	   12,	      0x15,	    7,	    2	},
  {	   12,	      0x16,	   21,	    1	},
  {	   12,	      0x17,	   20,	    1	},
  {	   12,	      0x18,	    0,	    9	},
  {	   12,	      0x19,	   19,	    1	},
  {	   12,	      0x1A,	   18,	    1	},
  {	   12,	      0x1B,	    1,	    5	},
  {	   12,	      0x1C,	    3,	    3	},
  {	   12,	      0x1D,	    0,	    8	},
  {	   12,	      0x1E,	    6,	    2	},
  {	   12,	      0x1F,	   17,	    1	},
  {	   13,	      0x10,	   10,	    2	},
  {	   13,	      0x11,	    9,	    2	},
  {	   13,	      0x12,	    5,	    3	},
  {	   13,	      0x13,	    3,	    4	},
  {	   13,	      0x14,	    2,	    5	},
  {	   13,	      0x15,	    1,	    7	},
  {	   13,	      0x16,	    1,	    6	},
  {	   13,	      0x17,	    0,	   15	},
  {	   13,	      0x18,	    0,	   14	},
  {	   13,	      0x19,	    0,	   13	},
  {	   13,	      0x1A,	    0,	   12	},
  {	   13,	      0x1B,	   26,	    1	},
  {	   13,	      0x1C,	   25,	    1	},
  {	   13,	      0x1D,	   24,	    1	},
  {	   13,	      0x1E,	   23,	    1	},
  {	   13,	      0x1F,	   22,	    1	},
  {	   14,	      0x10,	    0,	   31	},
  {	   14,	      0x11,	    0,	   30	},
  {	   14,	      0x12,	    0,	   29	},
  {	   14,	      0x13,	    0,	   28	},
  {	   14,	      0x14,	    0,	   27	},
  {	   14,	      0x15,	    0,	   26	},
  {	   14,	      0x16,	    0,	   25	},
  {	   14,	      0x17,	    0,	   24	},
  {	   14,	      0x18,	    0,	   23	},
  {	   14,	      0x19,	    0,	   22	},
  {	   14,	      0x1A,	    0,	   21	},
  {	   14,	      0x1B,	    0,	   20	},
  {	   14,	      0x1C,	    0,	   19	},
  {	   14,	      0x1D,	    0,	   18	},
  {	   14,	      0x1E,	    0,	   17	},
  {	   14,	      0x1F,	    0,	   16	},
  {	   15,	      0x10,	    0,	   40	},
  {	   15,	      0x11,	    0,	   39	},
  {	   15,	      0x12,	    0,	   38	},
  {	   15,	      0x13,	    0,	   37	},
  {	   15,	      0x14,	    0,	   36	},
  {	   15,	      0x15,	    0,	   35	},
  {	   15,	      0x16,	    0,	   34	},
  {	   15,	      0x17,	    0,	   33	},
  {	   15,	      0x18,	    0,	   32	},
  {	   15,	      0x19,	    1,	   14	},
  {	   15,	      0x1A,	    1,	   13	},
  {	   15,	      0x1B,	    1,	   12	},
  {	   15,	      0x1C,	    1,	   11	},
  {	   15,	      0x1D,	    1,	   10	},
  {	   15,	      0x1E,	    1,	    9	},
  {	   15,	      0x1F,	    1,	    8	},
  {	   16,	      0x10,	    1,	   18	},
  {	   16,	      0x11,	    1,	   17	},
  {	   16,	      0x12,	    1,	   16	},
  {	   16,	      0x13,	    1,	   15	},
  {	   16,	      0x14,	    6,	    3	},
  {	   16,	      0x15,	   16,	    2	},
  {	   16,	      0x16,	   15,	    2	},
  {	   16,	      0x17,	   14,	    2	},
  {	   16,	      0x18,	   13,	    2	},
  {	   16,	      0x19,	   12,	    2	},
  {	   16,	      0x1A,	   11,	    2	},
  {	   16,	      0x1B,	   31,	    1	},
  {	   16,	      0x1C,	   30,	    1	},
  {	   16,	      0x1D,	   29,	    1	},
  {	   16,	      0x1E,	   28,	    1	},
  {	   16,	      0x1F,	   27,	    1	},
  {  VLC_FAIL,    VLC_FAIL,  VLC_FAIL, VLC_FAIL }
};

/* Table B-15 --- DCT coefficients Table one */
const vlc_rl_table table_b15[] = {
  {	    2,	       0x2,	    0,	    1	},
  {	    3,	       0x2,	    1,	    1	},
  {	    3,	       0x6,	    0,	    2	},
  {	    4,	       0x6, VLC_END_OF_BLOCK, VLC_END_OF_BLOCK},
  {	    4,	       0x7,	    0,	    3	},
  {	    5,	       0x5,	    2,	    1	},
  {	    5,	       0x6,	    1,	    2	},
  {	    5,	       0x7,	    3,	    1	},
  {	    5,	      0x1C,	    0,	    4	},
  {	    5,	      0x1D,	    0,	    5	},
  {	    6,	       0x4,	    0,	    7	},
  {	    6,	       0x5,	    0,	    6	},
  {	    6,	       0x6,	    4,	    1	},
  {	    6,	       0x7,	    5,	    1	},
#if 0 /* Normal Escape */
  {	    6,	       0x1,VLC_ESCAPE,VLC_ESCAPE},  
#else /* Expanded Escape */
  {	    12,	      0x40,         0,VLC_ESCAPE},
  {	    12,	      0x41,         1,VLC_ESCAPE},
  {	    12,	      0x42,         2,VLC_ESCAPE},
  {	    12,	      0x43,         3,VLC_ESCAPE},
  {	    12,	      0x44,         4,VLC_ESCAPE},
  {	    12,	      0x45,         5,VLC_ESCAPE},
  {	    12,	      0x46,         6,VLC_ESCAPE},
  {	    12,	      0x47,         7,VLC_ESCAPE},
  {	    12,	      0x48,         8,VLC_ESCAPE},
  {	    12,	      0x49,         9,VLC_ESCAPE},
  {	    12,	      0x4a,        10,VLC_ESCAPE},
  {	    12,	      0x4b,        11,VLC_ESCAPE},
  {	    12,	      0x4c,        12,VLC_ESCAPE},
  {	    12,	      0x4d,        13,VLC_ESCAPE},
  {	    12,	      0x4e,        14,VLC_ESCAPE},
  {	    12,	      0x4f,        15,VLC_ESCAPE},
  {	    12,	      0x50,        16,VLC_ESCAPE},
  {	    12,	      0x51,        17,VLC_ESCAPE},
  {	    12,	      0x52,        18,VLC_ESCAPE},
  {	    12,	      0x53,        19,VLC_ESCAPE},
  {	    12,	      0x54,        20,VLC_ESCAPE},
  {	    12,	      0x55,        21,VLC_ESCAPE},
  {	    12,	      0x56,        22,VLC_ESCAPE},
  {	    12,	      0x57,        23,VLC_ESCAPE},
  {	    12,	      0x58,        24,VLC_ESCAPE},
  {	    12,	      0x59,        25,VLC_ESCAPE},
  {	    12,	      0x5a,        26,VLC_ESCAPE},
  {	    12,	      0x5b,        27,VLC_ESCAPE},
  {	    12,	      0x5c,        28,VLC_ESCAPE},
  {	    12,	      0x5d,        29,VLC_ESCAPE},
  {	    12,	      0x5e,        30,VLC_ESCAPE},
  {	    12,	      0x5f,        31,VLC_ESCAPE},
  {	    12,	      0x60,        32,VLC_ESCAPE},
  {	    12,	      0x61,        33,VLC_ESCAPE},
  {	    12,	      0x62,        34,VLC_ESCAPE},
  {	    12,	      0x63,        35,VLC_ESCAPE},
  {	    12,	      0x64,        36,VLC_ESCAPE},
  {	    12,	      0x65,        37,VLC_ESCAPE},
  {	    12,	      0x66,        38,VLC_ESCAPE},
  {	    12,	      0x67,        39,VLC_ESCAPE},
  {	    12,	      0x68,        40,VLC_ESCAPE},
  {	    12,	      0x69,        41,VLC_ESCAPE},
  {	    12,	      0x6a,        42,VLC_ESCAPE},
  {	    12,	      0x6b,        43,VLC_ESCAPE},
  {	    12,	      0x6c,        44,VLC_ESCAPE},
  {	    12,	      0x6d,        45,VLC_ESCAPE},
  {	    12,	      0x6e,        46,VLC_ESCAPE},
  {	    12,	      0x6f,        47,VLC_ESCAPE},
  {	    12,	      0x70,        48,VLC_ESCAPE},
  {	    12,	      0x71,        49,VLC_ESCAPE},
  {	    12,	      0x72,        50,VLC_ESCAPE},
  {	    12,	      0x73,        51,VLC_ESCAPE},
  {	    12,	      0x74,        52,VLC_ESCAPE},
  {	    12,	      0x75,        53,VLC_ESCAPE},
  {	    12,	      0x76,        54,VLC_ESCAPE},
  {	    12,	      0x77,        55,VLC_ESCAPE},
  {	    12,	      0x78,        56,VLC_ESCAPE},
  {	    12,	      0x79,        57,VLC_ESCAPE},
  {	    12,	      0x7a,        58,VLC_ESCAPE},
  {	    12,	      0x7b,        59,VLC_ESCAPE},
  {	    12,	      0x7c,        60,VLC_ESCAPE},
  {	    12,	      0x7d,        61,VLC_ESCAPE},
  {	    12,	      0x7e,        62,VLC_ESCAPE},
  {	    12,	      0x7f,        63,VLC_ESCAPE},
#endif
  {	    7,	       0x4,	    7,	    1	},
  {	    7,	       0x5,	    8,	    1	},
  {	    7,	       0x6,	    6,	    1	},
  {	    7,	       0x7,	    2,	    2	},
  {	    7,	      0x78,	    9,	    1	},
  {	    7,	      0x79,	    1,	    3	},
  {	    7,	      0x7A,	   10,	    1	},
  {	    7,	      0x7B,	    0,	    8	},
  {	    7,	      0x7C,	    0,	    9	},
  {	    8,	      0x20,	    1,	    5	},
  {	    8,	      0x21,	   11,	    1	},
  {	    8,	      0x22,	    0,	   11	},
  {	    8,	      0x23,	    0,	   10	},
  {	    8,	      0x24,	   13,	    1	},
  {	    8,	      0x25,	   12,	    1	},
  {	    8,	      0x26,	    3,	    2	},
  {	    8,	      0x27,	    1,	    4	},
  {	    8,	      0xFA,	    0,	   12	},
  {	    8,	      0xFB,	    0,	   13	},
  {	    8,	      0xFC,	    2,	    3	},
  {	    8,	      0xFD,	    4,	    2	},
  {	    8,	      0xFE,	    0,	   14	},
  {	    8,	      0xFF,	    0,	   15	},
  {	    9,	       0x4,	    5,	    2	},
  {	    9,	       0x5,	   14,	    1	},
  {	    9,	       0x7,	   15,	    1	},
  {	   10,	       0xC,	    2,	    4	},
  {	   10,	       0xD,	   16,	    1	},
  {	   12,	      0x11,	    8,	    2	},
  {	   12,	      0x12,	    4,	    3	},
  {	   12,	      0x15,	    7,	    2	},
  {	   12,	      0x16,	   21,	    1	},
  {	   12,	      0x17,	   20,	    1	},
  {	   12,	      0x19,	   19,	    1	},
  {	   12,	      0x1A,	   18,	    1	},
  {	   12,	      0x1C,	    3,	    3	},
  {	   12,	      0x1E,	    6,	    2	},
  {	   12,	      0x1F,	   17,	    1	},
  {	   13,	      0x10,	   10,	    2	},
  {	   13,	      0x11,	    9,	    2	},
  {	   13,	      0x12,	    5,	    3	},
  {	   13,	      0x13,	    3,	    4	},
  {	   13,	      0x14,	    2,	    5	},
  {	   13,	      0x15,	    1,	    7	},
  {	   13,	      0x16,	    1,	    6	},
  {	   13,	      0x1B,	   26,	    1	},
  {	   13,	      0x1C,	   25,	    1	},
  {	   13,	      0x1D,	   24,	    1	},
  {	   13,	      0x1E,	   23,	    1	},
  {	   13,	      0x1F,	   22,	    1	},
  {	   14,	      0x10,	    0,	   31	},
  {	   14,	      0x11,	    0,	   30	},
  {	   14,	      0x12,	    0,	   29	},
  {	   14,	      0x13,	    0,	   28	},
  {	   14,	      0x14,	    0,	   27	},
  {	   14,	      0x15,	    0,	   26	},
  {	   14,	      0x16,	    0,	   25	},
  {	   14,	      0x17,	    0,	   24	},
  {	   14,	      0x18,	    0,	   23	},
  {	   14,	      0x19,	    0,	   22	},
  {	   14,	      0x1A,	    0,	   21	},
  {	   14,	      0x1B,	    0,	   20	},
  {	   14,	      0x1C,	    0,	   19	},
  {	   14,	      0x1D,	    0,	   18	},
  {	   14,	      0x1E,	    0,	   17	},
  {	   14,	      0x1F,	    0,	   16	},
  {	   15,	      0x10,	    0,	   40	},
  {	   15,	      0x11,	    0,	   39	},
  {	   15,	      0x12,	    0,	   38	},
  {	   15,	      0x13,	    0,	   37	},
  {	   15,	      0x14,	    0,	   36	},
  {	   15,	      0x15,	    0,	   35	},
  {	   15,	      0x16,	    0,	   34	},
  {	   15,	      0x17,	    0,	   33	},
  {	   15,	      0x18,	    0,	   32	},
  {	   15,	      0x19,	    1,	   14	},
  {	   15,	      0x1A,	    1,	   13	},
  {	   15,	      0x1B,	    1,	   12	},
  {	   15,	      0x1C,	    1,	   11	},
  {	   15,	      0x1D,	    1,	   10	},
  {	   15,	      0x1E,	    1,	    9	},
  {	   15,	      0x1F,	    1,	    8	},
  {	   16,	      0x10,	    1,	   18	},
  {	   16,	      0x11,	    1,	   17	},
  {	   16,	      0x12,	    1,	   16	},
  {	   16,	      0x13,	    1,	   15	},
  {	   16,	      0x14,	    6,	    3	},
  {	   16,	      0x15,	   16,	    2	},
  {	   16,	      0x16,	   15,	    2	},
  {	   16,	      0x17,	   14,	    2	},
  {	   16,	      0x18,	   13,	    2	},
  {	   16,	      0x19,	   12,	    2	},
  {	   16,	      0x1A,	   11,	    2	},
  {	   16,	      0x1B,	   31,	    1	},
  {	   16,	      0x1C,	   30,	    1	},
  {	   16,	      0x1D,	   29,	    1	},
  {	   16,	      0x1E,	   28,	    1	},
  {	   16,	      0x1F,	   27,	    1	},
  {  VLC_FAIL,    VLC_FAIL,  VLC_FAIL, VLC_FAIL }  
};


/* Table B-2  Variable length codes for macroblock_type in I-pictures */
const vlc_table_t table_b2[]= {
  { 1, 1, MACROBLOCK_INTRA },
  { 2, 1, MACROBLOCK_QUANT | MACROBLOCK_INTRA },
  { VLC_FAIL, VLC_FAIL, VLC_FAIL }
};

/* Table B-3  Variable length codes for macroblock_type in P-pictures */
const vlc_table_t table_b3[]= {
  { 1, 1, MACROBLOCK_MOTION_FORWARD | MACROBLOCK_PATTERN },
  { 2, 1, MACROBLOCK_PATTERN },
  { 3, 1, MACROBLOCK_MOTION_FORWARD },
  { 5, 3, MACROBLOCK_INTRA },
  { 5, 2, MACROBLOCK_QUANT | MACROBLOCK_MOTION_FORWARD | MACROBLOCK_PATTERN },
  { 5, 1, MACROBLOCK_QUANT | MACROBLOCK_PATTERN },
  { 6, 1, MACROBLOCK_QUANT | MACROBLOCK_INTRA },
  { VLC_FAIL, VLC_FAIL, VLC_FAIL }
};

/* Table B-4  Variable length codes for macroblock_type in B-pictures */
const vlc_table_t table_b4[]= {
  { 2, 2, MACROBLOCK_MOTION_FORWARD | MACROBLOCK_MOTION_BACKWARD },
  { 2, 3, MACROBLOCK_MOTION_FORWARD | MACROBLOCK_MOTION_BACKWARD | MACROBLOCK_PATTERN },
  { 3, 2, MACROBLOCK_MOTION_BACKWARD },
  { 3, 3, MACROBLOCK_MOTION_BACKWARD | MACROBLOCK_PATTERN },
  { 4, 2, MACROBLOCK_MOTION_FORWARD}, 
  { 4, 3, MACROBLOCK_MOTION_FORWARD | MACROBLOCK_PATTERN},
  { 5, 3, MACROBLOCK_INTRA},
  { 5, 2, MACROBLOCK_QUANT | MACROBLOCK_MOTION_FORWARD | MACROBLOCK_MOTION_BACKWARD | MACROBLOCK_PATTERN},
  { 6, 3, MACROBLOCK_QUANT | MACROBLOCK_MOTION_FORWARD | MACROBLOCK_PATTERN},
  { 6, 2, MACROBLOCK_QUANT | MACROBLOCK_MOTION_BACKWARD | MACROBLOCK_PATTERN},
  { 6, 1, MACROBLOCK_QUANT | MACROBLOCK_INTRA },
  { VLC_FAIL, VLC_FAIL, VLC_FAIL }
};


/* Table B-1 --- Variable length codes for macroblock_address_increment */
const vlc_table_t table_b1[] = {
  {	    1,	  0x1,	    1	},
  {	    3,	  0x3,	    2	},
  {	    3,	  0x2,	    3	},
  {	    4,	  0x3,	    4	},
  {	    4,	  0x2,	    5	},
  {	    5,	  0x3,	    6	},
  {	    5,	  0x2,	    7	},
  {	    7,	  0x7,	    8	},
  {	    7,	  0x6,	    9	},
  {	    8,	  0xB,	   10	},
  {	    8,	  0xA,	   11	},
  {	    8,	  0x9,	   12	},
  {	    8,	  0x8,	   13	},
  {	    8,	  0x7,	   14	},
  {	    8,	  0x6,	   15	},
  {	   10,	 0x17,	   16	},
  {	   10,	 0x16,	   17	},
  {	   10,	 0x15,	   18	},
  {	   10,	 0x14,	   19	},
  {	   10,	 0x13,	   20	},
  {	   10,	 0x12,	   21	},
  {	   11,	 0x23,	   22	},
  {	   11,	 0x22,	   23	},
  {	   11,	 0x21,	   24	},
  {	   11,	 0x20,	   25	},
  {	   11,	 0x1F,	   26	},
  {	   11,	 0x1E,	   27	},
  {	   11,	 0x1D,	   28	},
  {	   11,	 0x1C,	   29	},
  {	   11,	 0x1B,	   30	},
  {	   11,	 0x1A,	   31	},
  {	   11,	 0x19,	   32	},
  {	   11,	 0x18,	   33	},
  {	   11,	  0x8,	VLC_MACROBLOCK_ESCAPE },
  { VLC_FAIL, VLC_FAIL, VLC_FAIL}
};

/* Table B-10 --- Variable length codes for motion_code */
const vlc_table_t table_b10[] = {
  {	    1,	  0x1,	    0	},
  {	    3,	  0x2,	    1	},
  {	    3,	  0x3,	   -1	},
  {	    4,	  0x2,	    2	},
  {	    4,	  0x3,	   -2	},
  {	    5,	  0x2,	    3	},
  {	    5,	  0x3,	   -3	},
  {	    7,	  0x6,	    4	},
  {	    7,	  0x7,	   -4	},
  {	    8,	  0x6,	    7	},
  {	    8,	  0x7,	   -7	},
  {	    8,	  0x8,	    6	},
  {	    8,	  0x9,	   -6	},
  {	    8,	  0xA,	    5	},
  {	    8,	  0xB,	   -5	},
  {	   10,	 0x12,	   10	},
  {	   10,	 0x13,	  -10	},
  {	   10,	 0x14,	    9	},
  {	   10,	 0x15,	   -9	},
  {	   10,	 0x16,	    8	},
  {	   10,	 0x17,	   -8	},
  {	   11,	 0x18,	   16	},
  {	   11,	 0x19,	  -16	},
  {	   11,	 0x1A,	   15	},
  {	   11,	 0x1B,	  -15	},
  {	   11,	 0x1C,	   14	},
  {	   11,	 0x1D,	  -14	},
  {	   11,	 0x1E,	   13	},
  {	   11,	 0x1F,	  -13	},
  {	   11,	 0x20,	   12	},
  {	   11,	 0x21,	  -12	},
  {	   11,	 0x22,	   11	},
  {	   11,	 0x23,	  -11	},
  { VLC_FAIL, VLC_FAIL, VLC_FAIL}
};

/* Table B-11  Variable length codes for dmvector[t] */
const vlc_table_t table_b11[] = {
  {	    1,	  0x0,	    0	},
  {	    2,	  0x2,	    1	},
  {	    2,	  0x3,	   -1	},
  { VLC_FAIL, VLC_FAIL, VLC_FAIL}
};

/* Table B-9 --- Variable length codes for coded_block_pattern. */
const vlc_table_t table_b9[] = {
  {	    3,	  0x7,	   60	},
  {	    4,	  0xD,	    4	},
  {	    4,	  0xC,	    8	},
  {	    4,	  0xB,	   16	},
  {	    4,	  0xA,	   32	},
  {	    5,	 0x13,	   12	},
  {	    5,	 0x12,	   48	},
  {	    5,	 0x11,	   20	},
  {	    5,	 0x10,	   40	},
  {	    5,	  0xF,	   28	},
  {	    5,	  0xE,	   44	},
  {	    5,	  0xD,	   52	},
  {	    5,	  0xC,	   56	},
  {	    5,	  0xB,	    1	},
  {	    5,	  0xA,	   61	},
  {	    5,	  0x9,	    2	},
  {	    5,	  0x8,	   62	},
  {	    6,	  0xF,	   24	},
  {	    6,	  0xE,	   36	},
  {	    6,	  0xD,	    3	},
  {	    6,	  0xC,	   63	},
  {	    7,	 0x17,	    5	},
  {	    7,	 0x16,	    9	},
  {	    7,	 0x15,	   17	},
  {	    7,	 0x14,	   33	},
  {	    7,	 0x13,	    6	},
  {	    7,	 0x12,	   10	},
  {	    7,	 0x11,	   18	},
  {	    7,	 0x10,	   34	},
  {	    8,	 0x1F,	    7	},
  {	    8,	 0x1E,	   11	},
  {	    8,	 0x1D,	   19	},
  {	    8,	 0x1C,	   35	},
  {	    8,	 0x1B,	   13	},
  {	    8,	 0x1A,	   49	},
  {	    8,	 0x19,	   21	},
  {	    8,	 0x18,	   41	},
  {	    8,	 0x17,	   14	},
  {	    8,	 0x16,	   50	},
  {	    8,	 0x15,	   22	},
  {	    8,	 0x14,	   42	},
  {	    8,	 0x13,	   15	},
  {	    8,	 0x12,	   51	},
  {	    8,	 0x11,	   23	},
  {	    8,	 0x10,	   43	},
  {	    8,	  0xF,	   25	},
  {	    8,	  0xE,	   37	},
  {	    8,	  0xD,	   26	},
  {	    8,	  0xC,	   38	},
  {	    8,	  0xB,	   29	},
  {	    8,	  0xA,	   45	},
  {	    8,	  0x9,	   53	},
  {	    8,	  0x8,	   57	},
  {	    8,	  0x7,	   30	},
  {	    8,	  0x6,	   46	},
  {	    8,	  0x5,	   54	},
  {	    8,	  0x4,	   58	},
  {	    9,	  0x7,	   31	},
  {	    9,	  0x6,	   47	},
  {	    9,	  0x5,	   55	},
  {	    9,	  0x4,	   59	},
  {	    9,	  0x3,	   27	},
  {	    9,	  0x2,	   39	},
  {	    9,	  0x1,	    0	},
  { VLC_FAIL, VLC_FAIL, VLC_FAIL}
};


#if defined(HAVE_ALTIVEC)
/* Transposed version for the AltiVec IDCT */
const uint8_t inverse_scan[2][64] = {
  /* Derived from Figure 7-1. Definition of scan[0][v][u] */
  {  0,  8,  1,  2,  9, 16, 24, 17,
    10,  3,  4, 11, 18, 25, 32, 40,
    33, 26, 19, 12,  5,  6, 13, 20,
    27, 34, 41, 48, 56, 49, 42, 35,
    28, 21, 14,  7, 15, 22, 29, 36,
    43, 50, 57, 58, 51, 44, 37, 30,
    23, 31, 38, 45, 52, 59, 60, 53,  
    46, 39, 47, 54, 61, 62, 55, 63
  }, 
  /* Derived from Figure 7-2. Definition of scan[1][v][u] */
  {  0,  1,  2,  3,  8,  9, 16, 17,
    10, 11,  4,  5,  6,  7, 15, 14,
    13, 12, 19, 18, 24, 25, 32, 33,
    26, 27, 20, 21, 22, 23, 28, 29,
    30, 31, 34, 35, 40, 41, 48, 49,
    42, 43, 36, 37, 38, 39, 44, 45,
    46, 47, 50, 51, 56, 57, 58, 59,
    52, 53, 54, 55, 60, 61, 62, 63,
  }
};
#elif defined(HAVE_MMX)
/* Wierd version for the MMX IDCT */
const uint8_t inverse_scan[2][64] = {
  /* Derived from Figure 7-1. Definition of scan[0][v][u] */
  {  0,  4,  8, 16, 12,  1,  5,  9,
    20, 24, 32, 28, 17, 13,  2,  6,
    10, 21, 25, 36, 40, 48, 44, 33,
    29, 18, 14,  3,  7, 11, 22, 26,
    37, 41, 52, 56, 60, 49, 45, 34,
    30, 19, 15, 23, 27, 38, 42, 53,
    57, 61, 50, 46, 35, 31, 39, 43,
    54, 58, 62, 51, 47, 55, 59, 63,
  },
  {  0,  8, 16, 24,  4, 12,  1,  9,
    20, 28, 32, 40, 48, 56, 60, 52,
    44, 36, 25, 17,  5, 13,  2, 10,
    21, 29, 33, 41, 49, 57, 37, 45,
    53, 61, 18, 26,  6, 14,  3, 11,
    22, 30, 34, 42, 50, 58, 38, 46,
    54, 62, 19, 27,  7, 15, 23, 31,
    35, 43, 51, 59, 39, 47, 55, 63,
  }
};
#else
const uint8_t inverse_scan[2][64] = {
  /* Derived from Figure 7-1. Definition of scan[0][v][u] */
  {  0,  1,  8, 16,  9,  2,  3, 10,  
    17, 24, 32, 25, 18, 11,  4,  5,  
    12, 19, 26, 33, 40, 48, 41, 34,  
    27, 20, 13,  6,  7, 14, 21, 28,  
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
  }, 
  /* Derived from Figure 7-2. Definition of scan[1][v][u] */
  {  0,  8, 16, 24,  1,  9,  2, 10,
    17, 25, 32, 40, 48, 56, 57, 49,
    41, 33, 26, 18,  3, 11,  4, 12,
    19, 27, 34, 42, 50, 58, 35, 43,
    51, 59, 20, 28,  5, 13,  6, 14,
    21, 29, 36, 44, 52, 60, 37, 45,
    53, 61, 22, 30,  7, 15, 23, 31,
    38, 46, 54, 62, 39, 47, 55, 63
  }
};
#endif

/* Table 7-6. Relation between quantiser_scale and quantiser_scale_code */
const uint8_t q_scale[2][32] = {
  { 255,  2,  4,  6,  8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 
     32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62 },
  
  { 255,   1, 2,  3,  4,  5,  6,  7,  8, 10, 12, 14, 16, 18, 20, 22,
     24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72, 80, 88, 96,104,112 }
};



/* 6.3.7 Quant matrix extension */
#if defined(HAVE_ALTIVEC)
/* Transposed version for the AltiVec IDCTs */
const int8_t default_intra_inverse_quantiser_matrix[8][8] = {
  {  8, 16, 19, 22, 22, 26, 26, 27 },
  { 16, 16, 22, 22, 26, 27, 27, 29 },
  { 19, 22, 26, 26, 27, 29, 29, 35 },
  { 22, 24, 27, 27, 29, 32, 34, 38 },
  { 26, 27, 29, 29, 32, 35, 38, 46 },
  { 27, 29, 34, 34, 35, 40, 46, 56 },
  { 29, 34, 34, 37, 40, 48, 56, 69 },
  { 34, 37, 38, 40, 48, 58, 69, 83 }
};
#elif defined(HAVE_MMX)
const int8_t default_intra_inverse_quantiser_matrix[8][8] = {
  {  8, 19, 26, 29, 16, 22, 27, 34 },
  { 16, 22, 27, 34, 16, 24, 29, 37 },
  { 19, 26, 29, 34, 22, 27, 34, 38 },
  { 22, 26, 29, 37, 22, 27, 34, 40 },
  { 22, 27, 32, 40, 26, 29, 35, 48 },
  { 26, 29, 35, 48, 27, 32, 40, 58 },
  { 26, 29, 38, 56, 27, 34, 46, 69 },
  { 27, 35, 46, 69, 29, 38, 56, 83 }
};
#else
const int8_t default_intra_inverse_quantiser_matrix[8][8] = {
  {  8, 16, 19, 22, 26, 27, 29, 34 },
  { 16, 16, 22, 24, 27, 29, 34, 37 },
  { 19, 22, 26, 27, 29, 34, 34, 38 },
  { 22, 22, 26, 27, 29, 34, 37, 40 },
  { 22, 26, 27, 29, 32, 35, 40, 48 },
  { 26, 27, 29, 32, 35, 40, 48, 58 },
  { 26, 27, 29, 34, 38, 46, 56, 69 },
  { 27, 29, 35, 38, 46, 56, 69, 83 }
};
#endif

const int8_t default_non_intra_inverse_quantiser_matrix[8][8] = {
  { 16, 16, 16, 16, 16, 16, 16, 16 },
  { 16, 16, 16, 16, 16, 16, 16, 16 },
  { 16, 16, 16, 16, 16, 16, 16, 16 },
  { 16, 16, 16, 16, 16, 16, 16, 16 },
  { 16, 16, 16, 16, 16, 16, 16, 16 },
  { 16, 16, 16, 16, 16, 16, 16, 16 },
  { 16, 16, 16, 16, 16, 16, 16, 16 },
  { 16, 16, 16, 16, 16, 16, 16, 16 }
};




/* Table B-14, DCT coefficients table zero,
 * codes 0100 ... 1xxx (used for first (DC) coefficient)
 */
const DCTtab DCTtabfirst[12] =
{
  { 0, 2, 4}, {2, 1, 4}, {1, 1, 3}, {1, 1, 3},
  { 0, 1, 1}, {0, 1, 1}, {0, 1, 1}, {0, 1, 1},
  { 0, 1, 1}, {0, 1, 1}, {0, 1, 1}, {0, 1, 1}
};

/* Table B-14, DCT coefficients table zero,
 * codes 0100 ... 1xxx (used for all other coefficients)
 */
const DCTtab DCTtabnext[12] =
{
  { 0, 2, 4}, { 2, 1, 4}, { 1, 1, 3}, { 1, 1, 3},
  {64, 0, 2}, {64, 0, 2}, {64, 0, 2}, {64, 0, 2}, /* EOB */
  { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}
};

/* Table B-14, DCT coefficients table zero,
 * codes 000001xx ... 00111xxx
 */
const DCTtab DCTtab0[60] =
{
  {65, 0, 6}, {65, 0, 6}, {65, 0, 6}, {65, 0, 6}, /* Escape */
  { 2, 2, 7}, { 2, 2, 7}, { 9, 1, 7}, { 9, 1, 7},
  { 0, 4, 7}, { 0, 4, 7}, { 8, 1, 7}, { 8, 1, 7},
  { 7, 1, 6}, { 7, 1, 6}, { 7, 1, 6}, { 7, 1, 6},
  { 6, 1, 6}, { 6, 1, 6}, { 6, 1, 6}, { 6, 1, 6},
  { 1, 2, 6}, { 1, 2, 6}, { 1, 2, 6}, { 1, 2, 6},
  { 5, 1, 6}, { 5, 1, 6}, { 5, 1, 6}, { 5, 1, 6},
  {13, 1, 8}, { 0, 6, 8}, {12, 1, 8}, {11, 1, 8},
  { 3, 2, 8}, { 1, 3, 8}, { 0, 5, 8}, {10, 1, 8},
  { 0, 3, 5}, { 0, 3, 5}, { 0, 3, 5}, { 0, 3, 5},
  { 0, 3, 5}, { 0, 3, 5}, { 0, 3, 5}, { 0, 3, 5},
  { 4, 1, 5}, { 4, 1, 5}, { 4, 1, 5}, { 4, 1, 5},
  { 4, 1, 5}, { 4, 1, 5}, { 4, 1, 5}, { 4, 1, 5},
  { 3, 1, 5}, { 3, 1, 5}, { 3, 1, 5}, { 3, 1, 5},
  { 3, 1, 5}, { 3, 1, 5}, { 3, 1, 5}, { 3, 1, 5}
};

/* Table B-15, DCT coefficients table one,
 * codes 000001xx ... 11111111
*/
const DCTtab DCTtab0a[252] =
{
  {65, 0, 6}, {65, 0, 6}, {65, 0, 6}, {65, 0, 6}, /* Escape */
  { 7, 1, 7}, { 7, 1, 7}, { 8, 1, 7}, { 8, 1, 7},
  { 6, 1, 7}, { 6, 1, 7}, { 2, 2, 7}, { 2, 2, 7},
  { 0, 7, 6}, { 0, 7, 6}, { 0, 7, 6}, { 0, 7, 6},
  { 0, 6, 6}, { 0, 6, 6}, { 0, 6, 6}, { 0, 6, 6},
  { 4, 1, 6}, { 4, 1, 6}, { 4, 1, 6}, { 4, 1, 6},
  { 5, 1, 6}, { 5, 1, 6}, { 5, 1, 6}, { 5, 1, 6},
  { 1, 5, 8}, {11, 1, 8}, { 0,11, 8}, { 0,10, 8},
  {13, 1, 8}, {12, 1, 8}, { 3, 2, 8}, { 1, 4, 8},
  { 2, 1, 5}, { 2, 1, 5}, { 2, 1, 5}, { 2, 1, 5},
  { 2, 1, 5}, { 2, 1, 5}, { 2, 1, 5}, { 2, 1, 5},
  { 1, 2, 5}, { 1, 2, 5}, { 1, 2, 5}, { 1, 2, 5},
  { 1, 2, 5}, { 1, 2, 5}, { 1, 2, 5}, { 1, 2, 5},
  { 3, 1, 5}, { 3, 1, 5}, { 3, 1, 5}, { 3, 1, 5},
  { 3, 1, 5}, { 3, 1, 5}, { 3, 1, 5}, { 3, 1, 5},
  { 1, 1, 3}, { 1, 1, 3}, { 1, 1, 3}, { 1, 1, 3},
  { 1, 1, 3}, { 1, 1, 3}, { 1, 1, 3}, { 1, 1, 3},
  { 1, 1, 3}, { 1, 1, 3}, { 1, 1, 3}, { 1, 1, 3},
  { 1, 1, 3}, { 1, 1, 3}, { 1, 1, 3}, { 1, 1, 3},
  { 1, 1, 3}, { 1, 1, 3}, { 1, 1, 3}, { 1, 1, 3},
  { 1, 1, 3}, { 1, 1, 3}, { 1, 1, 3}, { 1, 1, 3},
  { 1, 1, 3}, { 1, 1, 3}, { 1, 1, 3}, { 1, 1, 3},
  { 1, 1, 3}, { 1, 1, 3}, { 1, 1, 3}, { 1, 1, 3},
  {64, 0, 4}, {64, 0, 4}, {64, 0, 4}, {64, 0, 4}, /* EOB */
  {64, 0, 4}, {64, 0, 4}, {64, 0, 4}, {64, 0, 4},
  {64, 0, 4}, {64, 0, 4}, {64, 0, 4}, {64, 0, 4},
  {64, 0, 4}, {64, 0, 4}, {64, 0, 4}, {64, 0, 4},
  { 0, 3, 4}, { 0, 3, 4}, { 0, 3, 4}, { 0, 3, 4},
  { 0, 3, 4}, { 0, 3, 4}, { 0, 3, 4}, { 0, 3, 4},
  { 0, 3, 4}, { 0, 3, 4}, { 0, 3, 4}, { 0, 3, 4},
  { 0, 3, 4}, { 0, 3, 4}, { 0, 3, 4}, { 0, 3, 4},
  { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2},
  { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2},
  { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2},
  { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2},
  { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2},
  { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2},
  { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2},
  { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2},
  { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2},
  { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2},
  { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2},
  { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2},
  { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2},
  { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2},
  { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2},
  { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2}, { 0, 1, 2},
  { 0, 2, 3}, { 0, 2, 3}, { 0, 2, 3}, { 0, 2, 3},
  { 0, 2, 3}, { 0, 2, 3}, { 0, 2, 3}, { 0, 2, 3},
  { 0, 2, 3}, { 0, 2, 3}, { 0, 2, 3}, { 0, 2, 3},
  { 0, 2, 3}, { 0, 2, 3}, { 0, 2, 3}, { 0, 2, 3},
  { 0, 2, 3}, { 0, 2, 3}, { 0, 2, 3}, { 0, 2, 3},
  { 0, 2, 3}, { 0, 2, 3}, { 0, 2, 3}, { 0, 2, 3},
  { 0, 2, 3}, { 0, 2, 3}, { 0, 2, 3}, { 0, 2, 3},
  { 0, 2, 3}, { 0, 2, 3}, { 0, 2, 3}, { 0, 2, 3},
  { 0, 4, 5}, { 0, 4, 5}, { 0, 4, 5}, { 0, 4, 5},
  { 0, 4, 5}, { 0, 4, 5}, { 0, 4, 5}, { 0, 4, 5},
  { 0, 5, 5}, { 0, 5, 5}, { 0, 5, 5}, { 0, 5, 5},
  { 0, 5, 5}, { 0, 5, 5}, { 0, 5, 5}, { 0, 5, 5},
  { 9, 1, 7}, { 9, 1, 7}, { 1, 3, 7}, { 1, 3, 7},
  {10, 1, 7}, {10, 1, 7}, { 0, 8, 7}, { 0, 8, 7},
  { 0, 9, 7}, { 0, 9, 7}, { 0,12, 8}, { 0,13, 8},
  { 2, 3, 8}, { 4, 2, 8}, { 0,14, 8}, { 0,15, 8}
};

/* Table B-14, DCT coefficients table zero,
 * codes 0000001000 ... 0000001111
 */
const DCTtab DCTtab1[8] =
{
  {16, 1,10}, { 5, 2,10}, { 0, 7,10}, { 2, 3,10},
  { 1, 4,10}, {15, 1,10}, {14, 1,10}, { 4, 2,10}
};

/* Table B-15, DCT coefficients table one,
 * codes 000000100x ... 000000111x
 */
const DCTtab DCTtab1a[8] =
{
  { 5, 2, 9}, { 5, 2, 9}, {14, 1, 9}, {14, 1, 9},
  { 2, 4,10}, {16, 1,10}, {15, 1, 9}, {15, 1, 9}
};

/* Table B-14/15, DCT coefficients table zero / one,
 * codes 000000010000 ... 000000011111
 */
const DCTtab DCTtab2[16] =
{
  { 0,11,12}, { 8, 2,12}, { 4, 3,12}, { 0,10,12},
  { 2, 4,12}, { 7, 2,12}, {21, 1,12}, {20, 1,12},
  { 0, 9,12}, {19, 1,12}, {18, 1,12}, { 1, 5,12},
  { 3, 3,12}, { 0, 8,12}, { 6, 2,12}, {17, 1,12}
};

/* Table B-14/15, DCT coefficients table zero / one,
 * codes 0000000010000 ... 0000000011111
 */
const DCTtab DCTtab3[16] =
{
  {10, 2,13}, { 9, 2,13}, { 5, 3,13}, { 3, 4,13},
  { 2, 5,13}, { 1, 7,13}, { 1, 6,13}, { 0,15,13},
  { 0,14,13}, { 0,13,13}, { 0,12,13}, {26, 1,13},
  {25, 1,13}, {24, 1,13}, {23, 1,13}, {22, 1,13}
};

/* Table B-14/15, DCT coefficients table zero / one,
 * codes 00000000010000 ... 00000000011111
 */
const DCTtab DCTtab4[16] =
{
  { 0,31,14}, { 0,30,14}, { 0,29,14}, { 0,28,14},
  { 0,27,14}, { 0,26,14}, { 0,25,14}, { 0,24,14},
  { 0,23,14}, { 0,22,14}, { 0,21,14}, { 0,20,14},
  { 0,19,14}, { 0,18,14}, { 0,17,14}, { 0,16,14}
};

/* Table B-14/15, DCT coefficients table zero / one,
 * codes 000000000010000 ... 000000000011111
 */
const DCTtab DCTtab5[16] =
{
  { 0,40,15}, { 0,39,15}, { 0,38,15}, { 0,37,15},
  { 0,36,15}, { 0,35,15}, { 0,34,15}, { 0,33,15},
  { 0,32,15}, { 1,14,15}, { 1,13,15}, { 1,12,15},
  { 1,11,15}, { 1,10,15}, { 1, 9,15}, { 1, 8,15}
};

/* Table B-14/15, DCT coefficients table zero / one,
 * codes 0000000000010000 ... 0000000000011111
 */
const DCTtab DCTtab6[16] =
{
  { 1,18,16}, { 1,17,16}, { 1,16,16}, { 1,15,16},
  { 6, 3,16}, {16, 2,16}, {15, 2,16}, {14, 2,16},
  {13, 2,16}, {12, 2,16}, {11, 2,16}, {31, 1,16},
  {30, 1,16}, {29, 1,16}, {28, 1,16}, {27, 1,16}
};

