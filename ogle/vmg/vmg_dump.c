/* SKROMPF - A video player
 * Copyright (C) 2000 Björn Englund, Håkan Hjort, Martin Norbäck
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

#include "nav.h"
#include "nav_read.h"
#include "nav_print.h"

int debug = 8;
char *program_name;


void parse_vmg_data (FILE *in)
{
  pci_t pci;
  dsi_t dsi;
  uint8_t substream;
  buffer_t buffer;
  buffer.bit_position = BLOCK_SIZE * 8;
  
  while(1) {
    int left = BLOCK_SIZE - buffer.bit_position/8;
    
    if(buffer.bit_position % 8 != 0)
      abort();
    memmove(&buffer.bytes[0], &buffer.bytes[buffer.bit_position/8], left);
    if(fread(&buffer.bytes[left], 1, BLOCK_SIZE - left, in) != BLOCK_SIZE - left) {
      perror("reading nav data");
      exit(1);
    }
    buffer.bit_position = 0;
    
    substream = get_bits (&buffer, 8);
    if (substream == PS2_PCI_SUBSTREAM_ID) {
      read_pci_packet(&pci, stdout, &buffer);
      //print_pci_packet(stdout, &pci);
    }
    else if (substream == PS2_DSI_SUBSTREAM_ID) {
      int i, j;
      read_dsi_packet(&dsi, stdout, &buffer);
      print_dsi_packet(stdout, &dsi);
#if 1
      for(i = 0; i < 148; i++) {
	fprintf(stdout, "%02x ", dsi.sml_pbi.unknown[i]);
	if(i % 20 == 19)
	  fprintf(stdout, "\n");
      }
      if(i % 20 != 19)
	fprintf(stdout, "\n");
#endif
#if 0
      for(i = 0; i < 9; i++) {
	for(j = 0; j < 6; j++)
	  fprintf(stdout, "%02x ", dsi.sml_agli.unknown[i][j]);
	fprintf(stdout, "\n");
      }
#endif
#if 0
      fprintf(stdout, "%08x\n", dsi.vobu_sri.unknown1);
      for(i = 0; i < 20; i++) {
	fprintf(stdout, "%08x ", dsi.vobu_sri.unknown2[i]);
	if(i % 5 == 4)
	  fprintf(stdout, "\n");
      }
      fprintf(stdout, "--\n");
      for(i = 0; i < 20; i++) {
	fprintf(stdout, "%08x ", dsi.vobu_sri.unknown3[i]);
	if(i % 5 == 4)
	  fprintf(stdout, "\n");
      }
      fprintf(stdout, "%08x\n", dsi.vobu_sri.unknown4);
#endif
#if 0
      fprintf(stdout, "%04x\n", dsi.synci.offset);
      for(i = 0; i < 142; i++) {
	fprintf(stdout, "%02x ", dsi.synci.unknown[i]);
	if(i % 20 == 19)
	  fprintf(stdout, "\n");
      }
      if(i % 20 != 19)
	fprintf(stdout, "\n");
#endif
    }
    else {
      fprintf (stdout, "ps2 packet of unknown substream 0x%02x", substream);
      exit(1);
    }
  }
}



void usage()
{
  fprintf(stderr, "Usage: %s  [-d <debug_level>] file\n", 
          program_name);
}

int main(int argc, char *argv[])
{
  int c;
  FILE *infile;
  program_name = argv[0];
  
  /* Parse command line options */
  while ((c = getopt(argc, argv, "d:h?")) != EOF) {
    switch (c) {
    case 'd':
      debug = atoi(optarg);
      break;
    case 'h':
    case '?':
      usage();
      return 1;
    }
  }

  if(argc - optind != 1){
    usage();
    return 1;
  }

  infile = fopen(argv[optind], "rb");
  if(!infile) {
    fprintf (stderr, "error opening file\n");
    exit(1);
  }
  parse_vmg_data(infile);
  
  exit(0);
}
