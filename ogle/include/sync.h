#ifndef SYNC_H_INCLUDED
#define SYNC_H_INCLUDED

/* Ogle - A video player
 * Copyright (C) 2000, 2001 Björn Englund, Håkan Hjort
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

#include "timemath.h"
#include "common.h"

clocktime_t get_time_base_offset(uint64_t PTS,
				 ctrl_time_t *ctrl_time,
				 int scr_nr);

int set_time_base(uint64_t PTS,
		  ctrl_time_t *ctrl_time,
		  int scr_nr,
		  clocktime_t offset);

#endif /* SYNC_H_INCLUDED */
