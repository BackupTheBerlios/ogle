
//#define RL_BITFIELD
//#define INDEXBITS

#ifdef RL_BITFIELD
#define VLC_CODE_BITS(X) ((X).code_bits)
#define VLC_RUN(X) ((X).run)
#define VLC_LEVEL(X) ((X).level)

#define VLC_RL(NR, RUN, LEVEL) \
  { NR-1, RUN, LEVEL }

typedef struct {
  unsigned int code_bits : 4; // 1-16 (-1)         4bit (humm...)
  unsigned int run       : 6; // 0-31? (+ eob+63)  6bit
  unsigned int level     : 6; // 0-40  (+ eob, na) 6bit
} __attribute__ ((packed)) index_table_t;

#else

#define VLC_CODE_BITS(X) ((X) >> 12)
#define VLC_RUN(X) (((X) >> 6) & 0x3f)
#define VLC_LEVEL(X) ((X) & 0x3f)

#define VLC_RL(NR, RUN, LEVEL) \
  ((((NR-1) & 0x0f) << 12) | (((RUN) & 0x3f) <<  6) | (((LEVEL) & 0x3f) <<  0))

typedef uint16_t index_table_t;

#endif


#ifdef INDEXBITS
#define TOP_OFFSET(T, X) ((T)[(X)].offset)
#define TOP_IBITS(T, X) ((T)[(X)].index_bits)

#define TOP_VAL(OFFSET, INDEX_BITS) { OFFSET, INDEX_BITS }

typedef struct {
  uint8_t offset;       // offs in index_table 0-81 7bits
  uint8_t index_bits;   // 8-nr of bit to index the sub table with 0-8 4bit
} base_table_t;         // => total size 2byte / 16bit 

#else

#define TOP_OFFSET(T, X) ((T)[(X)])
/* This should calculate the number of bits to shift the sub index with
   The offset from in the top table 0->0,  1->4,  2->6,  3->6
   and all others must have the lower 5 bits [8-31] */
#define TOP_IBITS(T, X) \
   ( ((X) & (16+2)) | (((X) & (8+4+2)) << 1) | (((X) & 1) << 2) \
     | ((X) >> 2) | ((X) >> 4) )

#define TOP_VAL(OFFSET, INDEX_BITS) OFFSET

typedef uint8_t base_table_t;
#endif





