
#define PACK_START_CODE             0x000001ba
#define SYSTEM_HEADER_START_CODE    0x000001bb
#define PRIVATE_STREAM_1_START_CODE 0x000001bd
#define PADDING_STREAM_START_CODE   0x000001be
#define PRIVATE_STREAM_2_START_CODE 0x000001bf
#define VIDEO_STREAM_START_CODE     0x000001e0
#define AUDIO_STREAM_0_START_CODE   0x000001c0
#define AUDIO_STREAM_7_START_CODE   0x000001c7
#define AUDIO_STREAM_8_START_CODE   0x000001d0
#define AUDIO_STREAM_15_START_CODE  0x000001d7


#define BLOCK_SIZE 2048

typedef struct
{
  uint32_t bit_position;
  uint8_t bytes [BLOCK_SIZE];
} buffer_t;



uint32_t get_bits (buffer_t *buffer, uint32_t count);
uint32_t peek_bits (buffer_t *buffer, uint32_t count);
void unget_bits (buffer_t *buffer, uint32_t count);
void skip_bits (buffer_t *buffer, uint32_t count);
void expect_bits (FILE *out, buffer_t *buffer, uint32_t count, 
		  uint32_t expected, char *description);
void reserved_bits (FILE *out, buffer_t *buffer, uint32_t count);
int bits_avail (buffer_t *buffer);



#define PS2_PCI_SUBSTREAM_ID 0x00
#define PS2_DSI_SUBSTREAM_ID 0x01


typedef struct {
  uint32_t nv_pck_lbn;
  uint16_t vobu_cat;
  uint16_t zero1;
  uint32_t vobu_uop_ctl;
  uint32_t vobu_s_ptm;
  uint32_t vobu_e_ptm;
  uint32_t vobu_se_e_ptm;
  uint32_t e_eltm;
  char vobu_isrc[32];
} __attribute__ ((packed)) pci_gi_t;


typedef struct {
  uint32_t nsml_agl_dsta[9]; 
} __attribute__ ((packed)) nsml_agli_t;


typedef struct {
  uint16_t hli_ss; // only low 2 bits
  uint32_t hli_s_ptm;
  uint32_t hli_e_ptm;
  uint32_t btn_se_e_ptm;
#if WORDS_BIGENDIAN
  unsigned int zero1 : 2;
  unsigned int btngr_ns : 2;
  unsigned int zero2 : 1;
  unsigned int btngr1_dsp_ty : 3;
  unsigned int zero3 : 1;
  unsigned int btngr2_dsp_ty : 3;
  unsigned int zero4 : 1;
  unsigned int btngr3_dsp_ty : 3;
#else
  unsigned int btngr1_dsp_ty : 3;
  unsigned int zero2 : 1;
  unsigned int btngr_ns : 2;
  unsigned int zero1 : 2;
  unsigned int btngr3_dsp_ty : 3;
  unsigned int zero4 : 1;
  unsigned int btngr2_dsp_ty : 3;
  unsigned int zero3 : 1;
#endif
  uint8_t btn_ofn;
  uint8_t btn_ns; // only low 6 bits
  uint8_t nsl_btn_ns; // only low 6 bits
  uint8_t zero5;
  uint8_t fosl_btnn; // only low 6 bits
  uint8_t foac_btnn; // only low 6 bits
} __attribute__ ((packed)) hl_gi_t;

typedef struct {
  uint32_t btn_coli[3][2];
} __attribute__ ((packed)) btn_colit_t;

typedef struct {
#if WORDS_BIGENDIAN
  unsigned int btn_coln : 2;
  unsigned int x_start : 10;
  unsigned int zero1 : 2;
  unsigned int x_end : 10;
  unsigned int auto_action_mode : 2;
  unsigned int y_start : 10;
  unsigned int zero2 : 2;
  unsigned int y_end : 10;

  unsigned int zero3 : 2;
  unsigned int up : 6;
  unsigned int zero4 : 2;
  unsigned int down : 6;
  unsigned int zero5 : 2;
  unsigned int left : 6;
  unsigned int zero6 : 2;
  unsigned int right : 6;
#else
  unsigned int x_end : 10;
  unsigned int zero1 : 2;
  unsigned int x_start : 10;
  unsigned int btn_coln : 2;
  unsigned int y_end : 10;
  unsigned int zero2 : 2;
  unsigned int y_start : 10;
  unsigned int auto_action_mode : 2;

  unsigned int up : 6;
  unsigned int zero3 : 2;
  unsigned int down : 6;
  unsigned int zero4 : 2;
  unsigned int left : 6;
  unsigned int zero5 : 2;
  unsigned int right : 6;
  unsigned int zero6 : 2;
#endif
  uint8_t cmd[8];
} __attribute__ ((packed)) btni_t;

typedef struct {
  hl_gi_t     hl_gi;
  btn_colit_t btn_colit;
  btni_t     btnit[36];
} __attribute__ ((packed)) hli_t;

typedef struct {
  pci_gi_t    pci_gi;
  nsml_agli_t nsml_agli;
  hli_t       hli;
  uint8_t     zero1[189];
} __attribute__ ((packed)) pci_t;




typedef struct {
  uint32_t nv_pck_scr;
  uint32_t nv_pck_lbn;
  uint32_t vobu_ea;
  uint32_t vobu_1stref_ea;
  uint32_t vobu_2ndref_ea;
  uint32_t vobu_3rdref_ea;
  uint16_t vobu_vob_idn;
  uint8_t  zero1;
  uint8_t  vobu_c_idn;
  uint32_t c_eltm;
} __attribute__ ((packed)) dsi_gi_t;

typedef struct {
  uint8_t unknown[148];
} __attribute__ ((packed)) sml_pbi_t;

typedef struct {
  uint8_t unknown[9][6];
} __attribute__ ((packed)) sml_agli_t;

typedef struct {
  uint32_t unknown1; // Next (nv_pck_lbn+this value -> next vobu lbn)
  uint32_t unknown2[20]; //Forwards
  uint32_t unknown3[20]; //Backwars
  uint32_t unknown4; // Previous (nv_pck_lbn-this value -> previous vobu lbn)
} __attribute__ ((packed)) vobu_sri_t;

typedef struct {
  uint16_t offset; //  Highbit == signbit
  uint8_t unknown[142];
} __attribute__ ((packed)) synci_t;

typedef struct {
  dsi_gi_t   dsi_gi;
  sml_pbi_t  sml_pbi;
  sml_agli_t sml_agli;
  vobu_sri_t vobu_sri;
  synci_t    synci;
  uint8_t    zero1[471];
} __attribute__ ((packed)) dsi_t;


