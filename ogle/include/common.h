#include <inttypes.h>

/* Packettypes. */

enum {
      PACK_TYPE_OFF_LEN, 
      PACK_TYPE_LOAD_FILE
     };

/* "There's len bytes of data for you in your file at this offset." */
struct off_len_packet {
//	               uint32_t type;
                       uint32_t offset;
                       uint32_t length;
                      };

/* "mmap this file now; off_len_packets from now on will be relative
 * this file." */
struct load_file_packet {
//                        uint32_t type;
                         uint32_t length;
                         char     filename[200];   // HACK ALERT!
                        };


/* Imgaretype. */

typedef struct {
  uint8_t *y; //[480][720];  //y-component image
  uint8_t *u; //[480/2][720/2]; //u-component
  uint8_t *v; //[480/2][720/2]; //v-component
  
  //timecode_t time;

  uint8_t lock;
} yuv_image_t;