const base_table_t top_b15[256] = {
  TOP_VAL( 83,  0 ), /* all except for the 0,0,0 error cases (32) */
  TOP_VAL( 67,  4 ), /* all except for the 0,0,0 error cases (6) */
  TOP_VAL( 55,  6 ),
  TOP_VAL( 51,  6 ),
  	      
  TOP_VAL( 24,  8 ), /*share*/
  TOP_VAL( 24,  8 ), /*share*/
  TOP_VAL( 24,  8 ), /*share*/
  TOP_VAL( 24,  8 ), /*share*/
  TOP_VAL( 50,  8 ),
  TOP_VAL( 50,  8 ),
  TOP_VAL( 20,  8 ), /*share*/
  TOP_VAL( 20,  8 ), /*share*/
  TOP_VAL( 49,  8 ),
  TOP_VAL( 49,  8 ),
  TOP_VAL( 23,  8 ), /*share*/
  TOP_VAL( 23,  8 ), /*share*/
  TOP_VAL( 48,  8 ),
  TOP_VAL( 48,  8 ),
  TOP_VAL( 48,  8 ),
  TOP_VAL( 48,  8 ),
  TOP_VAL( 47,  8 ),
  TOP_VAL( 47,  8 ),
  TOP_VAL( 47,  8 ),
  TOP_VAL( 47,  8 ),
  TOP_VAL( 46,  8 ),
  TOP_VAL( 46,  8 ),
  TOP_VAL( 46,  8 ),
  TOP_VAL( 46,  8 ),
  TOP_VAL( 16,  8 ), /*share*/
  TOP_VAL( 16,  8 ), /*share*/
  TOP_VAL( 16,  8 ), /*share*/
  TOP_VAL( 16,  8 ), /*share*/
  TOP_VAL( 45,  8 ),
  TOP_VAL( 12,  8 ), /*share*/
  TOP_VAL( 44,  8 ),
  TOP_VAL( 43,  8 ),
  TOP_VAL( 15,  8 ), /*share*/
  TOP_VAL( 13,  8 ), /*share*/
  TOP_VAL( 11,  8 ), /*share*/
  TOP_VAL( 42,  8 ),
  TOP_VAL( 41,  8 ),
  TOP_VAL( 41,  8 ),
  TOP_VAL( 41,  8 ),
  TOP_VAL( 41,  8 ),
  TOP_VAL( 41,  8 ),
  TOP_VAL( 41,  8 ),
  TOP_VAL( 41,  8 ),
  TOP_VAL( 41,  8 ),
  TOP_VAL( 40,  8 ),
  TOP_VAL( 40,  8 ),
  TOP_VAL( 40,  8 ),
  TOP_VAL( 40,  8 ),
  TOP_VAL( 40,  8 ),
  TOP_VAL( 40,  8 ),
  TOP_VAL( 40,  8 ),
  TOP_VAL( 40,  8 ),
  TOP_VAL(  5,  8 ), /*share*/
  TOP_VAL(  5,  8 ), /*share*/
  TOP_VAL(  5,  8 ), /*share*/
  TOP_VAL(  5,  8 ), /*share*/
  TOP_VAL(  5,  8 ), /*share*/
  TOP_VAL(  5,  8 ), /*share*/
  TOP_VAL(  5,  8 ), /*share*/
  TOP_VAL(  5,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL(  2,  8 ), /*share*/
  TOP_VAL( 39,  8 ),
  TOP_VAL( 39,  8 ),
  TOP_VAL( 39,  8 ),
  TOP_VAL( 39,  8 ),
  TOP_VAL( 39,  8 ),
  TOP_VAL( 39,  8 ),
  TOP_VAL( 39,  8 ),
  TOP_VAL( 39,  8 ),
  TOP_VAL( 39,  8 ),
  TOP_VAL( 39,  8 ),
  TOP_VAL( 39,  8 ),
  TOP_VAL( 39,  8 ),
  TOP_VAL( 39,  8 ),
  TOP_VAL( 39,  8 ),
  TOP_VAL( 39,  8 ),
  TOP_VAL( 39,  8 ),
  TOP_VAL( 38,  8 ),
  TOP_VAL( 38,  8 ),
  TOP_VAL( 38,  8 ),
  TOP_VAL( 38,  8 ),
  TOP_VAL( 38,  8 ),
  TOP_VAL( 38,  8 ),
  TOP_VAL( 38,  8 ),
  TOP_VAL( 38,  8 ),
  TOP_VAL( 38,  8 ),
  TOP_VAL( 38,  8 ),
  TOP_VAL( 38,  8 ),
  TOP_VAL( 38,  8 ),
  TOP_VAL( 38,  8 ),
  TOP_VAL( 38,  8 ),
  TOP_VAL( 38,  8 ),
  TOP_VAL( 38,  8 ),
  TOP_VAL(  0,  8 ), /*share*/
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ), /*share*/
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 37,  8 ),
  TOP_VAL( 36,  8 ),
  TOP_VAL( 36,  8 ),
  TOP_VAL( 36,  8 ),
  TOP_VAL( 36,  8 ),
  TOP_VAL( 36,  8 ),
  TOP_VAL( 36,  8 ),
  TOP_VAL( 36,  8 ),
  TOP_VAL( 36,  8 ),
  TOP_VAL( 35,  8 ),
  TOP_VAL( 35,  8 ),
  TOP_VAL( 35,  8 ),
  TOP_VAL( 35,  8 ),
  TOP_VAL( 35,  8 ),
  TOP_VAL( 35,  8 ),
  TOP_VAL( 35,  8 ),
  TOP_VAL( 35,  8 ),
  TOP_VAL( 22,  8 ), /*share*/
  TOP_VAL( 22,  8 ), /*share*/
  TOP_VAL( 34,  8 ),
  TOP_VAL( 34,  8 ),
  TOP_VAL( 33,  8 ),
  TOP_VAL( 33,  8 ),
  TOP_VAL( 32,  8 ),
  TOP_VAL( 32,  8 ),
  TOP_VAL( 31,  8 ),
  TOP_VAL( 31,  8 ),
  TOP_VAL( 30,  8 ),
  TOP_VAL( 29,  8 ),
  TOP_VAL( 28,  8 ),
  TOP_VAL( 27,  8 ),
  TOP_VAL( 26,  8 ),
  TOP_VAL( 25,  8 )
};
//const base_table_t top_b15_2[256];

