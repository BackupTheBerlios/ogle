#ifndef SYNC_H
#define SYNC_H

#include "timemath.h"
#include "common.h"

clocktime_t get_time_base_offset(uint64_t PTS,
				 ctrl_time_t *ctrl_time,
				 int scr_nr);

int set_time_base(uint64_t PTS,
		  ctrl_time_t *ctrl_time,
		  int scr_nr,
		  clocktime_t offset);
#endif /* SYNC_H */
