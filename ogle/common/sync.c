
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
  timesub(&(ctrl_time[scr_nr].realtime_offset), &modtime, &ptstime);
  ctrl_time[scr_nr].offset_valid = OFFSET_VALID;
  /*
  fprintf(stderr, "ac3: setting offset[%d]: %ld.%09ld\n",
	  scr_nr,
	  TIME_S(ctrl_time[scr_nr].realtime_offset),
	  TIME_SS(ctrl_time[scr_nr].realtime_offset));
  */
  return 0;
}
