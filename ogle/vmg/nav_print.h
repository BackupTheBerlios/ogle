#ifndef __NAV_PRINT_H__
#define __NAV_PRINT_H__

#include <stdio.h>

#include "nav.h"

void print_pci_packet (FILE *out, pci_t *pci);
void print_dsi_packet (FILE *out, dsi_t *dsi);

#endif /* __NAV_PRINT_H__ */
