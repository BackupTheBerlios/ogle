/*
#define MPEG2_VS_START_CODE_PREFIX 0x000001

#define MPEG2_VS_PICTURE_START_CODE 0x00
#define MPEG2_VS_USER_DATA_START_CODE 0xB2
#define MPEG2_VS_SEQUENCE_HEADER_CODE 0xB2
#define MPEG2_VS_SEQUENCE_ERROR_CODE 0xB2
#define MPEG2_VS_EXTENSION_START_CODE 0xB2
#define MPEG2_VS_SEQUENCE_END_CODE 0xB2
#define MPEG2_VS_GROUP_START_CODE 0xB2

*/


#define MPEG2_VS_PICTURE_START_CODE     0x00000100
#define MPEG2_VS_SLICE_START_CODE_LOWEST 0x00000101
#define MPEG2_VS_SLICE_START_CODE_HIGHEST 0x000001AF
#define MPEG2_VS_USER_DATA_START_CODE   0x000001B2
#define MPEG2_VS_SEQUENCE_HEADER_CODE   0x000001B3
#define MPEG2_VS_SEQUENCE_ERROR_CODE    0x000001B4
#define MPEG2_VS_EXTENSION_START_CODE   0x000001B5
#define MPEG2_VS_SEQUENCE_END_CODE      0x000001B7
#define MPEG2_VS_GROUP_START_CODE       0x000001B8

typedef struct { 
	int numberofbits;
	int vlc;
	int value;
} vlc_table_t;

typedef struct { 
	int numberofbits;
	int vlc;
	int run;
	int level;
} vlc_rl_table;


typedef struct {
  uint8_t run;
  int16_t level;
} runlevel_t;

#define VLC_FAIL 0

// used by b14
#define DCT_DC_FIRST          0
#define DCT_DC_SUBSEQUENT     1

#define VLC_ESCAPE         255
#define VLC_END_OF_BLOCK   254 


vlc_table_t table_b12[] = { 
	{    2,  0x000,     1 },
	{    2,  0x001,     2 },
	{    3,  0x004,     0 },
	{    3,  0x005,     3 },
	{    3,  0x006,     4 },
	{    4,  0x00E,     5 },
	{    5,  0x01E,     6 },
	{    6,  0x03E,     7 },
	{    7,  0x07E,     8 },
	{    8,  0x0FE,     9 },
	{    9,  0x0FE,    10 },
	{    9,  0x1FF,    11 },
	{ VLC_FAIL, VLC_FAIL, VLC_FAIL } };

vlc_table_t table_b13[] = {
	{    2,  0x000,     0 },
	{    2,  0x001,     1 },
	{    2,  0x002,     2 },
	{    3,  0x006,     3 },
	{    4,  0x00E,     4 },
	{    5,  0x01E,     5 },
	{    6,  0x03E,     6 },
	{    7,  0x07E,     7 },
	{    8,  0x0FE,     8 },
	{    9,  0x1FE,     9 },
	{   10,  0x3FE,    10 },
	{   10,  0x3FF,    11 },
	{ VLC_FAIL, VLC_FAIL, VLC_FAIL } };



