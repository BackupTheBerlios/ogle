/*
 * VOBDUMP: a program for examining DVD .VOB filse
 *
 * Copyright 1998, 1999 Eric Smith <eric@brouhaha.com>
 *
 * VOBDUMP is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.  Note that I am not
 * granting permission to redistribute or modify VOBDUMP under the
 * terms of any later version of the General Public License.
 *
 * This program is distributed in the hope that it will be useful (or
 * at least amusing), but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the file "COPYING"); if not, write to
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA
 * 02139, USA.
 *
 * $Id: vobdump.c,v 1.2 2000/08/22 09:50:45 d95mback Exp $
 */

#include <stdio.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;

#define BLOCK_SIZE 2048


#define START_CODE_BITS 32
#define PACKET_LENGTH_BITS 16


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

#define PS2_PCI_SUBSTREAM_ID 0x00
#define PS2_DSI_SUBSTREAM_ID 0x01


/* these lengths in bytes, exclusive of start code and length */
#define SYSTEM_HEADER_BYTES 0x12
#define PCI_BYTES 0x3d4
#define DSI_BYTES 0x3fa


#define COMMAND_BYTES 8

typedef struct
{
  u8 bits [COMMAND_BYTES];
  u8 examined [COMMAND_BYTES];
} cmd_t;


typedef struct
{
  u32 bit_position;
  u8 bytes [BLOCK_SIZE];
} buffer_t;


u32 get_bits (buffer_t *buffer, u32 count)
{
  unsigned long val = 0;
  if ((buffer->bit_position + count) > (BLOCK_SIZE * 8))
    {
      fprintf (stderr, "attempt to read past end of block\n");
      exit (2);
    }
  while (count--)
    {
      val <<= 1;
      if ((buffer->bytes [buffer->bit_position>>3]) & 
	  (0x80 >> (buffer->bit_position & 7)))
	val |= 1;
      buffer->bit_position++;
    }
  return (val);
}


u32 peek_bits (buffer_t *buffer, u32 count)
{
  unsigned long val = 0;
  u32 count2 = count;

  if ((buffer->bit_position + count) > (BLOCK_SIZE * 8))
    {
      fprintf (stderr, "attempt to read past end of block\n");
      exit (2);
    }
  while (count--)
    {
      val <<= 1;
      if ((buffer->bytes [buffer->bit_position>>3]) & 
	  (0x80 >> (buffer->bit_position & 7)))
	val |= 1;
      buffer->bit_position++;
    }
  buffer->bit_position -= count2;
  return (val);
}


void unget_bits (buffer_t *buffer, u32 count)
{
  if (buffer->bit_position < count)
    {
      fprintf (stderr, "attempt to unget too many bits\n");
      exit (2);
    }
  buffer->bit_position -= count;
}


void skip_bits (buffer_t *buffer, u32 count)
{
  if ((buffer->bit_position + count) > (BLOCK_SIZE * 8))
    {
      fprintf (stderr, "attempt to skip past end of block\n");
      exit (2);
    }
  buffer->bit_position += count;
}


void expect_bits (FILE *out, buffer_t *buffer, u32 count, u32 expected, char *description)
{
  u32 val;
  val = get_bits (buffer, count);
  if (val != expected)
    fprintf (out, "%s is 0x%x, should be 0x%x\n", description, val, expected);
}


void reserved_bits (FILE *out, buffer_t *buffer, u32 count)
{
  if ((buffer->bit_position + count) > (BLOCK_SIZE * 8))
    {
      fprintf (stderr, "attempt to read past end of block\n");
      exit (2);
    }
  while (count)
    {
      if ((buffer->bytes [buffer->bit_position>>3]) & 
	  (0x80 >> (buffer->bit_position & 7)))
	{
	  fprintf (out, "reserved bits not all zeros\n");
	  buffer->bit_position += count;
	  count = 0;
	}
      else
	{
	  buffer->bit_position++;
	  count--;
	}
    }
}


