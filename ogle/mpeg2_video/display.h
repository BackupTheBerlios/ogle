#ifndef OGLE_DISPLAY_H
#define OGLE_DISPLAY_H

typedef enum {
  DpyInfoOriginX11 = 1,
  DpyInfoOriginXF86VidMode,
  DpyInfoOriginUser
} DpyInfoOrigin_t;



int DpyInfoInit(Display *dpy, int screen_nr);


int DpyInfoSetUserResolution(Display *dpy, int screen_nr,
			     int horizontal_pixels,
			     int vertical_pixels);

int DpyInfoSetUserGeometry(Display *dpy, int screen_nr,
			   int width,
			   int height);


DpyInfoOrigin_t DpyInfoSetUpdateGeometry(Display *dpy, int screen_nr,
					   DpyInfoOrigin_t origin);

DpyInfoOrigin_t DpyInfoSetUpdateResolution(Display *dpy, int screen_nr,
					     DpyInfoOrigin_t origin);


int DpyInfoUpdateResolution(Display *dpy, int screen_nr);

int DpyInfoUpdateGeometry(Display *dpy, int screen_nr);


int DpyInfoGetResolution(Display *dpy, int screen_nr,
			 int *horizontal_pixels,
			 int *vertical_pixels);

int DpyInfoGetGeometry(Display *dpy, int screen_nr,
		       int *width,
		       int *height);

int DpyInfoGetSAR(Display *dpy, int screen_nr,
		  int *sar_frac_n, int *sar_frac_d);

#endif /* OGLE_DISPLAY_H */
