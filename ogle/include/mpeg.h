#ifndef MPEG_H
#define MPEG_H

//#define LITTLE_ENDIAN
//#define BIG_ENDIAN 

/* program stream 
 * ==============
 */

#define MPEG2_PS_PACK_START_CODE 0x000001BA
#define MPEG2_PS_PROGRAM_END_CODE 0x000001B9
#define MPEG2_PS_SYSTEM_HEADER_START_CODE 0x000001BB
#define MPEG2_PES_PACKET_START_CODE_PREFIX 0x000001
/*
#else

#define MPEG2_PS_PACK_START_CODE 0xBA010000
#define MPEG2_PS_PROGRAM_END_CODE 0xB9010000
#define MPEG2_PS_SYSTEM_HEADER_START_CODE 0xBB010000
#define MPEG2_PES_PACKET_START_CODE_PREFIX 0x010000

#endif
*/
#define MPEG2_PS_PACK_START_CODE_SIZE 32
#define MPEG2_PS_PROGRAM_END_CODE_SIZE 32
#define MPEG2_PS_SYSTEM_HEADER_START_CODE_SIZE 32
#define MPEG2_PES_PACKET_START_CODE_PREFIX_SIZE 24


#define MPEG2_PRIVATE_STREAM_1 0xBD
#define MPEG2_PADDING_STREAM 0xBE
#define MPEG2_PRIVATE_STREAM_2 0xBF



/* video stream
 * ============
 */

/*
#define MPEG2_VS_START_CODE_PREFIX 0x000001

#define MPEG2_VS_PICTURE_START_CODE   0x00
#define MPEG2_VS_USER_DATA_START_CODE 0xB2
#define MPEG2_VS_SEQUENCE_HEADER_CODE 0xB2
#define MPEG2_VS_SEQUENCE_ERROR_CODE  0xB2
#define MPEG2_VS_EXTENSION_START_CODE 0xB2
#define MPEG2_VS_SEQUENCE_END_CODE    0xB2
#define MPEG2_VS_GROUP_START_CODE     0xB2
*/


/* Table 6-1  Start code values */
#define MPEG2_VS_PICTURE_START_CODE       0x00000100
#define MPEG2_VS_SLICE_START_CODE_LOWEST  0x00000101
#define MPEG2_VS_SLICE_START_CODE_HIGHEST 0x000001AF
#define MPEG2_VS_USER_DATA_START_CODE     0x000001B2
#define MPEG2_VS_SEQUENCE_HEADER_CODE     0x000001B3
#define MPEG2_VS_SEQUENCE_ERROR_CODE      0x000001B4
#define MPEG2_VS_EXTENSION_START_CODE     0x000001B5
#define MPEG2_VS_SEQUENCE_END_CODE        0x000001B7
#define MPEG2_VS_GROUP_START_CODE         0x000001B8

/* Table 6-2. extension_start_code_identifier codes. */
#define MPEG2_VS_SEQUENCE_EXTENSION_ID                  0x1
#define MPEG2_VS_SEQUENCE_DISPLAY_EXTENSION_ID          0x2
#define MPEG2_VS_QUANT_MATRIX_EXTENSION_ID              0x3
#define MPEG2_VS_SEQUENCE_SCALABLE_EXTENSION_ID         0x5
#define MPEG2_VS_PICTURE_DISPLAY_EXTENSION_ID           0x7
#define MPEG2_VS_PICTURE_CODING_EXTENSION_ID            0x8
#define MPEG2_VS_PICTURE_SPATIAL_SCALABLE_EXTENSION_ID  0x9
#define MPEG2_VS_PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID 0xA

#endif /* MPEG_H */