int bits_avail (buffer_t *buffer)
{
  return (buffer->bit_position < (BLOCK_SIZE * 8));
}



u32 get_command_bits (cmd_t *cmd, int bit_position, int count)
{
  u32 val = 0;
  int byte, bit;
  while (count--)
    {
      byte = 7 - (bit_position >> 3);
      bit = 0x01 << (bit_position & 0x07);
      val <<= 1;
      if ((cmd->bits [byte]) & bit)
	val |= 1;
      cmd->examined [byte] |= bit;
      bit_position--;
    }
  return (val);
}


char *compare_option_name [8] =
{
  NULL, "BC", "EQ", "NE", "GE", "GT", "LE", "LT"
};

void dump_scg (FILE *out, cmd_t *cmd, int bit_position)
{
  int prm = get_command_bits (cmd, bit_position, 4);
  fprintf (out, "g%d", prm);
}

void dump_cp1 (FILE *out, cmd_t *cmd, int bit_position)
{
  int prm = get_command_bits (cmd, bit_position - 4, 4);
  fprintf (out, "g%d", prm);
}

void dump_cp2 (FILE *out, cmd_t *cmd, int bit_position)
{
  int prm_flag;
  int prm;

  prm_flag = get_command_bits (cmd, bit_position, 1);
  if (prm_flag)
    prm = get_command_bits (cmd, bit_position - 3, 5);
  else
    prm = get_command_bits (cmd, bit_position - 4, 4);
  fprintf (out, "%c%d", prm_flag ? 's' : 'g', prm);
}

void dump_c2 (FILE *out, cmd_t *cmd, int immed_bit_position, int bit_position)
{
  int immediate;
  if (immed_bit_position < 0)
    immediate = 0;
  else
    immediate = get_command_bits (cmd, immed_bit_position, 1);
  if (immediate)
    {
      int val = get_command_bits (cmd, bit_position, 16);
      fprintf (out, "#%d", val);
    }
  else
    dump_cp2 (out, cmd, bit_position + 8);
}

void dump_goto_command (FILE *out, cmd_t *cmd)
{
  int immediate = get_command_bits (cmd, 55, 1);
  int compare = get_command_bits (cmd, 54, 3);
  int branch = get_command_bits (cmd, 51, 4);

  if (compare)
    {
      fprintf (out, "cp%s", compare_option_name [compare]);
      if (immediate)
	fprintf (out, "I");
      fprintf (out, "_");
      dump_cp1 (out, cmd, 39);
      fprintf (out, ",");
      dump_c2 (out, cmd, 55, 31);
      fprintf (out, " ");
    }

  switch (branch)
    {
    case 0:
      /* no args */
      fprintf (out, "Nop");
      break;
    case 1:
      {
	int nav_cmd = get_command_bits (cmd, 7, 8);
	fprintf (out, "GoTo %d", nav_cmd);
	break;
      }
    case 2:
      /* no args */
      fprintf (out, "Break");
      break;
    case 3:
      {
	int parental_level = get_command_bits (cmd, 11, 4);
	int nav_cmd = get_command_bits (cmd, 7, 8);
	fprintf (out, "SetTmpPML %d,%d", parental_level, nav_cmd);
	break;
      }
    default:
      fprintf (out, "unknown");
      break;
    }
}

char *sublink_name [] =
{
  "LinkNoLink",  "LinkTopC",    "LinkNextC",   "LinkPrevC",
  NULL,          "LinkTopPG",   "LinkNextPG",  "LinkPrevPG",
  NULL,          "LinkTopPGC",  "LinkNextPGC", "LinkPrevPGC",
  "LinkGoUpPGC", "LinkTailPGC", NULL,          NULL,
  "RSM"
};

