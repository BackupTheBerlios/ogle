#include <X11/Xlib.h>

#ifdef HAVE_XF86VIDMODE
#include <X11/extensions/xf86vmode.h>
#endif

#include "display.h"


typedef struct {
  int horizontal_pixels;
  int vertical_pixels;
} resolution_t;

typedef struct {
  int width;
  int height;
} geometry_t;

typedef struct {
  resolution_t src_res;
  resolution_t disp_res;
} fullscreen_resolution_t;

typedef struct _dpy_info_t{
  resolution_t resolution;
  geometry_t geometry;
  resolution_t user_resolution;
  geometry_t user_geometry;

  int sar_frac_n;  /* Display (i.e. monitor) sample aspect ratio. */
  int sar_frac_d;  /* calculated in update_geometry and update_resolution */

  DpyInfoOrigin_t geometry_origin;
  DpyInfoOrigin_t resolution_origin;
  resolution_t normal_dpy_res;
  fullscreen_resolution_t *fullscreen_dpy_res;
} dpy_info_t;

dpy_info_t dpyinfo;


int DpyInfoGetResolution(Display *dpy, int screen_nr,
			 int *horizontal_pixels,
			 int *vertical_pixels)
{
  *horizontal_pixels = dpyinfo.resolution.horizontal_pixels;
  *vertical_pixels = dpyinfo.resolution.vertical_pixels;

  return 1;
}


int DpyInfoGetGeometry(Display *dpy, int screen_nr,
		       int *width,
		       int *height)
{
  *width = dpyinfo.geometry.width;
  *height = dpyinfo.geometry.height;

  return 1;
}


int DpyInfoGetSAR(Display *dpy, int screen_nr,
		  int *sar_frac_n, int *sar_frac_d)
{
  if(dpyinfo.sar_frac_n > 0 && dpyinfo.sar_frac_d > 0) {
    *sar_frac_n = dpyinfo.sar_frac_n;
    *sar_frac_d = dpyinfo.sar_frac_d;
    return 1;
  }
  
  return 0;
}


int DpyInfoSetUserResolution(Display *dpy, int screen_nr,
			     int horizontal_pixels,
			     int vertical_pixels)
{
  if(horizontal_pixels > 0 && vertical_pixels > 0) {
    dpyinfo.user_resolution.horizontal_pixels = horizontal_pixels;
    dpyinfo.user_resolution.vertical_pixels = vertical_pixels;
    return 1;
  }

  return 0;
}


int DpyInfoSetUserGeometry(Display *dpy, int screen_nr,
			   int width,
			   int height)
{
  if(width > 0 && height > 0) {
    dpyinfo.user_geometry.width = width;
    dpyinfo.user_geometry.height = height;
    return 1;
  }
  
  return 0;
}


static void update_sar(dpy_info_t *info) {
  info->sar_frac_n = info->geometry.height*info->resolution.horizontal_pixels;
  info->sar_frac_d = info->geometry.width*info->resolution.vertical_pixels;
}

static int update_resolution_user(dpy_info_t *info,
				  Display *dpy, int screen_nr)
{
  if(info->user_resolution.horizontal_pixels > 0 &&
     info->user_resolution.vertical_pixels > 0) {
    
    info->resolution.horizontal_pixels =
      info->user_resolution.horizontal_pixels;
    info->resolution.vertical_pixels =
      info->user_resolution.vertical_pixels;
    
    update_sar(info);
    
    return 1;
  }
  return 0;
}
 

static int update_resolution_x11(dpy_info_t *info, Display *dpy, int screen_nr)
{

  info->resolution.horizontal_pixels = DisplayWidth(dpy, screen_nr);
  info->resolution.vertical_pixels = DisplayHeight(dpy, screen_nr);
  
  update_sar(info);
  
  return 1;
}
  

#ifdef HAVE_XF86VIDMODE
    
static int update_resolution_xf86vidmode(dpy_info_t *info, Display *dpy,
					 int screen_nr)
{
  int dotclk;

  XF86VidModeModeLine modeline;

  if(XF86VidModeGetModeLine(dpy, screen_nr, &dotclk, &modeline)) {
    if(modeline.privsize != 0) {
      XFree(modeline.private);
    }
    info->resolution.horizontal_pixels = modeline.hdisplay;
    info->resolution.vertical_pixels = modeline.vdisplay;

    update_sar(info);
    
    return 1;
  } 
  
  return 0;
}

