#ifndef YUV2RGB_H_INCLUDED
#define YUV2RGB_H_INCLUDED

/* Ogle - A video player
 * Copyright (C) 2000 Björn Englund, Håkan Hjort
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

#define MODE_RGB  0x1
#define MODE_BGR  0x2

typedef void (*yuv2rgb_fun)(uint8_t* image,
			    const uint8_t* py,
			    const uint8_t* pu,
			    const uint8_t* pv,
			    const uint32_t h_size,
			    const uint32_t v_size,
			    const uint32_t rgb_stride,
			    const uint32_t y_stride,
			    const uint32_t uv_stride);

extern yuv2rgb_fun yuv2rgb;

void yuv2rgb_init(uint32_t bpp, uint32_t mode);

#endif /* VIDEO_TYPES_H_INCLUDED */
