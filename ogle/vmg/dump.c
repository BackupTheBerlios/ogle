#include <stdio.h>
#include <inttypes.h>
#define BLOCK_SIZE 2048

typedef struct
{
  uint32_t bit_position;
  uint8_t bytes [BLOCK_SIZE];
} buffer_t;

main ()
{
  buffer_t buffer;
  int i;
  unsigned int res;

  while(!feof(stdin)) {
    for(i = 0; i < 8; i++) {
      scanf("%x", &res);
      buffer.bytes[i] = res;
    }
    printf("OMS:  ");
    ifoPrintVMOP (buffer.bytes);
    printf("\n");
    printf("VDMP: ");
    dump_command(stdout, buffer);
    printf("\n\n");
  }
}