vlc_rl_table table_b14[] = {
  {	    2,	       0x2,VLC_END_OF_BLOCK, VLC_END_OF_BLOCK},
  {	    2,	       0x3,         0,	    1	},// NOTE 2, 3
  {	    3,	       0x3,	    1,	    1	},
  {	    4,	       0x4,	    0,	    2	},
  {	    4,	       0x5,	    2,	    1	},
  {	    5,	       0x5,	    0,	    3	},
  {	    5,	       0x6,	    4,	    1	},
  {	    5,	       0x7,	    3,	    1	},
  {	    6,	       0x4,	    7,	    1	},
  {	    6,	       0x5,	    6,	    1	},
  {	    6,	       0x6,	    1,	    2	},
  {	    6,	       0x7,	    5,	    1	},
  {	    6,	      0x01,VLC_ESCAPE,VLC_ESCAPE},
  {	    7,	       0x4,	    2,	    2	},
  {	    7,	       0x5,	    9,	    1	},
  {	    7,	       0x6,	    0,	    4	},
  {	    7,	       0x7,	    8,	    1	},
  {	    8,	      0x20,	   13,	    1	},
  {	    8,	      0x21,	    0,	    6	},
  {	    8,	      0x22,	   12,	    1	},
  {	    8,	      0x23,	   11,	    1	},
  {	    8,	      0x24,	    3,	    2	},
  {	    8,	      0x25,	    1,	    3	},
  {	    8,	      0x26,	    0,	    5	},
  {	    8,	      0x27,	   10,	    1	},
  {	   10,	       0x8,	   16,	    1	},
  {	   10,	       0x9,	    5,	    2	},
  {	   10,	       0xA,	    0,	    7	},
  {	   10,	       0xB,	    2,	    3	},
  {	   10,	       0xC,	    1,	    4	},
  {	   10,	       0xD,	   15,	    1	},
  {	   10,	       0xE,	   14,	    1	},
  {	   10,	       0xF,	    4,	    2	},
  {	   12,	      0x10,	    0,	   11	},
  {	   12,	      0x11,	    8,	    2	},
  {	   12,	      0x12,	    4,	    3	},
  {	   12,	      0x13,	    0,	   10	},
  {	   12,	      0x14,	    2,	    4	},
  {	   12,	      0x15,	    7,	    2	},
  {	   12,	      0x16,	   21,	    1	},
  {	   12,	      0x17,	   20,	    1	},
  {	   12,	      0x18,	    0,	    9	},
  {	   12,	      0x19,	   19,	    1	},
  {	   12,	      0x1A,	   18,	    1	},
  {	   12,	      0x1B,	    1,	    5	},
  {	   12,	      0x1C,	    3,	    3	},
  {	   12,	      0x1D,	    0,	    8	},
  {	   12,	      0x1E,	    6,	    2	},
  {	   12,	      0x1F,	   17,	    1	},
  {	   13,	      0x10,	   10,	    2	},
  {	   13,	      0x11,	    9,	    2	},
  {	   13,	      0x12,	    5,	    3	},
  {	   13,	      0x13,	    3,	    4	},
  {	   13,	      0x14,	    2,	    5	},
  {	   13,	      0x15,	    1,	    7	},
  {	   13,	      0x16,	    1,	    6	},
  {	   13,	      0x17,	    0,	   15	},
  {	   13,	      0x18,	    0,	   14	},
  {	   13,	      0x19,	    0,	   13	},
  {	   13,	      0x1A,	    0,	   12	},
  {	   13,	      0x1B,	   26,	    1	},
  {	   13,	      0x1C,	   25,	    1	},
  {	   13,	      0x1D,	   24,	    1	},
  {	   13,	      0x1E,	   23,	    1	},
  {	   13,	      0x1F,	   22,	    1	},
  {	   14,	      0x10,	    0,	   31	},
  {	   14,	      0x11,	    0,	   30	},
  {	   14,	      0x12,	    0,	   29	},
  {	   14,	      0x13,	    0,	   28	},
  {	   14,	      0x14,	    0,	   27	},
  {	   14,	      0x15,	    0,	   26	},
  {	   14,	      0x16,	    0,	   25	},
  {	   14,	      0x17,	    0,	   24	},
  {	   14,	      0x18,	    0,	   23	},
  {	   14,	      0x19,	    0,	   22	},
  {	   14,	      0x1A,	    0,	   21	},
  {	   14,	      0x1B,	    0,	   20	},
  {	   14,	      0x1C,	    0,	   19	},
  {	   14,	      0x1D,	    0,	   18	},
  {	   14,	      0x1E,	    0,	   17	},
  {	   14,	      0x1F,	    0,	   16	},
  {	   15,	      0x10,	    0,	   40	},
  {	   15,	      0x11,	    0,	   39	},
  {	   15,	      0x12,	    0,	   38	},
  {	   15,	      0x13,	    0,	   37	},
  {	   15,	      0x14,	    0,	   36	},
  {	   15,	      0x15,	    0,	   35	},
  {	   15,	      0x16,	    0,	   34	},
  {	   15,	      0x17,	    0,	   33	},
  {	   15,	      0x18,	    0,	   32	},
  {	   15,	      0x19,	    1,	   14	},
  {	   15,	      0x1A,	    1,	   13	},
  {	   15,	      0x1B,	    1,	   12	},
  {	   15,	      0x1C,	    1,	   11	},
  {	   15,	      0x1D,	    1,	   10	},
  {	   15,	      0x1E,	    1,	    9	},
  {	   15,	      0x1F,	    1,	    8	},
  {	   16,	      0x10,	    1,	   18	},
  {	   16,	      0x11,	    1,	   17	},
  {	   16,	      0x12,	    1,	   16	},
  {	   16,	      0x13,	    1,	   15	},
  {	   16,	      0x14,	    6,	    3	},
  {	   16,	      0x15,	   16,	    2	},
  {	   16,	      0x16,	   15,	    2	},
  {	   16,	      0x17,	   14,	    2	},
  {	   16,	      0x18,	   13,	    2	},
  {	   16,	      0x19,	   12,	    2	},
  {	   16,	      0x1A,	   11,	    2	},
  {	   16,	      0x1B,	   31,	    1	},
  {	   16,	      0x1C,	   30,	    1	},
  {	   16,	      0x1D,	   29,	    1	},
  {	   16,	      0x1E,	   28,	    1	},
  {	   16,	      0x1F,	   27,	    1	},
  {	   VLC_FAIL, VLC_FAIL, VLC_FAIL, VLC_FAIL }
};

