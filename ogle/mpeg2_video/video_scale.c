#include <inttypes.h>
#include <stdio.h>

inline 
uint32_t nearest(uint32_t *source, uint32_t src_stride, float x, float y) {
  unsigned int j,k;
  j = x + 0.5;
  k = y + 0.5;
  return source[j + src_stride/4 * k];
}

inline
uint32_t bilinear(uint32_t *source, uint32_t src_stride, float x, float y) {
  unsigned int j, k;
  float a, b;
  float dest_r, dest_g, dest_b;
  unsigned int pixel_r, pixel_g, pixel_b;

  j = x;
  k = y;
  a = x - j;
  b = y - k;
  dest_r = (1-a)*(1-b)*(source[j + src_stride/4 * k]&0xff)
    + a*(1-b)*(source[(j+1) + src_stride/4 * k]&0xff)
    + b*(1-a)*(source[j + src_stride/4 * (k+1)]&0xff)
    + a*b*(source[(j+1) + src_stride/4 * (k+1)]&0xff);
  dest_g = (1-a)*(1-b)*((source[j + src_stride/4 * k]>>8)&0xff)
    + a*(1-b)*((source[(j+1) + src_stride/4 * k]>>8)&0xff)
    + b*(1-a)*((source[j + src_stride/4 * (k+1)]>>8)&0xff)
    + a*b*((source[(j+1) + src_stride/4 * (k+1)]>>8)&0xff);
  dest_b = (1-a)*(1-b)*((source[j + src_stride/4 * k]>>16)&0xff)
    + a*(1-b)*((source[(j+1) + src_stride/4 * k]>>16)&0xff)
    + b*(1-a)*((source[j + src_stride/4 * (k+1)]>>16)&0xff)
    + a*b*((source[(j+1) + src_stride/4 * (k+1)]>>16)&0xff);
  pixel_r = dest_r;
  pixel_g = dest_g;
  pixel_b = dest_b;

  return (pixel_b<<16) | (pixel_g<<8) | pixel_r;
}


void mylib_VideoScale_YUV420_to_YUYV422(uint8_t *y_src,
					uint8_t *u_src,
					uint8_t *v_src,
					const uint32_t src_width,
					const uint32_t src_height,
					const uint32_t src_stride,
					uint32_t *yuyv_dst,
					const uint32_t dst_width,
					const uint32_t dst_height,
					const uint32_t dst_stride) {
  unsigned int x, y;
  double xr, yr;
  double dx, dy;
  double dux, duy;
  uint8_t *src_pixel;
  uint32_t dst_pixel;

  
  dx = (double)src_width / (double)dst_width;
  dy = (double)src_height / (double)dst_height;

  dux = (double)(src_width/2) / (double)(dst_width/2);
  duy = (double)(src_height/2) / (double)(dst_height/2);
  
  for(y = 0;  y < dst_height; y++) {
    int y_int = (int)((double)(y)*dy);
    double y_frac = (double)(y)*dy-(double)y_int;
    double y_frac_inv = 1.0-y_frac;

    int uy_int = (int)((double)(y/2)*duy);
    double uy_frac = (double)(y/2)*duy-(double)uy_int;
    double uy_frac_inv = 1.0-uy_frac;
    
    for(x = 0; x < dst_width; x++) {
      double a,b,c,d;
      uint8_t e;
      uint32_t dst_pixel = 0;
      double p,q;
      int x_int = (int)((double)(x)*dx);
      double x_frac = (double)(x)*dx-(double)(x_int);
      
      src_pixel = y_src +y_int*src_stride + x_int;
      a = (double)(*src_pixel);
      b = (double)(*(src_pixel+1));
      c = (double)(*(src_pixel+src_stride));
      d = (double)(*(src_pixel+src_stride+1));
      
      p = y_frac*c + y_frac_inv*a;
      q = y_frac*d + y_frac_inv*b;

      e = (uint8_t)(x_frac*q + (1.0-x_frac)*p);
      
      dst_pixel = e << 24;
      x++;
      x_int = (int)((double)(x)*dx);
      x_frac = (double)(x)*dx-(double)x_int;
      
      src_pixel = y_src+y_int*src_stride+x_int;
      
      a = (double)(*(src_pixel));
      b = (double)(*(src_pixel+1));
      c = (double)(*(src_pixel+src_stride));
      d = (double)(*(src_pixel+src_stride+1));
      
      p = y_frac*c + y_frac_inv*a;
      q = y_frac*d + y_frac_inv*b;
      
      e = (uint8_t)(x_frac*q + (1.0-x_frac)*p);
      
      dst_pixel = dst_pixel | (e << 8);
      yuyv_dst[y*dst_stride/4+x/2] = dst_pixel;
 
    }
    
    
    
  }
}


void mylib_VideoScaleABGR( uint8_t *dst, 
			   uint8_t *src,
			   const uint32_t dst_h_size,
			   const uint32_t dst_v_size,
			   const uint32_t src_h_size,
			   const uint32_t src_v_size,
			   const uint32_t dst_stride,
			   const uint32_t src_stride ) {
  unsigned int x, y;
  double xr, yr;
  double h_scale, v_scale;
  uint32_t *source = (uint32_t *)src;
  uint32_t *base;
  uint32_t *dest = (uint32_t *)dst; 
  uint32_t ncolor;
  
  h_scale = (double)src_h_size / (double)dst_h_size;
  v_scale = (double)src_v_size / (double)dst_v_size;

  for( y = 0;  y < dst_v_size; y++ ) {
    unsigned int k;
    yr = y * v_scale;
    k = yr;
    base = &source[k * src_stride/4]; 
    
    for( x = 0; x < dst_h_size; x++ ) {
      unsigned int j;
      xr = x * h_scale;
      j = xr;
      dest[x] = base[j];
    }
    dest += dst_stride/4;
  }
}








/*
FUNCTION Bicubic (x,y:real) : byte;
var j,k:integer;
var a,b:real;
var dest:real;
var t1,t2,t3,t4:real;
var color:integer;
begin
  j:=round (x);
  k:=round (y);
  a:=x-j;
  b:=y-k;
  t1:=-a*(1-a)*(1-a)*RC(j-1,k-1)+
         (1-2*a*a+a*a*a)*RC(j,k-1)+
       a*(1+a-a*a)*RC(j+1,k-1)-
       a*a*(1-a)*RC(j+2,k-1);
  t2:=-a*(1-a)*(1-a)*RC(j-1,k)+
         (1-2*a*a+a*a*a)*RC(j,k)+
       a*(1+a-a*a)*RC(j+1,k)-
       a*a*(1-a)*RC(j+2,k);
  t3:=-a*(1-a)*(1-a)*RC(j-1,k+1)+
         (1-2*a*a+a*a*a)*RC(j,k+1)+
       a*(1+a-a*a)*RC(j+1,k+1)-
       a*a*(1-a)*RC(j+2,k+1);
  t4:=-a*(1-a)*(1-a)*RC(j-1,k+2)+
         (1-2*a*a+a*a*a)*RC(j,k+2)+
       a*(1+a-a*a)*RC(j+1,k+2)-
       a*a*(1-a)*RC(j+2,k+2);

  dest:= -b*(1-b)*(1-b)*t1+
            (1-2*b*b+b*b*b)*t2+
          b*(1+b-b*b)*t3+
          b*b*(b-1)*t4;
  color:=round(dest);
  if color<0 then color:=abs(color);  // cannot have negative values !
  if (color>gifrec.colors) then color:=gifrec.colors; //
  Bicubic:=color;
end;
*/
