/* 
 * Matrox G400 Video output
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>

#include "../include/common.h"
#include "video_types.h"
#include "yuv2rgb.h"
#include "mga_vid.h"


uint_8 *vid_data;
mga_vid_config_t mga_vid_config;
int show_stat = 0;

/* Ugly */
static int horizontal_size = 720;
static int vertical_size = 480;

void
display_init ()
{
	int f;
	uint_32 frame_size;

	f = open ("/dev/mga_vid", O_RDWR);

	printf ("Size: %d %d\n", horizontal_size, vertical_size);
	if(f == -1)
	{
		fprintf(stderr,"Couldn't open /dev/mga_vid\n");
		exit(1);
	}
	
	mga_vid_config.src_width = horizontal_size;
	mga_vid_config.src_height = vertical_size;
	mga_vid_config.dest_width = horizontal_size;
	mga_vid_config.dest_height = vertical_size;
	mga_vid_config.x_org= 10;
	mga_vid_config.y_org= 10;

	if (ioctl(f,MGA_VID_CONFIG,&mga_vid_config))
	{
		perror("Error in mga_vid_config ioctl");
	}
	ioctl(f,MGA_VID_ON,0);

	frame_size = ((horizontal_size + 31) & ~31) * vertical_size + (((horizontal_size + 31) & ~31) * vertical_size) / 2;
	vid_data = (char*)mmap(0,frame_size,PROT_WRITE,MAP_SHARED,f,0);

	//clear the buffer
	memset(vid_data,0x80,frame_size);

	printf ("Display init\n");
}

void
user_control (macroblock_t *cur_mbs,
	      macroblock_t *ref1_mbs,
	      macroblock_t *ref2_mbs)
{
	printf ("user_control\n");
}

void 
display_exit(void) 
{

	printf ("Display_exit\n");
}

void
write_frame_g400(uint_8 *y,uint_8 *cr, uint_8 *cb)
{
	uint_8 *dest;
	uint_32 bespitch,h;

	dest = vid_data;
	bespitch = (mga_vid_config.src_width + 31) & ~31;

	for(h=0; h < mga_vid_config.src_height; h++) 
	{
		memcpy(dest, y, mga_vid_config.src_width);
		y += mga_vid_config.src_width;
		dest += bespitch;
	}

	for(h=0; h < mga_vid_config.src_height/2; h++) 
	{
		memcpy(dest, cb, mga_vid_config.src_width/2);
		cb += mga_vid_config.src_width/2;
		dest += bespitch/2;
	}

	for(h=0; h < mga_vid_config.src_height/2; h++) 
	{
		memcpy(dest, cr, mga_vid_config.src_width/2);
		cr += mga_vid_config.src_width/2;
		dest += bespitch/2;
	}
}

void frame_done(yuv_image_t *current_image, macroblock_t *cur_mbs,
		yuv_image_t *ref_image1, macroblock_t *ref1_mbs,
		yuv_image_t *ref_image2, macroblock_t *ref2_mbs, 
		uint8_t picture_coding_type)
{
	write_frame_g400 (current_image->y,
			  current_image->v,
			  current_image->u);
}