void dump_link_command (FILE *out, cmd_t *cmd)
{
  int immediate = get_command_bits (cmd, 55, 1);
  int compare = get_command_bits (cmd, 54, 3);
  int branch = get_command_bits (cmd, 51, 4);

  if (compare)
    {
      fprintf (out, "%s", compare_option_name [compare]);
      if (immediate)
	fprintf (out, "I");
      fprintf (out, "_");
      dump_cp1 (out, cmd, 39);
      fprintf (out, ",");
      dump_c2 (out, cmd, 55, 31);
      fprintf (out, " ");
    }

  switch (branch)
    {
    case 1:
      {
	int subinst = get_command_bits (cmd, 7, 8);
	int button = get_command_bits (cmd, 15, 6);
	if ((subinst <= 0x10) && sublink_name [subinst])
	  fprintf (out, "%s %d", sublink_name [subinst], button);
	else
	  fprintf (out, "LinkSIns[0x%02x] %d", subinst, button);
	break;
      }
    case 4:
      {
	int pgcn = get_command_bits (cmd, 14, 15);
	fprintf (out, "LinkPGCN %d", pgcn);
	break;
      }
    case 5:
      {
	int button = get_command_bits (cmd, 15, 6);
	int pttn = get_command_bits (cmd, 9, 10);
	fprintf (out, "LinkPTTN %d,%d", button, pttn);
	break;
      }
    case 6:
      {
	int button = get_command_bits (cmd, 15, 6);
	int pgn = get_command_bits (cmd, 6, 7);
	fprintf (out, "LinkPGN %d,%d", button, pgn);
	break;
      }
    case 7:
      {
	int button = get_command_bits (cmd, 15, 6);
	int cn = get_command_bits (cmd, 7, 8);
	fprintf (out, "LinkCN %d,%d", button, cn);
	break;
      }
    default:
      fprintf (out, "unknown");
      break;
    }
}

char *domain_name [] =
{ "FP_DOM", "VMGM_DOM", "VTSM_DOM", "VMGM_DOM" };

void dump_jump_command (FILE *out, cmd_t *cmd)
{
  int compare = get_command_bits (cmd, 54, 3);
  int branch = get_command_bits (cmd, 51, 4);

  if (compare)
    {
      fprintf (out, "cp%s", compare_option_name [compare]);
      fprintf (out, "_");
      dump_cp1 (out, cmd, 15);
      fprintf (out, ",");
      dump_cp2 (out, cmd, 7);
      fprintf (out, " ");
    }

  switch (branch)
    {
    case 1:
      /* no args */
      fprintf (out, "Exit");
      break;
    case 2:
      {
	int title = get_command_bits (cmd, 22, 7);
	fprintf (out, "JumpTT %d", title);
	break;
      }
    case 3:
      {
	int vts_tt = get_command_bits (cmd, 22, 7);
	fprintf (out, "JumpVTS_TT %d", vts_tt);
	break;
      }
    case 5:
      {
	int vts_tt = get_command_bits (cmd, 22, 7);
	int ptt = get_command_bits (cmd, 41, 10);
	fprintf (out, "JumpVTS_PTT %d,%d", vts_tt, ptt);
	break;
      }
    case 6:
      {
	int domain_id, menu_id, vmgm_pgc, vts, vts_tt;
	domain_id = get_command_bits (cmd, 23, 2);
	fprintf (out, "JumpSS domain=%s", domain_name [domain_id]);
	if ((domain_id == 1) || (domain_id == 2))
	  {
	    menu_id = get_command_bits (cmd, 19, 4);
	    fprintf (out, " menu=%d", menu_id);
	  }
	if (domain_id == 2)
	  {
	    vts = get_command_bits (cmd, 30, 7);
	    vts_tt = get_command_bits (cmd, 38, 7);
	    fprintf (out, " vts=%d vts_tt=%d", vts, vts_tt);
	  }
	else
	  {
	    vmgm_pgc = get_command_bits (cmd, 46, 15);
	    fprintf (out, " vmgm_pgc=%d", vmgm_pgc);
	  }
	break;
      }
    case 8:
      {
	int domain_id, menu_id, vmgm_pgc, rsm_cell;
	domain_id = get_command_bits (cmd, 23, 2);
	fprintf (out, "CallSS domain=%s", domain_name [domain_id]);
	if ((domain_id == 1) || (domain_id == 2))
	  {
	    menu_id = get_command_bits (cmd, 19, 4);
	    fprintf (out, " menu=%d", menu_id);
	  }
	if (domain_id != 2)
	  {
	    vmgm_pgc = get_command_bits (cmd, 46, 15);
	    fprintf (out, " vmgm_pgc=%d", vmgm_pgc);
	  }
	rsm_cell = get_command_bits (cmd, 31, 8);
	fprintf (out, "rsm_cell=%d", rsm_cell);
	break;
      }
    default:
      fprintf (out, "unknown");
      break;
    }
}

