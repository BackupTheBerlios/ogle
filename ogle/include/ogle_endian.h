#ifndef ENDIAN_H_INCLUDED
#define ENDIAN_H_INCLUDED

/* Ogle - A video player
 * Copyright (C) 2001 Martin Norb�ck
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

/* For now, just 32 bit byteswap */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_BYTESWAP_H
#  include <byteswap.h>
#else
#  define bswap_32(x) \
     ((((x) & 0xff000000) >> 24) | \
      (((x) & 0x00ff0000) >>  8) | \
      (((x) & 0x0000ff00) <<  8) | \
      (((x) & 0x000000ff) << 24))
#endif

#ifdef WORDS_BIGENDIAN
#  define FROM_BE_32(x) (x)
#else
#  define FROM_BE_32(x) (bswap_32((x)))
#endif

#endif /* COMMON_H_INCLUDED */
