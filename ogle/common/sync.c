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

#include <stdio.h>

#include "sync.h"
#include "common.h"
#include "timemath.h"

clocktime_t get_time_base_offset(uint64_t PTS,
				 ctrl_time_t *ctrl_time,
				 int scr_nr)
{
  clocktime_t curtime, ptstime, predtime, offset;
  
  PTS_TO_CLOCKTIME(ptstime, PTS);
  
  clocktime_get(&curtime);
  timeadd(&predtime, &(ctrl_time[scr_nr].realtime_offset), &ptstime);

  timesub(&offset, &predtime, &curtime);
  
  /*
  fprintf(stderr, "\nac3: get offset: %ld.%09ld\n", 
	  TIME_S(offset), 
	  TIME_SS(offset));
  */
  return offset;
}


int set_time_base(uint64_t PTS,
		  ctrl_time_t *ctrl_time,
		  int scr_nr,
		  clocktime_t offset)
{
  clocktime_t curtime, ptstime, modtime;
  
  PTS_TO_CLOCKTIME(ptstime, PTS)

  clocktime_get(&curtime);
  timeadd(&modtime, &curtime, &offset);
  
  if(ctrl_time[scr_nr].offset_valid == OFFSET_VALID) { // DEBUG
    clocktime_t adjtime;
    timesub(&adjtime, &modtime, &ptstime);
    timesub(&adjtime, &adjtime, &(ctrl_time[scr_nr].realtime_offset));
    fprintf(stderr, "set_time_base: offset adjusted with: %ld.%09ld\n",
	    TIME_S(adjtime),
	    TIME_SS(adjtime));
  }

  timesub(&(ctrl_time[scr_nr].realtime_offset), &modtime, &ptstime);
  ctrl_time[scr_nr].offset_valid = OFFSET_VALID;

  /*
  fprintf(stderr, "set_time_base: setting offset[%d]: rtoff: %ld.%09ld, curtime: %ld.%09ld, PTS %lld\n",
	  scr_nr,
	  TIME_S(ctrl_time[scr_nr].realtime_offset),
	  TIME_SS(ctrl_time[scr_nr].realtime_offset),
	  TIME_S(curtime),
	  TIME_SS(curtime),  
	  PTS);
  */
  return 0;
}

void calc_realtime_from_scrtime(clocktime_t *rt, clocktime_t *scrtime,
				sync_point_t *sp)
{

  double inv_speed = 1.0/sp->speed;
  timesub(rt, scrtime, &sp->scr);
  timemul(rt, rt, inv_speed);
  timeadd(rt, rt, &sp->rt);

}

void set_sync_point(sync_point_t *sp, clocktime_t *rt,
		    clocktime_t *scrtime, double speed)
{
  sp->rt = *rt;
  sp->scr = *scrtime;
  sp->speed = speed;
}

void set_speed(sync_point_t *sp, double speed)
{
  clocktime_t cur_scrtime;
  clocktime_t cur_rt;
  
  clocktime_get(&cur_rt);
  timesub(&cur_scrtime, &cur_rt, &sp->rt); 
  timemul(&cur_scrtime, &cur_scrtime, sp->speed);
  timeadd(&cur_scrtime, &cur_scrtime, &sp->scr);

  
  sp->speed = speed;
  sp->rt = cur_rt;
  sp->scr = cur_scrtime;
  
}