void dump_setsystem_command (FILE *out, cmd_t *cmd)
{
  int compare = get_command_bits (cmd, 54, 3);

  if (compare)
    {
      fprintf (out, "cp%s", compare_option_name [compare]);
      fprintf (out, "_");
      dump_cp1 (out, cmd, 15);
      fprintf (out, ",");
      dump_cp2 (out, cmd, 7);
      fprintf (out, " ");
    }

  fprintf (out, "setsystem");
  /* $$$ more code needed here */
}

void dump_set_command (FILE *out, cmd_t *cmd)
{
  fprintf (out, "set");
  /* $$$ more code needed here */
}

void dump_set_compare_linksins_command (FILE *out, cmd_t *cmd)
{
  fprintf (out, "set compare linksins");
  /* $$$ more code needed here */
}

void dump_compare_and_set_linksins_command (FILE *out, cmd_t *cmd)
{
  fprintf (out, "compare and set linksins");
  /* $$$ more code needed here */
}

void dump_compare_set_and_linksins_command (FILE *out, cmd_t *cmd)
{
  fprintf (out, "compare set and linksins");
  /* $$$ more code needed here */
}

void dump_command (FILE *out, buffer_t *buffer)
{
  int i;
  cmd_t cmd;
  int cmd_id1;
  int extra_bits;

  for (i = 0; i < 8; i++)
    {
      cmd.bits [i] = get_bits (buffer, 8);
      cmd.examined [i] = 0;
    }

#if 0
  for (i = 0; i < 8; i++)
    fprintf (out, "%02x ", cmd.bits [i]);
  fprintf (out, " ");
#endif

  cmd_id1 = get_command_bits (& cmd, 63, 3);

  switch (cmd_id1)
    {
    case 0:  dump_goto_command (out, & cmd); break;
    case 1:
      {
	int cmd_id2 = get_command_bits (& cmd, 60, 1);
	if (cmd_id2 == 0)
	  dump_link_command (out, & cmd);
	else
	  dump_jump_command (out, & cmd);
	break;
      }
    case 2: dump_setsystem_command (out, & cmd); break;
    case 3: dump_set_command (out, & cmd); break;
    case 4: dump_set_compare_linksins_command (out, & cmd); break;
    case 5: dump_compare_and_set_linksins_command (out, & cmd); break;
    case 6: dump_compare_set_and_linksins_command (out, & cmd); break;
    case 7: fprintf (out, "unknown"); break;
    }

  extra_bits = 0;
  for (i = 0; i < 8; i++)
    if (cmd.bits [i] & ~ cmd.examined [i])
      {
	extra_bits = 1;
	break;
      }
  if (extra_bits)
    {
      fprintf (out, "  extra bits");
      for (i = 0; i < 8; i++)
	fprintf (out, " %02x", cmd.bits [i] & ~ cmd.examined [i]);
    }

  //fprintf (out, "\n");
}