vlc_rl_table table_b15[] = {
  {	    2,	       0x2,	    0,	    1	},
  {	    3,	       0x2,	    1,	    1	},
  {	    3,	       0x6,	    0,	    2	},
  {	    4,	       0x6, VLC_END_OF_BLOCK, VLC_END_OF_BLOCK},
  {	    4,	       0x7,	    0,	    3	},
  {	    5,	       0x5,	    2,	    1	},
  {	    5,	       0x6,	    1,	    2	},
  {	    5,	       0x7,	    3,	    1	},
  {	    5,	      0x1C,	    0,	    4	},
  {	    5,	      0x1D,	    0,	    5	},
  {	    6,	       0x4,	    0,	    7	},
  {	    6,	       0x5,	    0,	    6	},
  {	    6,	       0x6,	    4,	    1	},
  {	    6,	       0x7,	    5,	    1	},
  {	    6,	      0x01,VLC_ESCAPE,VLC_ESCAPE}, 
  {	    7,	       0x4,	    7,	    1	},
  {	    7,	       0x5,	    8,	    1	},
  {	    7,	       0x6,	    6,	    1	},
  {	    7,	       0x7,	    2,	    2	},
  {	    7,	      0x78,	    9,	    1	},
  {	    7,	      0x79,	    1,	    3	},
  {	    7,	      0x7A,	   10,	    1	},
  {	    7,	      0x7B,	    0,	    8	},
  {	    7,	      0x7C,	    0,	    9	},
  {	    8,	      0x20,	    1,	    5	},
  {	    8,	      0x21,	   11,	    1	},
  {	    8,	      0x22,	    0,	   11	},
  {	    8,	      0x23,	    0,	   10	},
  {	    8,	      0x24,	   13,	    1	},
  {	    8,	      0x25,	   12,	    1	},
  {	    8,	      0x26,	    3,	    2	},
  {	    8,	      0x27,	    1,	    4	},
  {	    8,	      0xFA,	    0,	   12	},
  {	    8,	      0xFB,	    0,	   13	},
  {	    8,	      0xFC,	    2,	    3	},
  {	    8,	      0xFD,	    4,	    2	},
  {	    8,	      0xFE,	    0,	   14	},
  {	    8,	      0xFF,	    0,	   15	},
  {	    9,	       0x4,	    5,	    2	},
  {	    9,	       0x5,	   14,	    1	},
  {	    9,	       0x7,	   15,	    1	},
  {	   10,	       0xC,	    2,	    4	},
  {	   10,	       0xD,	   16,	    1	},
  {	   12,	      0x11,	    8,	    2	},
  {	   12,	      0x12,	    4,	    3	},
  {	   12,	      0x15,	    7,	    2	},
  {	   12,	      0x16,	   21,	    1	},
  {	   12,	      0x17,	   20,	    1	},
  {	   12,	      0x19,	   19,	    1	},
  {	   12,	      0x1A,	   18,	    1	},
  {	   12,	      0x1C,	    3,	    3	},
  {	   12,	      0x1E,	    6,	    2	},
  {	   12,	      0x1F,	   17,	    1	},
  {	   13,	      0x10,	   10,	    2	},
  {	   13,	      0x11,	    9,	    2	},
  {	   13,	      0x12,	    5,	    3	},
  {	   13,	      0x13,	    3,	    4	},
  {	   13,	      0x14,	    2,	    5	},
  {	   13,	      0x15,	    1,	    7	},
  {	   13,	      0x16,	    1,	    6	},
  {	   13,	      0x1B,	   26,	    1	},
  {	   13,	      0x1C,	   25,	    1	},
  {	   13,	      0x1D,	   24,	    1	},
  {	   13,	      0x1E,	   23,	    1	},
  {	   13,	      0x1F,	   22,	    1	},
  {	   14,	      0x10,	    0,	   31	},
  {	   14,	      0x11,	    0,	   30	},
  {	   14,	      0x12,	    0,	   29	},
  {	   14,	      0x13,	    0,	   28	},
  {	   14,	      0x14,	    0,	   27	},
  {	   14,	      0x15,	    0,	   26	},
  {	   14,	      0x16,	    0,	   25	},
  {	   14,	      0x17,	    0,	   24	},
  {	   14,	      0x18,	    0,	   23	},
  {	   14,	      0x19,	    0,	   22	},
  {	   14,	      0x1A,	    0,	   21	},
  {	   14,	      0x1B,	    0,	   20	},
  {	   14,	      0x1C,	    0,	   19	},
  {	   14,	      0x1D,	    0,	   18	},
  {	   14,	      0x1E,	    0,	   17	},
  {	   14,	      0x1F,	    0,	   16	},
  {	   15,	      0x10,	    0,	   40	},
  {	   15,	      0x11,	    0,	   39	},
  {	   15,	      0x12,	    0,	   38	},
  {	   15,	      0x13,	    0,	   37	},
  {	   15,	      0x14,	    0,	   36	},
  {	   15,	      0x15,	    0,	   35	},
  {	   15,	      0x16,	    0,	   34	},
  {	   15,	      0x17,	    0,	   33	},
  {	   15,	      0x18,	    0,	   32	},
  {	   15,	      0x19,	    1,	   14	},
  {	   15,	      0x1A,	    1,	   13	},
  {	   15,	      0x1B,	    1,	   12	},
  {	   15,	      0x1C,	    1,	   11	},
  {	   15,	      0x1D,	    1,	   10	},
  {	   15,	      0x1E,	    1,	    9	},
  {	   15,	      0x1F,	    1,	    8	},
  {	   16,	      0x10,	    1,	   18	},
  {	   16,	      0x11,	    1,	   17	},
  {	   16,	      0x12,	    1,	   16	},
  {	   16,	      0x13,	    1,	   15	},
  {	   16,	      0x14,	    6,	    3	},
  {	   16,	      0x15,	   16,	    2	},
  {	   16,	      0x16,	   15,	    2	},
  {	   16,	      0x17,	   14,	    2	},
  {	   16,	      0x18,	   13,	    2	},
  {	   16,	      0x19,	   12,	    2	},
  {	   16,	      0x1A,	   11,	    2	},
  {	   16,	      0x1B,	   31,	    1	},
  {	   16,	      0x1C,	   30,	    1	},
  {	   16,	      0x1D,	   29,	    1	},
  {	   16,	      0x1E,	   28,	    1	},
  {	   16,	      0x1F,	   27,	    1	},
  {	   VLC_FAIL, VLC_FAIL, VLC_FAIL, VLC_FAIL }  
};