const base_table_t top_b14[256] = {
  TOP_VAL( 83,  0 ),
  TOP_VAL( 67,  4 ),
  TOP_VAL( 63,  6 ),
  TOP_VAL( 59,  6 ),
	      
  TOP_VAL( 24,  8 ),
  TOP_VAL( 24,  8 ),
  TOP_VAL( 24,  8 ),
  TOP_VAL( 24,  8 ),
  TOP_VAL( 23,  8 ),
  TOP_VAL( 23,  8 ),
  TOP_VAL( 22,  8 ),
  TOP_VAL( 22,  8 ),
  TOP_VAL( 21,  8 ),
  TOP_VAL( 21,  8 ),
  TOP_VAL( 20,  8 ),
  TOP_VAL( 20,  8 ),
  TOP_VAL( 19,  8 ),
  TOP_VAL( 19,  8 ),
  TOP_VAL( 19,  8 ),
  TOP_VAL( 19,  8 ),
  TOP_VAL( 18,  8 ),
  TOP_VAL( 18,  8 ),
  TOP_VAL( 18,  8 ),
  TOP_VAL( 18,  8 ),
  TOP_VAL( 17,  8 ),
  TOP_VAL( 17,  8 ),
  TOP_VAL( 17,  8 ),
  TOP_VAL( 17,  8 ),
  TOP_VAL( 16,  8 ),
  TOP_VAL( 16,  8 ),
  TOP_VAL( 16,  8 ),
  TOP_VAL( 16,  8 ),
  TOP_VAL( 15,  8 ),
  TOP_VAL( 14,  8 ),
  TOP_VAL( 13,  8 ),
  TOP_VAL( 12,  8 ),
  TOP_VAL( 11,  8 ),
  TOP_VAL( 10,  8 ),
  TOP_VAL(  9,  8 ),
  TOP_VAL(  8,  8 ),
  TOP_VAL(  7,  8 ),
  TOP_VAL(  7,  8 ),
  TOP_VAL(  7,  8 ),
  TOP_VAL(  7,  8 ),
  TOP_VAL(  7,  8 ),
  TOP_VAL(  7,  8 ),
  TOP_VAL(  7,  8 ),
  TOP_VAL(  7,  8 ),
  TOP_VAL(  6,  8 ),
  TOP_VAL(  6,  8 ),
  TOP_VAL(  6,  8 ),
  TOP_VAL(  6,  8 ),
  TOP_VAL(  6,  8 ),
  TOP_VAL(  6,  8 ),
  TOP_VAL(  6,  8 ),
  TOP_VAL(  6,  8 ),
  TOP_VAL(  5,  8 ),
  TOP_VAL(  5,  8 ),
  TOP_VAL(  5,  8 ),
  TOP_VAL(  5,  8 ),
  TOP_VAL(  5,  8 ),
  TOP_VAL(  5,  8 ),
  TOP_VAL(  5,  8 ),
  TOP_VAL(  5,  8 ),
  TOP_VAL(  4,  8 ),
  TOP_VAL(  4,  8 ),
  TOP_VAL(  4,  8 ),
  TOP_VAL(  4,  8 ),
  TOP_VAL(  4,  8 ),
  TOP_VAL(  4,  8 ),
  TOP_VAL(  4,  8 ),
  TOP_VAL(  4,  8 ),
  TOP_VAL(  4,  8 ),
  TOP_VAL(  4,  8 ),
  TOP_VAL(  4,  8 ),
  TOP_VAL(  4,  8 ),
  TOP_VAL(  4,  8 ),
  TOP_VAL(  4,  8 ),
  TOP_VAL(  4,  8 ),
  TOP_VAL(  4,  8 ),
  TOP_VAL(  3,  8 ),
  TOP_VAL(  3,  8 ),
  TOP_VAL(  3,  8 ),
  TOP_VAL(  3,  8 ),
  TOP_VAL(  3,  8 ),
  TOP_VAL(  3,  8 ),
  TOP_VAL(  3,  8 ),
  TOP_VAL(  3,  8 ),
  TOP_VAL(  3,  8 ),
  TOP_VAL(  3,  8 ),
  TOP_VAL(  3,  8 ),
  TOP_VAL(  3,  8 ),
  TOP_VAL(  3,  8 ),
  TOP_VAL(  3,  8 ),
  TOP_VAL(  3,  8 ),
  TOP_VAL(  3,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  2,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  1,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 ),
  TOP_VAL(  0,  8 )
};
//const base_table_t top_b14_2[256];