void dump_system_header (FILE *out, buffer_t *buffer)
{
  u32 start_code;
  u16 length;

  start_code = get_bits (buffer, START_CODE_BITS);
  length = get_bits (buffer, PACKET_LENGTH_BITS);

  fprintf (out, "system header\n");
  if (length != SYSTEM_HEADER_BYTES)
    {
      fprintf (out, "length is 0x%04x, should be 0x%04x\n", length,
	       SYSTEM_HEADER_BYTES);
      skip_bits (buffer, 8 * length);
      return;
    }
  skip_bits (buffer, 8 * length);
}

void dump_ps1_packet (FILE *out, buffer_t *buffer)
{
  u32 start_code;
  u16 length;

  start_code = get_bits (buffer, START_CODE_BITS);
  length = get_bits (buffer, PACKET_LENGTH_BITS);

  fprintf (out, "ps1 packet, length 0x%04x\n", length);
  skip_bits (buffer, 8 * length);
}

void dump_padding_packet (FILE *out, buffer_t *buffer)
{
  u32 start_code;
  u16 length;

  start_code = get_bits (buffer, START_CODE_BITS);
  length = get_bits (buffer, PACKET_LENGTH_BITS);

  fprintf (out, "padding packet, length 0x%04x\n", length);
  skip_bits (buffer, 8 * length);
}

void dump_pci_gi (FILE *out, buffer_t *buffer)
{
  int i, c;

  fprintf (out, "pci_gi:\n");
  fprintf (out, "nv_pck_lbn    0x%08x\n", get_bits (buffer, 32));
  fprintf (out, "vobu_cat      0x%04x\n", get_bits (buffer, 16));
  reserved_bits (out, buffer, 16);
  fprintf (out, "vobu_uop_ctl  0x%08x\n", get_bits (buffer, 32));
  fprintf (out, "vobu_s_ptm    0x%08x\n", get_bits (buffer, 32));
  fprintf (out, "vobu_e_ptm    0x%08x\n", get_bits (buffer, 32));
  fprintf (out, "vobu_se_e_ptm 0x%08x\n", get_bits (buffer, 32));
  fprintf (out, "e_eltm        0x%08x\n", get_bits (buffer, 32));
  fprintf (out, "vobu_isrc     \"");
  for (i = 0; i < 32; i++)
    {
      c = get_bits (buffer, 8);
      if ((c >= ' ')  && (c <= '~'))
	fprintf (out, "%c", c);
      else
	fprintf (out, ".");
    }
  fprintf (out, "\"\n");
}

void dump_nsml_agli (FILE *out, buffer_t *buffer)
{
  int i;

  fprintf (out, "nsml_agli:\n");
  for (i = 1; i <= 9; i++)
    fprintf (out, "nsml_agl_c%d_dsta  0x%08x\n", i, get_bits (buffer, 32));
}

void dump_hl_gi (FILE *out, buffer_t *buffer, int *btngr_ns, int *btn_ns)
{
  int i;

  fprintf (out, "hl_gi:\n");
  reserved_bits (out, buffer, 14);
  fprintf (out, "hli_ss        0x%01x\n", get_bits (buffer, 2));
  fprintf (out, "hli_s_ptm     0x%08x\n", get_bits (buffer, 32));
  fprintf (out, "hli_e_ptm     0x%08x\n", get_bits (buffer, 32));
  fprintf (out, "btn_se_e_ptm  0x%08x\n", get_bits (buffer, 32));

  /* fprintf (out, "btn_md        0x%04x\n", get_bits (buffer, 16)); */
  reserved_bits (out, buffer, 2);
  (* btngr_ns) = get_bits (buffer, 2);
  fprintf (out, "btngr_ns      %d\n", (* btngr_ns));
  for (i = 1; i <= 3; i++)
    {
      reserved_bits (out, buffer, 1);
      fprintf (out, "btngr%d_dsp_ty    0x%02x\n", i, get_bits (buffer, 3));
    }

  fprintf (out, "btn_ofn       0x%02x\n", get_bits (buffer, 8));
  reserved_bits (out, buffer, 2);
  (* btn_ns) = get_bits (buffer, 6);
  fprintf (out, "btn_ns        0x%02x\n", (* btn_ns));
  reserved_bits (out, buffer, 2);
  fprintf (out, "nsl_btn_ns    0x%02x\n", get_bits (buffer, 6));
  reserved_bits (out, buffer, 8);
  reserved_bits (out, buffer, 2);
  fprintf (out, "fosl_btnn     0x%02x\n", get_bits (buffer, 6));
  reserved_bits (out, buffer, 2);
  fprintf (out, "foac_btnn     0x%02x\n", get_bits (buffer, 6));
}

