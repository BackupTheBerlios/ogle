#ifndef SPU_MIXER_H
#define SPU_MIXER_H


//ugly hack
#ifdef HAVE_XV
int mix_subpicture(yuv_image_t *img, yuv_image_t *reserv, int width,
		   int height, int picformat);
#else
void mix_subpicture(char *data, int width, int height, int picformat);
#endif

int init_spu(void);

#endif