const index_table_t sub_table[] = {
  VLC_RL(  2,  0,  1 ), /* replace with {1, 0, 1} for DCT_DC_FIRST */ /*share*/
  VLC_RL(  1, 63, 63 ), /* replace with {1, 0, 1} for DCT_DC_FIRST */ /* eob */
  VLC_RL(  3,  1,  1 ), /*share*/
  VLC_RL(  4,  2,  1 ), 
  VLC_RL(  4,  0,  2 ), 
  VLC_RL(  5,  3,  1 ), /*share*/
  VLC_RL(  5,  4,  1 ), 
  VLC_RL(  5,  0,  3 ), 
  VLC_RL(  8, 10,  1 ), 
  VLC_RL(  8,  0,  5 ), 
  VLC_RL(  8,  1,  3 ), 
  VLC_RL(  8,  3,  2 ), /*share*/
  VLC_RL(  8, 11,  1 ), /*share*/
  VLC_RL(  8, 12,  1 ), /*share*/
  VLC_RL(  8,  0,  6 ), 
  VLC_RL(  8, 13,  1 ), /*share*/
  VLC_RL(  6,  5,  1 ), /*share*/
  VLC_RL(  6,  1,  2 ), 
  VLC_RL(  6,  6,  1 ), 
  VLC_RL(  6,  7,  1 ), 
  VLC_RL(  7,  8,  1 ), /*share*/
  VLC_RL(  7,  0,  4 ), 
  VLC_RL(  7,  9,  1 ), /*share*/
  VLC_RL(  7,  2,  2 ), /*share*/
  VLC_RL(  6, 63, 63 ), /* index = 24 (esq, not really needed) */ /*share*/
  VLC_RL(  8,  0, 15 ), /*  0 25 */
  VLC_RL(  8,  0, 14 ), /*  1 26 */
  VLC_RL(  8,  4,  2 ), /*  2 27 */
  VLC_RL(  8,  2,  3 ), /*  3 28 */
  VLC_RL(  8,  0, 13 ), /*  4 29 */ 
  VLC_RL(  8,  0, 12 ), /*  5 30 */ 
  VLC_RL(  7,  0,  9 ), /*  6 31 */ 
  VLC_RL(  7,  0,  8 ), /*  7 32 */ 
  VLC_RL(  7, 10,  1 ), /*  8 33 */ 
  VLC_RL(  7,  1,  3 ), /*  9 34 */ 
  VLC_RL(  5,  0,  5 ), /* 11 35 */
  VLC_RL(  5,  0,  4 ), /* 12 36 */
  VLC_RL(  3,  0,  2 ), /* 13 37 */
  VLC_RL(  4,  0,  3 ), /* 15 38 */
  VLC_RL(  3, 63, 63 ), /* 16 39 */ /* eob */
  VLC_RL(  5,  1,  2 ), /* 19 40 */ 
  VLC_RL(  5,  2,  1 ), /* 20 41 */ 
  VLC_RL(  8,  1,  4 ), /* 21 42 */ 
  VLC_RL(  8,  0, 10 ), /* 25 43 */ 
  VLC_RL(  8,  0, 11 ), /* 26 44 */ 
  VLC_RL(  8,  1,  5 ), /* 28 45 */ 
  VLC_RL(  6,  4,  1 ), /* 30 46 */ 
  VLC_RL(  6,  0,  6 ), /* 31 47 */ 
  VLC_RL(  6,  0,  7 ), /* 32 48 */ 
  VLC_RL(  7,  6,  1 ), /* 34 49 */ 
  VLC_RL(  7,  7,  1 ), /* 36 50 */ 
  
  VLC_RL( 10,  2,  4 ), /* 38 51 */ 
  VLC_RL( 10, 16,  1 ), /* 38    */    
  VLC_RL(  9, 15,  1 ), /* 38    */    
  VLC_RL(  9, 15,  1 ), /* 38    */  
  
  VLC_RL(  9,  5,  2 ), /* 42 55 */ 
  VLC_RL(  9, 14,  1 ), /* 42    */
  VLC_RL(  9,  5,  2 ), /* 42    */ /* repeat two previous*/
  VLC_RL(  9, 14,  1 ), /* 42    */ /* to get 0,4,6,6 in both top tables */
  
  VLC_RL( 10,  1,  4 ), /* 25 -> 59 */
  VLC_RL( 10, 15,  1 ), 
  VLC_RL( 10, 14,  1 ), 
  VLC_RL( 10,  4,  2 ), 
  
  VLC_RL( 10, 16,  1 ), /* 29 -> 63 */
  VLC_RL( 10,  5,  2 ), 
  VLC_RL( 10,  0,  7 ), 
  VLC_RL( 10,  2,  3 ), 
  
  VLC_RL( 12,  0, 11 ), /* 33 -> 67 */ /*share*/
  VLC_RL( 12,  8,  2 ), 
  VLC_RL( 12,  4,  3 ), 
  VLC_RL( 12,  0, 10 ), 
  VLC_RL( 12,  2,  4 ), 
  VLC_RL( 12,  7,  2 ), 
  VLC_RL( 12, 21,  1 ), 
  VLC_RL( 12, 20,  1 ), 
  VLC_RL( 12,  0,  9 ), 
  VLC_RL( 12, 19,  1 ), 
  VLC_RL( 12, 18,  1 ), 
  VLC_RL( 12,  1,  5 ), 
  VLC_RL( 12,  3,  3 ), 
  VLC_RL( 12,  0,  8 ), 
  VLC_RL( 12,  6,  2 ), 
  VLC_RL( 12, 17,  1 ), 
  
  VLC_RL(  1,  0,  0 ), /* 49 -> 83 */ /*share*/
  VLC_RL(  1,  0,  0 ), /* could replace 81 with 81-16 = 75 */
  VLC_RL(  1,  0,  0 ), /* if we remove the 16 0,0,0 lines */
  VLC_RL(  1,  0,  0 ), /* the 0,0,0 are all 'impossible' */
  VLC_RL(  1,  0,  0 ), /* i.e. errors, code that should not appear normaly */
  VLC_RL(  1,  0,  0 ), 
  VLC_RL(  1,  0,  0 ), 
  VLC_RL(  1,  0,  0 ), 
  VLC_RL(  1,  0,  0 ), 
  VLC_RL(  1,  0,  0 ), 
  VLC_RL(  1,  0,  0 ), 
  VLC_RL(  1,  0,  0 ), 
  VLC_RL(  1,  0,  0 ), 
  VLC_RL(  1,  0,  0 ), 
  VLC_RL(  1,  0,  0 ), 
  VLC_RL(  1,  0,  0 ), 
  VLC_RL( 16,  1, 18 ), 
  VLC_RL( 16,  1, 17 ), 
  VLC_RL( 16,  1, 16 ), 
  VLC_RL( 16,  1, 15 ), 
  VLC_RL( 16,  6,  3 ), 
  VLC_RL( 16, 16,  2 ), 
  VLC_RL( 16, 15,  2 ), 
  VLC_RL( 16, 14,  2 ), 
  VLC_RL( 16, 13,  2 ), 
  VLC_RL( 16, 12,  2 ), 
  VLC_RL( 16, 11,  2 ), 
  VLC_RL( 16, 31,  1 ), 
  VLC_RL( 16, 30,  1 ), 
  VLC_RL( 16, 29,  1 ), 
  VLC_RL( 16, 28,  1 ), 
  VLC_RL( 16, 27,  1 ), 
  VLC_RL( 15,  0, 40 ), 
  VLC_RL( 15,  0, 40 ), 
  VLC_RL( 15,  0, 39 ), 
  VLC_RL( 15,  0, 39 ), 
  VLC_RL( 15,  0, 38 ), 
  VLC_RL( 15,  0, 38 ), 
  VLC_RL( 15,  0, 37 ), 
  VLC_RL( 15,  0, 37 ), 
  VLC_RL( 15,  0, 36 ), 
  VLC_RL( 15,  0, 36 ), 
  VLC_RL( 15,  0, 35 ), 
  VLC_RL( 15,  0, 35 ), 
  VLC_RL( 15,  0, 34 ), 
  VLC_RL( 15,  0, 34 ), 
  VLC_RL( 15,  0, 33 ), 
  VLC_RL( 15,  0, 33 ), 
  VLC_RL( 15,  0, 32 ), 
  VLC_RL( 15,  0, 32 ), 
  VLC_RL( 15,  1, 14 ), 
  VLC_RL( 15,  1, 14 ), 
  VLC_RL( 15,  1, 13 ), 
  VLC_RL( 15,  1, 13 ), 
  VLC_RL( 15,  1, 12 ), 
  VLC_RL( 15,  1, 12 ), 
  VLC_RL( 15,  1, 11 ), 
  VLC_RL( 15,  1, 11 ), 
  VLC_RL( 15,  1, 10 ), 
  VLC_RL( 15,  1, 10 ), 
  VLC_RL( 15,  1,  9 ), 
  VLC_RL( 15,  1,  9 ), 
  VLC_RL( 15,  1,  8 ), 
  VLC_RL( 15,  1,  8 ), 
  VLC_RL( 14,  0, 31 ), 
  VLC_RL( 14,  0, 31 ), 
  VLC_RL( 14,  0, 31 ), 
  VLC_RL( 14,  0, 31 ), 
  VLC_RL( 14,  0, 30 ), 
  VLC_RL( 14,  0, 30 ), 
  VLC_RL( 14,  0, 30 ), 
  VLC_RL( 14,  0, 30 ), 
  VLC_RL( 14,  0, 29 ), 
  VLC_RL( 14,  0, 29 ), 
  VLC_RL( 14,  0, 29 ), 
  VLC_RL( 14,  0, 29 ), 
  VLC_RL( 14,  0, 28 ), 
  VLC_RL( 14,  0, 28 ), 
  VLC_RL( 14,  0, 28 ), 
  VLC_RL( 14,  0, 28 ), 
  VLC_RL( 14,  0, 27 ), 
  VLC_RL( 14,  0, 27 ), 
  VLC_RL( 14,  0, 27 ), 
  VLC_RL( 14,  0, 27 ), 
  VLC_RL( 14,  0, 26 ), 
  VLC_RL( 14,  0, 26 ), 
  VLC_RL( 14,  0, 26 ), 
  VLC_RL( 14,  0, 26 ), 
  VLC_RL( 14,  0, 25 ), 
  VLC_RL( 14,  0, 25 ), 
  VLC_RL( 14,  0, 25 ), 
  VLC_RL( 14,  0, 25 ), 
  VLC_RL( 14,  0, 24 ), 
  VLC_RL( 14,  0, 24 ), 
  VLC_RL( 14,  0, 24 ), 
  VLC_RL( 14,  0, 24 ), 
  VLC_RL( 14,  0, 23 ), 
  VLC_RL( 14,  0, 23 ), 
  VLC_RL( 14,  0, 23 ), 
  VLC_RL( 14,  0, 23 ), 
  VLC_RL( 14,  0, 22 ), 
  VLC_RL( 14,  0, 22 ), 
  VLC_RL( 14,  0, 22 ), 
  VLC_RL( 14,  0, 22 ), 
  VLC_RL( 14,  0, 21 ), 
  VLC_RL( 14,  0, 21 ), 
  VLC_RL( 14,  0, 21 ), 
  VLC_RL( 14,  0, 21 ), 
  VLC_RL( 14,  0, 20 ), 
  VLC_RL( 14,  0, 20 ), 
  VLC_RL( 14,  0, 20 ), 
  VLC_RL( 14,  0, 20 ), 
  VLC_RL( 14,  0, 19 ), 
  VLC_RL( 14,  0, 19 ), 
  VLC_RL( 14,  0, 19 ), 
  VLC_RL( 14,  0, 19 ), 
  VLC_RL( 14,  0, 18 ), 
  VLC_RL( 14,  0, 18 ), 
  VLC_RL( 14,  0, 18 ), 
  VLC_RL( 14,  0, 18 ), 
  VLC_RL( 14,  0, 17 ), 
  VLC_RL( 14,  0, 17 ), 
  VLC_RL( 14,  0, 17 ), 
  VLC_RL( 14,  0, 17 ), 
  VLC_RL( 14,  0, 16 ), 
  VLC_RL( 14,  0, 16 ), 
  VLC_RL( 14,  0, 16 ), 
  VLC_RL( 14,  0, 16 ), 
  VLC_RL( 13, 10,  2 ), 
  VLC_RL( 13, 10,  2 ), 
  VLC_RL( 13, 10,  2 ), 
  VLC_RL( 13, 10,  2 ), 
  VLC_RL( 13, 10,  2 ), 
  VLC_RL( 13, 10,  2 ), 
  VLC_RL( 13, 10,  2 ), 
  VLC_RL( 13, 10,  2 ), 
  VLC_RL( 13,  9,  2 ), 
  VLC_RL( 13,  9,  2 ), 
  VLC_RL( 13,  9,  2 ), 
  VLC_RL( 13,  9,  2 ), 
  VLC_RL( 13,  9,  2 ), 
  VLC_RL( 13,  9,  2 ), 
  VLC_RL( 13,  9,  2 ), 
  VLC_RL( 13,  9,  2 ), 
  VLC_RL( 13,  5,  3 ), 
  VLC_RL( 13,  5,  3 ), 
  VLC_RL( 13,  5,  3 ), 
  VLC_RL( 13,  5,  3 ), 
  VLC_RL( 13,  5,  3 ), 
  VLC_RL( 13,  5,  3 ), 
  VLC_RL( 13,  5,  3 ), 
  VLC_RL( 13,  5,  3 ), 
  VLC_RL( 13,  3,  4 ), 
  VLC_RL( 13,  3,  4 ), 
  VLC_RL( 13,  3,  4 ), 
  VLC_RL( 13,  3,  4 ), 
  VLC_RL( 13,  3,  4 ), 
  VLC_RL( 13,  3,  4 ), 
  VLC_RL( 13,  3,  4 ), 
  VLC_RL( 13,  3,  4 ), 
  VLC_RL( 13,  2,  5 ), 
  VLC_RL( 13,  2,  5 ), 
  VLC_RL( 13,  2,  5 ), 
  VLC_RL( 13,  2,  5 ), 
  VLC_RL( 13,  2,  5 ), 
  VLC_RL( 13,  2,  5 ), 
  VLC_RL( 13,  2,  5 ), 
  VLC_RL( 13,  2,  5 ), 
  VLC_RL( 13,  1,  7 ), 
  VLC_RL( 13,  1,  7 ), 
  VLC_RL( 13,  1,  7 ), 
  VLC_RL( 13,  1,  7 ), 
  VLC_RL( 13,  1,  7 ), 
  VLC_RL( 13,  1,  7 ), 
  VLC_RL( 13,  1,  7 ), 
  VLC_RL( 13,  1,  7 ), 
  VLC_RL( 13,  1,  6 ), 
  VLC_RL( 13,  1,  6 ), 
  VLC_RL( 13,  1,  6 ), 
  VLC_RL( 13,  1,  6 ), 
  VLC_RL( 13,  1,  6 ), 
  VLC_RL( 13,  1,  6 ), 
  VLC_RL( 13,  1,  6 ), 
  VLC_RL( 13,  1,  6 ), 
  VLC_RL( 13,  0, 15 ), 
  VLC_RL( 13,  0, 15 ), 
  VLC_RL( 13,  0, 15 ), 
  VLC_RL( 13,  0, 15 ), 
  VLC_RL( 13,  0, 15 ), 
  VLC_RL( 13,  0, 15 ), 
  VLC_RL( 13,  0, 15 ), 
  VLC_RL( 13,  0, 15 ), 
  VLC_RL( 13,  0, 14 ), 
  VLC_RL( 13,  0, 14 ), 
  VLC_RL( 13,  0, 14 ), 
  VLC_RL( 13,  0, 14 ), 
  VLC_RL( 13,  0, 14 ), 
  VLC_RL( 13,  0, 14 ), 
  VLC_RL( 13,  0, 14 ), 
  VLC_RL( 13,  0, 14 ), 
  VLC_RL( 13,  0, 13 ), 
  VLC_RL( 13,  0, 13 ), 
  VLC_RL( 13,  0, 13 ), 
  VLC_RL( 13,  0, 13 ), 
  VLC_RL( 13,  0, 13 ), 
  VLC_RL( 13,  0, 13 ), 
  VLC_RL( 13,  0, 13 ), 
  VLC_RL( 13,  0, 13 ), 
  VLC_RL( 13,  0, 12 ), 
  VLC_RL( 13,  0, 12 ), 
  VLC_RL( 13,  0, 12 ), 
  VLC_RL( 13,  0, 12 ), 
  VLC_RL( 13,  0, 12 ), 
  VLC_RL( 13,  0, 12 ), 
  VLC_RL( 13,  0, 12 ), 
  VLC_RL( 13,  0, 12 ), 
  VLC_RL( 13, 26,  1 ), 
  VLC_RL( 13, 26,  1 ), 
  VLC_RL( 13, 26,  1 ), 
  VLC_RL( 13, 26,  1 ), 
  VLC_RL( 13, 26,  1 ), 
  VLC_RL( 13, 26,  1 ), 
  VLC_RL( 13, 26,  1 ), 
  VLC_RL( 13, 26,  1 ), 
  VLC_RL( 13, 25,  1 ), 
  VLC_RL( 13, 25,  1 ), 
  VLC_RL( 13, 25,  1 ), 
  VLC_RL( 13, 25,  1 ), 
  VLC_RL( 13, 25,  1 ), 
  VLC_RL( 13, 25,  1 ), 
  VLC_RL( 13, 25,  1 ), 
  VLC_RL( 13, 25,  1 ), 
  VLC_RL( 13, 24,  1 ), 
  VLC_RL( 13, 24,  1 ), 
  VLC_RL( 13, 24,  1 ), 
  VLC_RL( 13, 24,  1 ), 
  VLC_RL( 13, 24,  1 ), 
  VLC_RL( 13, 24,  1 ), 
  VLC_RL( 13, 24,  1 ), 
  VLC_RL( 13, 24,  1 ), 
  VLC_RL( 13, 23,  1 ), 
  VLC_RL( 13, 23,  1 ), 
  VLC_RL( 13, 23,  1 ), 
  VLC_RL( 13, 23,  1 ), 
  VLC_RL( 13, 23,  1 ), 
  VLC_RL( 13, 23,  1 ), 
  VLC_RL( 13, 23,  1 ), 
  VLC_RL( 13, 23,  1 ), 
  VLC_RL( 13, 22,  1 ), 
  VLC_RL( 13, 22,  1 ), 
  VLC_RL( 13, 22,  1 ), 
  VLC_RL( 13, 22,  1 ), 
  VLC_RL( 13, 22,  1 ), 
  VLC_RL( 13, 22,  1 ), 
  VLC_RL( 13, 22,  1 ), 
  VLC_RL( 13, 22,  1 )
};
