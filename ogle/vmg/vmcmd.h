#ifndef __VMCMD_H__
#define __VMCMD_H__

#include <inttypes.h>
#include <dvdread/ifo_types.h> // Only for vm_cmd_t 

void vmcmd(uint8_t *bytes);
void vmPrint_CMD(int row, vm_cmd_t *command);

#endif /* __VMCMD_H__ */
