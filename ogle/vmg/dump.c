#include <stdio.h>
#include <inttypes.h>
#include "decoder.h"
#define BLOCK_SIZE 2048

typedef struct
{
  uint32_t bit_position;
  uint8_t bytes [BLOCK_SIZE];
} buffer_t;

main ()
{
  uint8_t buf[8];
  buffer_t buffer;
  int i;
  unsigned int res;
  state_t state;
  link_t return_values;

  bzero(&state, sizeof(state_t));

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
      scanf("%x", &res);
      buffer.bytes[i] = res;
      buf[i] = res;
    }
    eval (&buf, 1, &state, &return_values);
    printf("OMS:  ");
    ifoPrintVMOP (buffer.bytes);
    printf("\n");
    printf("n:    ");
    vmcmd(buffer.bytes);
    printf("\n");
    printf("VDMP: ");
    dump_command(stdout, buffer);
    printf("\n\n");
  }
}

