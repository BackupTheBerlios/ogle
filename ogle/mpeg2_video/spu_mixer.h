#ifndef SPU_MIXER_H
#define SPU_MIXER_H


void mix_subpicture_rgb(char *data, int width, int height);
//ugly hack
int mix_subpicture_yuv(yuv_image_t *img, yuv_image_t *reserv);

int init_spu(void);

void flush_subpicture(int scr_nr);

#endif
