#include <stdio.h>
#include <inttypes.h>

#include "ifo.h" // vm_cmd_t
#include "vmcmd.h"
#include "decoder.h"

int 
main(int argc, char *argv[])
{
  vm_cmd_t cmd;
  int i;
  unsigned int res;
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