#define MACROBLOCK_QUANT                    0x20
#define MACROBLOCK_MOTION_FORWARD           0x10
#define MACROBLOCK_MOTION_BACKWARD          0x08
#define MACROBLOCK_PATTERN                  0x04
#define MACROBLOCK_INTRA                    0x02
#define SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG   0x01


vlc_table_t table_b2[]= {
  { 1, 1, MACROBLOCK_INTRA },
  { 2, 1, MACROBLOCK_QUANT | MACROBLOCK_INTRA },
  { VLC_FAIL, VLC_FAIL, VLC_FAIL }
};


vlc_table_t table_b3[]= {
  { 1, 1, MACROBLOCK_MOTION_FORWARD | MACROBLOCK_PATTERN },
  { 2, 1, MACROBLOCK_PATTERN },
  { 3, 1, MACROBLOCK_MOTION_FORWARD },
  { 5, 3, MACROBLOCK_INTRA },
  { 5, 2, MACROBLOCK_QUANT | MACROBLOCK_MOTION_FORWARD | MACROBLOCK_PATTERN },
  { 5, 1, MACROBLOCK_QUANT | MACROBLOCK_PATTERN },
  { 6, 1, MACROBLOCK_QUANT | MACROBLOCK_INTRA },
  { VLC_FAIL, VLC_FAIL, VLC_FAIL }
};