void dump_btn_colit (FILE *out, buffer_t *buffer)
{
  int i, j;
  fprintf (out, "btn_colit:\n");
  for (i = 1; i <= 3; i++)
    for (j = 0; j <= 1; j++)
      {
	fprintf (out, "btn_coli %d  %s_coli:  %08x\n",
		 i,
		 (j == 0) ? "sl" : "ac",
		 get_bits (buffer, 32));
      }
}

void dump_btnit (FILE *out, buffer_t *buffer, int btngr_ns, int btn_ns)
{
  int i, j;
  u32 x_start, x_end, y_start, y_end;
  u32 btn_coln, auto_action_mode;

  fprintf (out, "btnit:\n");

  for (i = 1; i <= btngr_ns; i++)
    for (j = 1; j <= (36 / btngr_ns); j++)
      if (j <= btn_ns)
	{
	  btn_coln = get_bits (buffer, 2);

	  x_start = get_bits (buffer, 10);
	  reserved_bits (out, buffer, 2);
	  x_end = get_bits (buffer, 10);

	  auto_action_mode = get_bits (buffer, 2);

	  y_start = get_bits (buffer, 10);
	  reserved_bits (out, buffer, 2);
	  y_end = get_bits (buffer, 10);

	  fprintf (out, "group %d btni %d:  ", i, j);
	  fprintf (out, "btn_coln %d, auto_action_mode %d\n",
		   btn_coln, auto_action_mode);
	  fprintf (out, "coords   (0x%03x, 0x%03x) .. (0x%03x, 0x%03x)\n",
		   x_start, y_start, x_end, y_end);

	  reserved_bits (out, buffer, 2);
	  fprintf (out, "up %d, ", get_bits (buffer, 6));
	  reserved_bits (out, buffer, 2);
	  fprintf (out, "down %d, ", get_bits (buffer, 6));
	  reserved_bits (out, buffer, 2);
	  fprintf (out, "left %d, ", get_bits (buffer, 6));
	  reserved_bits (out, buffer, 2);
	  fprintf (out, "right %d\n", get_bits (buffer, 6));

	  dump_command (out, buffer);
	}
      else
	reserved_bits (out, buffer, 8 * 18);
}

void dump_hli (FILE *out, buffer_t *buffer)
{
  int btngr_ns, btn_ns;

  fprintf (out, "hli:\n");
  dump_hl_gi (out, buffer, & btngr_ns, & btn_ns);
  dump_btn_colit (out, buffer);
  dump_btnit (out, buffer, btngr_ns, btn_ns);
}

void dump_pci_packet (FILE *out, buffer_t *buffer)
{
  fprintf (out, "pci packet:\n");
  dump_pci_gi    (out, buffer);
  dump_nsml_agli (out, buffer);
  dump_hli       (out, buffer);
  skip_bits (buffer, 8 * 189);
}

