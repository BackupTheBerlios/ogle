#ifndef __NAV_READ_H__
#define __NAV_READ_H__

#include <stdio.h>
#include "nav.h"

void read_pci_packet (pci_t *pci, FILE *out, buffer_t *buffer);
void read_dsi_packet (dsi_t *dsi, FILE *out, buffer_t *buffer);

#endif /* __NAV_READ_H__ */
