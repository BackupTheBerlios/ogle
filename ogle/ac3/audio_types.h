#ifndef AUDIO_TYPES_H
#define AUDIO_TYPES_H

/* Ogle - A video player
 * Copyright (C) 2002 Björn Englund
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

typedef enum {
  ChannelType_Unspecified = -1,
  ChannelType_Null = 0,
  ChannelType_Left = 1,
  ChannelType_Right = 2,
  ChannelType_Center = 4,
  ChannelType_LeftSurround = 8,
  ChannelType_RightSurround = 16,
  ChannelType_LFE = 64,
  ChannelType_Surround = 32,
  ChannelType_CenterSurround = 128,
  ChannelType_Mono = 256
} ChannelType_t;

#endif /* AUDIO_TYPES_H */
