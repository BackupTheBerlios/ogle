/* SKROMPF - A video player
 * Copyright (C) 2000 Björn Englund, Håkan Hjort, Martin Norbäck
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



//#define LITTLE_ENDIAN
//#define BIG_ENDIAN 

#define MPEG2_PS_PACK_START_CODE 0x000001BA
#define MPEG2_PS_PROGRAM_END_CODE 0x000001B9
#define MPEG2_PS_SYSTEM_HEADER_START_CODE 0x000001BB
#define MPEG2_PES_PACKET_START_CODE_PREFIX 0x000001
/*
#else

#define MPEG2_PS_PACK_START_CODE 0xBA010000
#define MPEG2_PS_PROGRAM_END_CODE 0xB9010000
#define MPEG2_PS_SYSTEM_HEADER_START_CODE 0xBB010000
#define MPEG2_PES_PACKET_START_CODE_PREFIX 0x010000

#endif
*/
#define MPEG2_PS_PACK_START_CODE_SIZE 32
#define MPEG2_PS_PROGRAM_END_CODE_SIZE 32
#define MPEG2_PS_SYSTEM_HEADER_START_CODE_SIZE 32
#define MPEG2_PES_PACKET_START_CODE_PREFIX_SIZE 24


#define MPEG2_PRIVATE_STREAM_1 0xBD
#define MPEG2_PADDING_STREAM 0xBE
#define MPEG2_PRIVATE_STREAM_2 0xBF