vlc_table_t table_b4[]= {
  { 2, 2, MACROBLOCK_MOTION_FORWARD | MACROBLOCK_MOTION_BACKWARD },
  { 2, 3, MACROBLOCK_MOTION_FORWARD | MACROBLOCK_MOTION_BACKWARD | MACROBLOCK_PATTERN },
  { 3, 2, MACROBLOCK_MOTION_BACKWARD },
  { 3, 3, MACROBLOCK_MOTION_BACKWARD | MACROBLOCK_PATTERN },
  { 4, 2, MACROBLOCK_MOTION_FORWARD}, 
  { 4, 3, MACROBLOCK_MOTION_FORWARD | MACROBLOCK_PATTERN},
  { 5, 3, MACROBLOCK_INTRA},
  { 5, 2, MACROBLOCK_QUANT | MACROBLOCK_MOTION_FORWARD | MACROBLOCK_MOTION_BACKWARD | MACROBLOCK_PATTERN},
  { 6, 3, MACROBLOCK_QUANT | MACROBLOCK_MOTION_FORWARD | MACROBLOCK_PATTERN},
  { 6, 2, MACROBLOCK_QUANT | MACROBLOCK_MOTION_BACKWARD | MACROBLOCK_PATTERN},
  { 6, 1, MACROBLOCK_QUANT | MACROBLOCK_INTRA },
  { VLC_FAIL, VLC_FAIL, VLC_FAIL }
};

#define VLC_MACROBLOCK_ESCAPE 255



vlc_table_t table_b1[] = {
  {	    1,	  0x1,	    1	},
  {	    3,	  0x3,	    2	},
  {	    3,	  0x2,	    3	},
  {	    4,	  0x3,	    4	},
  {	    4,	  0x2,	    5	},
  {	    5,	  0x3,	    6	},
  {	    5,	  0x2,	    7	},
  {	    7,	  0x7,	    8	},
  {	    7,	  0x6,	    9	},
  {	    8,	  0xB,	   10	},
  {	    8,	  0xA,	   11	},
  {	    8,	  0x9,	   12	},
  {	    8,	  0x8,	   13	},
  {	    8,	  0x7,	   14	},
  {	    8,	  0x6,	   15	},
  {	   10,	 0x17,	   16	},
  {	   10,	 0x16,	   17	},
  {	   10,	 0x15,	   18	},
  {	   10,	 0x14,	   19	},
  {	   10,	 0x13,	   20	},
  {	   10,	 0x12,	   21	},
  {	   11,	 0x23,	   22	},
  {	   11,	 0x22,	   23	},
  {	   11,	 0x21,	   24	},
  {	   11,	 0x20,	   25	},
  {	   11,	 0x1F,	   26	},
  {	   11,	 0x1E,	   27	},
  {	   11,	 0x1D,	   28	},
  {	   11,	 0x1C,	   29	},
  {	   11,	 0x1B,	   30	},
  {	   11,	 0x1A,	   31	},
  {	   11,	 0x19,	   32	},
  {	   11,	 0x18,	   33	},
  {	   11,	  0x8,	VLC_MACROBLOCK_ESCAPE },
  { VLC_FAIL, VLC_FAIL, VLC_FAIL }
};


#define MV_FORMAT_FIELD 0
#define MV_FORMAT_FRAME 1

#define PRED_TYPE_FIELD_BASED 0
#define PRED_TYPE_FRAME_BASED 1
#define PRED_TYPE_DUAL_PRIME 2
#define PRED_TYPE_16x8_MC 3