#endif


int DpyInfoUpdateResolution(Display *dpy, int screen_nr)
{
  int ret;
  
  switch(dpyinfo.resolution_origin) {
  case DpyInfoOriginX11:
    ret = update_resolution_x11(&dpyinfo, dpy, screen_nr);
    break;
#ifdef HAVE_XF86VIDMODE
  case DpyInfoOriginXF86VidMode:
    ret = update_resolution_xf86vidmode(&dpyinfo, dpy, screen_nr);
    break;
#endif
  case DpyInfoOriginUser:
    ret = update_resolution_user(&dpyinfo, dpy, screen_nr);
    break;
  default:
    return 0;
    break;
  }

  return ret;
}


static int update_geometry_user(dpy_info_t *info, Display *dpy, int screen_nr)
{
  if(info->user_geometry.width > 0 && 
     info->user_geometry.height > 0) {
    info->geometry.width = info->user_geometry.width;
    info->geometry.height = info->user_geometry.height;

    update_sar(info);
    
    return 1;
  }
  
  return 0;
}
 

static int update_geometry_x11(dpy_info_t *info, Display *dpy, int screen_nr)
{

  info->geometry.width = DisplayWidthMM(dpy, screen_nr);
  info->geometry.height = DisplayHeightMM(dpy, screen_nr);
  
  update_sar(info);
  
  return 1;
}


int DpyInfoUpdateGeometry(Display *dpy, int screen_nr)
{
  int ret;
  
  switch(dpyinfo.geometry_origin) {
  case DpyInfoOriginX11:
    ret = update_geometry_x11(&dpyinfo, dpy, screen_nr);
    break;
  case DpyInfoOriginUser:
    ret = update_geometry_user(&dpyinfo, dpy, screen_nr);
    break;
  default:
    return 0;
    break;
  }

  return ret;
}



int DpyInfoInit(Display *dpy, int screen_nr)
{
  
  dpyinfo.resolution.horizontal_pixels = 0;
  dpyinfo.resolution.vertical_pixels = 0;
  dpyinfo.geometry.width = 0;
  dpyinfo.geometry.height = 0;

  dpyinfo.user_resolution.horizontal_pixels = 0;
  dpyinfo.user_resolution.vertical_pixels = 0;
  dpyinfo.user_geometry.width = 0;
  dpyinfo.user_geometry.height = 0;

  dpyinfo.resolution_origin = DpyInfoOriginX11;
  dpyinfo.geometry_origin = DpyInfoOriginX11;

  return 1;
}



DpyInfoOrigin_t DpyInfoSetUpdateGeometry(Display *dpy, int screen_nr,
					   DpyInfoOrigin_t origin)
{
  switch(origin) {
  case DpyInfoOriginUser:
    if(update_geometry_user(&dpyinfo, dpy, screen_nr)) {
      dpyinfo.geometry_origin = DpyInfoOriginUser;
      return dpyinfo.geometry_origin;
    }
  case DpyInfoOriginX11:
  default:
    if(update_geometry_x11(&dpyinfo, dpy, screen_nr)) {
      dpyinfo.geometry_origin = DpyInfoOriginX11;
      return dpyinfo.geometry_origin;
    }
  }
  
  return 0;
  
}


DpyInfoOrigin_t DpyInfoSetUpdateResolution(Display *dpy, int screen_nr,
					     DpyInfoOrigin_t origin)
{
  switch(origin) {
  case DpyInfoOriginUser:
    if(update_resolution_user(&dpyinfo, dpy, screen_nr)) {
      dpyinfo.resolution_origin = DpyInfoOriginUser;
      return dpyinfo.resolution_origin;
    }
#ifdef HAVE_XF86VIDMODE
  case DpyInfoOriginXF86VidMode:
    if(update_resolution_xf86vidmode(&dpyinfo, dpy, screen_nr)) {
      dpyinfo.resolution_origin = DpyInfoOriginXF86VidMode;
      return dpyinfo.resolution_origin;
    }
#endif
  case DpyInfoOriginX11:
  default:
    if(update_resolution_x11(&dpyinfo, dpy, screen_nr)) {
      dpyinfo.resolution_origin = DpyInfoOriginX11;
      return dpyinfo.resolution_origin;
    }
  }
  
  return 0;
  
}

