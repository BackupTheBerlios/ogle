#include <inttypes.h>

/* Packettypes. */

enum {
      PACK_TYPE_OFF_LEN, 
      PACK_TYPE_LOAD_FILE
     };

/* "There's len bytes of data for you in your file at this offset." */
struct off_len_packet {
	               uint32_t cmd;
                       uint32_t off;
                       uint32_t len;
                      };

/* "mmap this file now; off_len_packets from now on will be relative
 * this file." */
struct load_file_packet {
                         uint32_t cmd;
                         uint32_t len;
                         char     filename[200];   // HACK ALERT!
                        };

