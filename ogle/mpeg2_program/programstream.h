#include <inttypes.h>

//#define LITTLE_ENDIAN
#define BIG_ENDIAN 

#ifdef BIG_ENDIAN
#define MPEG2_PS_PACK_START_CODE 0x000001BA
#define MPEG2_PS_PROGRAM_END_CODE 0x000001B9
#define MPEG2_PS_SYSTEM_HEADER_START_CODE 0x000001BB
#define MPEG2_PES_PACKET_START_CODE_PREFIX 0x000001

#else

#define MPEG2_PS_PACK_START_CODE 0xBA010000
#define MPEG2_PS_PROGRAM_END_CODE 0xB9010000
#define MPEG2_PS_SYSTEM_HEADER_START_CODE 0xBB010000
#define MPEG2_PES_PACKET_START_CODE_PREFIX 0x010000

#endif

#define MPEG2_PS_PACK_START_CODE_SIZE 32
#define MPEG2_PS_PROGRAM_END_CODE_SIZE 32
#define MPEG2_PS_SYSTEM_HEADER_START_CODE_SIZE 32
#define MPEG2_PES_PACKET_START_CODE_PREFIX_SIZE 24


#define MPEG2_PRIVATE_STREAM_1 0xBD
#define MPEG2_PADDING_STREAM 0xBE
#define MPEG2_PRIVATE_STREAM_2 0xBF

enum {
      PACK_TYPE_OFF_LEN, 
      PACK_TYPE_LOAD_FILE
     };


struct off_len_packet {
	               uint8_t  cmd;
                       uint32_t off;
                       uint32_t len;
                      };

struct load_file_packet {
                         uint8_t  cmd;
                         uint16_t len;
                         char     payload[200];   // HACK ALERT!
                        };