void dump_dsi_gi (FILE *out, buffer_t *buffer)
{
  fprintf (out, "dsi_gi:\n");
  fprintf (out, "nv_pck_scr     0x%08x\n", get_bits (buffer, 32));
  fprintf (out, "nv_pck_lbn     0x%08x\n", get_bits (buffer, 32));
  fprintf (out, "vobu_ea        0x%08x\n", get_bits (buffer, 32));
  fprintf (out, "vobu_1stref_ea 0x%08x\n", get_bits (buffer, 32));
  fprintf (out, "vobu_2ndref_ea 0x%08x\n", get_bits (buffer, 32));
  fprintf (out, "vobu_3rdref_ea 0x%08x\n", get_bits (buffer, 32));
  fprintf (out, "vobu_vob_idn   0x%04x\n", get_bits (buffer, 16));
  reserved_bits (out, buffer, 8);
  fprintf (out, "vobu_c_idn     0x%02x\n", get_bits (buffer, 8));
  fprintf (out, "c_eltm         0x%04x\n", get_bits (buffer, 32));
}

void dump_sml_pbi (FILE *out, buffer_t *buffer)
{
  skip_bits (buffer, 8 * 148);
  /* $$$ more code needed here */
}

void dump_sml_agli (FILE *out, buffer_t *buffer)
{
  skip_bits (buffer, 8 * 54);
  /* $$$ more code needed here */
}

void dump_vobu_sri (FILE *out, buffer_t *buffer)
{
  skip_bits (buffer, 8 * 168);
  /* $$$ more code needed here */
}

void dump_synci (FILE *out, buffer_t *buffer)
{
  skip_bits (buffer, 8 * 144);
  /* $$$ more code needed here */
}

void dump_dsi_packet (FILE *out, buffer_t *buffer)
{
  fprintf (out, "dsi packet:\n");
  dump_dsi_gi    (out, buffer);
  dump_sml_pbi   (out, buffer);
  dump_sml_agli  (out, buffer);
  dump_vobu_sri  (out, buffer);
  dump_synci     (out, buffer);
  skip_bits (buffer, 8 * 471);
}

void dump_ps2_packet (FILE *out, buffer_t *buffer)
{
  u32 start_code;
  u16 length;
  u8 substream;

  start_code = get_bits (buffer, START_CODE_BITS);
  length = get_bits (buffer, PACKET_LENGTH_BITS);
  substream = get_bits (buffer, 8);
  if (substream == PS2_PCI_SUBSTREAM_ID)
    {
      if (length == PCI_BYTES)
	dump_pci_packet (out, buffer);
      else
	{
	  fprintf (out, "pci packet length is %04x, should be %04x\n",
		   length, PCI_BYTES);
	  skip_bits (buffer, 8 * (length - 1));
	}
    }
  else if (substream == PS2_DSI_SUBSTREAM_ID)
    {
      if (length == DSI_BYTES)
	dump_dsi_packet (out, buffer);
      else
	{
	  fprintf (out, "dsi packet length is %04x, should be %04x\n",
		   length, DSI_BYTES);
	  skip_bits (buffer, 8 * (length - 1));
	}
    }
  else
    {
      fprintf (out, "ps2 packet of unknown substream 0x%02x, length 0x%04x\n",
	       substream, length);
      skip_bits (buffer, 8 * (length - 1));
    }
}

void dump_video_packet (FILE *out, buffer_t *buffer)
{
  u32 start_code;
  u16 length;

  start_code = get_bits (buffer, START_CODE_BITS);
  length = get_bits (buffer, PACKET_LENGTH_BITS);

  fprintf (out, "video packet, length 0x%04x\n", length);
  skip_bits (buffer, 8 * length);
}

void dump_mpeg_audio_packet (FILE *out, buffer_t *buffer)
{
  u32 start_code;
  u16 length;

  start_code = get_bits (buffer, START_CODE_BITS);
  length = get_bits (buffer, PACKET_LENGTH_BITS);

  fprintf (out, "mpeg audio packet, start code 0x%08x, length 0x%04x\n",
	   start_code, length);
  skip_bits (buffer, 8 * length);
}

