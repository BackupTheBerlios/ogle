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
 * $Id: nav.c,v 1.4 2001/02/23 10:19:28 d95hjort Exp $
 */

#include <stdio.h>
#include <inttypes.h>

#include "ifo.h" // command_data_t
#include "nav.h"
#include "nav_read.h"
#include "nav_print.h"

#define START_CODE_BITS 32
#define PACKET_LENGTH_BITS 16

/* these lengths in bytes, exclusive of start code and length */
#define SYSTEM_HEADER_BYTES 0x12
#define PCI_BYTES 0x3d4
#define DSI_BYTES 0x3fa

#define COMMAND_BYTES 8


uint32_t get_bits (buffer_t *buffer, uint32_t count)
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


uint32_t peek_bits (buffer_t *buffer, uint32_t count)
{
  unsigned long val = 0;
  uint32_t count2 = count;

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


void unget_bits (buffer_t *buffer, uint32_t count)
{
  if (buffer->bit_position < count)
    {
      fprintf (stderr, "attempt to unget too many bits\n");
      exit (2);
    }
  buffer->bit_position -= count;
}


void skip_bits (buffer_t *buffer, uint32_t count)
{
  if ((buffer->bit_position + count) > (BLOCK_SIZE * 8))
    {
      fprintf (stderr, "attempt to skip past end of block\n");
      exit (2);
    }
  buffer->bit_position += count;
}


void expect_bits (FILE *out, buffer_t *buffer, uint32_t count, uint32_t expected, char *description)
{
  uint32_t val;
  val = get_bits (buffer, count);
  if (val != expected)
    fprintf (out, "%s is 0x%x, should be 0x%x\n", description, val, expected);
}


void reserved_bits (FILE *out, buffer_t *buffer, uint32_t count)
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










void parse_packet (FILE *out, buffer_t *buffer, pci_t *pci, dsi_t *dsi)
{
  uint32_t start_code;

  start_code = peek_bits (buffer, START_CODE_BITS);
  if (start_code == PRIVATE_STREAM_2_START_CODE)
    {
      uint32_t start_code;
      uint16_t length;
      uint8_t substream;
      
      start_code = get_bits (buffer, START_CODE_BITS);
      length = get_bits (buffer, PACKET_LENGTH_BITS);
      substream = get_bits (buffer, 8);
      if (substream == PS2_PCI_SUBSTREAM_ID)
	{
	  if (length == PCI_BYTES)
	    read_pci_packet (pci, out, buffer);
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
	    read_dsi_packet (dsi, out, buffer);
	  else
	    {
	      fprintf (out, "dsi packet length is %04x, should be %04x\n",
		       length, DSI_BYTES);
	      skip_bits (buffer, 8 * (length - 1));
	    }
	}
      else
	{
	  fprintf (out, "ps2 packet of unknown substream 0x%02x," 
		   " length 0x%04x\n", substream, length);
	  skip_bits (buffer, 8 * (length - 1));
	}
    }
  else
    {
#if 0
      uint32_t start_code;
      uint16_t length;
      
      start_code = get_bits (buffer, START_CODE_BITS);
      length = get_bits (buffer, PACKET_LENGTH_BITS);
      
      //fprintf (out, "packet, start code 0x%08x, length 0x%04x\n",
      //       start_code, length);
      skip_bits (buffer, 8 * length);
#else /* Avoid parsing block that aren't nav blocks (avoid scambled blocks) */
      uint32_t start_code;
      uint16_t length;

      start_code = get_bits (buffer, START_CODE_BITS);
      length = get_bits (buffer, PACKET_LENGTH_BITS);
      if (start_code == SYSTEM_HEADER_START_CODE)
	skip_bits (buffer, 8 * length);
      else
	skip_bits (buffer, BLOCK_SIZE * 8 - buffer->bit_position);
#endif
    }
}

void parse_pack (FILE *out, buffer_t *buffer, pci_t *pci, dsi_t *dsi)
{
  uint32_t scr_base, scr_extension;

  expect_bits (out, buffer, START_CODE_BITS, PACK_START_CODE, 
	       "pack start code");
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

#if 0 // Humm...
  start_code = peek_bits (buffer, START_CODE_BITS);
  if (start_code != SYSTEM_HEADER_START_CODE)
    return;
#endif

  //fprintf (out, "scr_base = 0x%08x  ", scr_base);
  //fprintf (out, "scr_extension = 0x%03x\n", scr_extension);
  while (bits_avail (buffer)) {
    parse_packet (out, buffer, pci, dsi);
  }
  //fprintf (out, "\n");
}

int demux_data(char *file_name, int start_sector, 
	       int last_sector, command_data_t *cmd) {
  FILE *vobfile;
  uint32_t block = 0;
  buffer_t buffer;
  pci_t pci;
  dsi_t dsi;
  int res = -1;
  
  memset(&pci, 0, sizeof(pci_t));
  memset(&dsi, 0, sizeof(dsi_t));
  
  vobfile = fopen (file_name, "rb");
  if (!vobfile) {
    fprintf (stderr, "error opening file\n");
    return 0;
  }
  if (fseek (vobfile, start_sector * 2048, SEEK_SET)) {
    fprintf (stderr, "error seeking in file\n");
    return 0;
  }
  
  while (start_sector + block <= last_sector && 
	BLOCK_SIZE == fread (&buffer.bytes [0], 1, BLOCK_SIZE, vobfile)) {
    buffer.bit_position = 0;
    parse_pack (stdout, &buffer, &pci, &dsi);
  
    if (pci.hli.hl_gi.hli_ss & 0x03) {
      print_pci_packet (stdout, &pci);
      
      fprintf (stdout, "Menu selection: "); 
      scanf("%i", &res);
      res = (res > 36) ? 0 : (res < 0) ? 0 : res;
      if(res)
	memcpy(cmd, pci.hli.btnit[res-1].cmd, sizeof (command_data_t));
      break;
    }
  
    block++;
  }
  fclose (vobfile);
  
  if(res == -1) {
    print_pci_packet (stdout, &pci);
    print_dsi_packet (stdout, &dsi);
    res = 0;
  }

  return res;
}

