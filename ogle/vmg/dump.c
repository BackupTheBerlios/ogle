/* Ogle - A video player
 * Copyright (C) 2000 Björn Englund, Håkan Hjort
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
#include <inttypes.h>

#include "vmcmd.h"
#include "decoder.h"

int 
main(int argc, char *argv[])
{
  vm_cmd_t cmd;
  int i;
  registers_t state;
  link_t return_values;

  memset(&state, 0, sizeof(registers_t));

  while(!feof(stdin)) {
    printf("   #   ");
    for(i = 0; i < 24; i++)
      printf(" %2d |", i);
    printf("\nSRPMS: ");
    for(i = 0; i < 24; i++)
      printf("%04x|", state.SPRM[i]);
    printf("\nGRPMS: ");
    for(i = 0; i < 16; i++)
      printf("%04x|", state.GPRM[i]);
    printf("\n");

    for(i = 0; i < 8; i++) {
      unsigned int res;
      if(feof(stdin))
	exit(0);
      scanf("%x", &res);
      cmd.bytes[i] = res;
    }
    
    eval(&cmd, 1, &state, &return_values);
    
    printf("mnemonic:    ");
    vmcmd(cmd.bytes);
    printf("\n");
  }
  exit(0);
}