void dump_unknown_packet (FILE *out, buffer_t *buffer)
{
  u32 start_code;
  u16 length;

  start_code = get_bits (buffer, START_CODE_BITS);
  length = get_bits (buffer, PACKET_LENGTH_BITS);

  fprintf (out, "unknown packet type, start code 0x%08x, length 0x%04x\n",
	   start_code, length);
  skip_bits (buffer, 8 * length);
}

void dump_packet (FILE *out, buffer_t *buffer)
{
  u32 start_code;

  start_code = peek_bits (buffer, START_CODE_BITS);

  if (start_code == SYSTEM_HEADER_START_CODE)
    dump_system_header (out, buffer);
  else if (start_code == PRIVATE_STREAM_1_START_CODE)
    dump_ps1_packet (out, buffer);
  else if (start_code == PADDING_STREAM_START_CODE)
    dump_padding_packet (out, buffer);
  else if (start_code == PRIVATE_STREAM_2_START_CODE)
    dump_ps2_packet (out, buffer);
  else if (start_code == VIDEO_STREAM_START_CODE)
    dump_video_packet (out, buffer);
  else if (((start_code >= AUDIO_STREAM_0_START_CODE) &&
	    (start_code <= AUDIO_STREAM_7_START_CODE)) ||
	   ((start_code >= AUDIO_STREAM_8_START_CODE) &&
	    (start_code <= AUDIO_STREAM_15_START_CODE)))
    dump_mpeg_audio_packet (out, buffer);
  else
    dump_unknown_packet (out, buffer);
}

void dump_pack (FILE *out, buffer_t *buffer, u32 block)
{
  u32 scr_base, scr_extension, start_code;

#if 0
  fprintf (out, "block %lu pack header:  ", block);
#endif

  expect_bits (out, buffer, START_CODE_BITS, PACK_START_CODE, "pack start code");
  expect_bits (out, buffer, 2, 0x01, "fill bits");
  scr_base = get_bits (buffer, 3) << 30;
  expect_bits (out, buffer, 1, 0x01, "marker bit");
  scr_base |= get_bits (buffer, 15) << 15;
  expect_bits (out, buffer, 1, 0x01, "marker bit");
  scr_base |= get_bits (buffer, 15);
  expect_bits (out, buffer, 1, 0x01, "marker bit");
  scr_extension = get_bits (buffer, 9);
  expect_bits (out, buffer, 1, 0x01, "marker bit");
  expect_bits (out, buffer, 24, 0x0189c3, "mux rate");
  expect_bits (out, buffer, 5, 0x1f, "reserved");
  expect_bits (out, buffer, 3, 0x00, "pack stuffing length");

#if 1
  start_code = peek_bits (buffer, START_CODE_BITS);
  if (start_code != SYSTEM_HEADER_START_CODE)
    return;
#endif

#if 1
  fprintf (out, "block %lu pack header:  ", block);
#endif

  fprintf (out, "scr_base = 0x%08x  ", scr_base);
  fprintf (out, "scr_extension = 0x%03x\n", scr_extension);
  while (bits_avail (buffer))
    dump_packet (out, buffer);
  fprintf (out, "\n");
}

void dump_vob (FILE *out, FILE *vobfile)
{
  u32 block = 0;
  buffer_t buffer;
  while (BLOCK_SIZE == fread (& buffer.bytes [0], 1, BLOCK_SIZE, vobfile))
    {
      buffer.bit_position = 0;
      dump_pack (out, & buffer, block);
      block++;
    }
}

#if 0
int main (int argc, char *argv[])
{
  FILE *f;
  if (argc != 2)
    {
      fprintf (stderr, "usage: %s vobfile\n", argv [0]);
      exit (1);
    }
  f = fopen (argv [1], "rb");
  if (! f)
    {
      fprintf (stderr, "error opening file\n");
      exit (2);
    }
  dump_vob (stdout, f);
  fclose (f);
}
#endif