vlc_table_t table_b10[] = {
	{	    1,	  0x1,	    0	},
	{	    3,	  0x2,	    1	},
	{	    3,	  0x3,	   -1	},
	{	    4,	  0x2,	    2	},
	{	    4,	  0x3,	   -2	},
	{	    5,	  0x2,	    3	},
	{	    5,	  0x3,	   -3	},
	{	    7,	  0x6,	    4	},
	{	    7,	  0x7,	   -4	},
	{	    8,	  0x6,	    7	},
	{	    8,	  0x7,	   -7	},
	{	    8,	  0x8,	    6	},
	{	    8,	  0x9,	   -6	},
	{	    8,	  0xA,	    5	},
	{	    8,	  0xB,	   -5	},
	{	   10,	 0x12,	   10	},
	{	   10,	 0x13,	  -10	},
	{	   10,	 0x14,	    9	},
	{	   10,	 0x15,	   -9	},
	{	   10,	 0x16,	    8	},
	{	   10,	 0x17,	   -8	},
	{	   11,	 0x18,	   16	},
	{	   11,	 0x19,	  -16	},
	{	   11,	 0x1A,	   15	},
	{	   11,	 0x1B,	  -15	},
	{	   11,	 0x1C,	   14	},
	{	   11,	 0x1D,	  -14	},
	{	   11,	 0x1E,	   13	},
	{	   11,	 0x1F,	  -13	},
	{	   11,	 0x20,	   12	},
	{	   11,	 0x21,	  -12	},
	{	   11,	 0x22,	   11	},
	{	   11,	 0x23,	  -11	},
	{ VLC_FAIL, VLC_FAIL, VLC_FAIL }
};

vlc_table_t table_b11[] = {
  {	    1,	  0x0,	    0	},
  {	    2,	  0x2,	    1	},
  {	    2,	  0x3,	   -1	},
  { VLC_FAIL, VLC_FAIL, VLC_FAIL }
};

vlc_table_t table_b9[] = {
	{	    3,	  0x7,	   60	},
	{	    4,	  0xD,	    4	},
	{	    4,	  0xC,	    8	},
	{	    4,	  0xB,	   16	},
	{	    4,	  0xA,	   32	},
	{	    5,	 0x13,	   12	},
	{	    5,	 0x12,	   48	},
	{	    5,	 0x11,	   20	},
	{	    5,	 0x10,	   40	},
	{	    5,	  0xF,	   28	},
	{	    5,	  0xE,	   44	},
	{	    5,	  0xD,	   52	},
	{	    5,	  0xC,	   56	},
	{	    5,	  0xB,	    1	},
	{	    5,	  0xA,	   61	},
	{	    5,	  0x9,	    2	},
	{	    5,	  0x8,	   62	},
	{	    6,	  0xF,	   24	},
	{	    6,	  0xE,	   36	},
	{	    6,	  0xD,	    3	},
	{	    6,	  0xC,	   63	},
	{	    7,	 0x17,	    5	},
	{	    7,	 0x16,	    9	},
	{	    7,	 0x15,	   17	},
	{	    7,	 0x14,	   33	},
	{	    7,	 0x13,	    6	},
	{	    7,	 0x12,	   10	},
	{	    7,	 0x11,	   18	},
	{	    7,	 0x10,	   34	},
	{	    8,	 0x1F,	    7	},
	{	    8,	 0x1E,	   11	},
	{	    8,	 0x1D,	   19	},
	{	    8,	 0x1C,	   35	},
	{	    8,	 0x1B,	   13	},
	{	    8,	 0x1A,	   49	},
	{	    8,	 0x19,	   21	},
	{	    8,	 0x18,	   41	},
	{	    8,	 0x17,	   14	},
	{	    8,	 0x16,	   50	},
	{	    8,	 0x15,	   22	},
	{	    8,	 0x14,	   42	},
	{	    8,	 0x13,	   15	},
	{	    8,	 0x12,	   51	},
	{	    8,	 0x11,	   23	},
	{	    8,	 0x10,	   43	},
	{	    8,	  0xF,	   25	},
	{	    8,	  0xE,	   37	},
	{	    8,	  0xD,	   26	},
	{	    8,	  0xC,	   38	},
	{	    8,	  0xB,	   29	},
	{	    8,	  0xA,	   45	},
	{	    8,	  0x9,	   53	},
	{	    8,	  0x8,	   57	},
	{	    8,	  0x7,	   30	},
	{	    8,	  0x6,	   46	},
	{	    8,	  0x5,	   54	},
	{	    8,	  0x4,	   58	},
	{	    9,	  0x7,	   31	},
	{	    9,	  0x6,	   47	},
	{	    9,	  0x5,	   55	},
	{	    9,	  0x4,	   59	},
	{	    9,	  0x3,	   27	},
	{	    9,	  0x2,	   39	},
	{	    9,	  0x1,	    0	},
	{ VLC_FAIL, VLC_FAIL, VLC_FAIL }
};
