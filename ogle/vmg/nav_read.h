#ifndef __NAV_READ_H__
#define __NAV_READ_H__

#include "nav.h"

void read_pci_packet(pci_t *pci, char *buffer, int len);
void read_dsi_packet(dsi_t *dsi, char *buffer, int len);

#endif /* __NAV_READ_H__ */
