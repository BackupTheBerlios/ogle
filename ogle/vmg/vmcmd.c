#include <stdio.h>
#include <ctype.h>
#include <inttypes.h>

#include "vmcmd.h"

typedef struct
{
  uint8_t bits[8];
  uint8_t examined[8];
} cmd_t;

static cmd_t cmd;

char *cmp_op_table[] = {
  NULL, "&", "==", "!=", ">=", ">", "<=", "<"
};
char *set_op_table[] = {
  NULL, "=", "<->", "+=", "-=", "*=", "/=", "%=", "rnd", "&=", "|=", "^="
};

char *link_table[] = {
  "LinkNoLink",  "LinkTopC",    "LinkNextC",   "LinkPrevC",
  NULL,          "LinkTopPG",   "LinkNextPG",  "LinkPrevPG",
  NULL,          "LinkTopPGC",  "LinkNextPGC", "LinkPrevPGC",
  "LinkGoUpPGC", "LinkTailPGC", NULL,          NULL,
  "RSM"
};

char *system_reg_table[] = {
  "Menu Description Language Code",
  "Audio Stream Number",
  "Sub-picture Stream Number",
  "Angle Number",
  "Title Track Number",
  "VTS Title Track Number",
  "VTS PGC Number",
  "PTT Number for One_Sequential_PGC_Title",
  "Highlighted Button Number",
  "Navigation Timer",
  "Title PGC Number for Navigation Timer",
  "Audio Mixing Mode for Karaoke",
  "Country Code for Parental Management",
  "Parental Level",
  "Player Configurations for Video",
  "Player Configurations for Audio",
  "Initial Language Code for Audio",
  "Initial Language Code Extension for Audio",
  "Initial Language Code for Sub-picture",
  "Initial Language Code Extension for Sub-picture",
  "Player Regional Code",
  "Reserved 21",
  "Reserved 22",
  "Reserved 23"
};

char *system_reg_abbr_table[] = {
  NULL,
  "ASTN",
  "AGLN",
  "TTN",
  "VTS_TTN",
  "TT_PGCN",
  "PTTN",
  "HL_BTNN"
};
    


uint32_t bits (int byte, int bit, int count)
{
  uint32_t val = 0;
  int bit_mask;
  while (count--)
    {
      if(bit > 7) {
        bit = 0;
        byte++;
      }
      bit_mask = 0x01 << (7-bit);
      val <<= 1;
      if ((cmd.bits [byte]) & bit_mask)
	val |= 1;
      cmd.examined [byte] |= bit_mask;
      bit++;
    }
  return (val);
}

static void
print_system_reg(uint16_t reg) {
  fprintf(stderr, system_reg_table[reg]);
}

static void
print_reg(uint8_t reg) {
  if(reg&0x80) {
    print_system_reg(reg&0x1f);
  } else {
    fprintf(stderr, "g[");
    fprintf(stderr, "%" PRIu8 "]", reg&0x7f);
  }
}

static void
print_cmp_op(uint8_t op) {
  fprintf(stderr, " %s ", cmp_op_table[op]);
}

static void
print_set_op(uint8_t op) {
  char *set_op = set_op_table[op];
  fprintf(stderr, " %s ", set_op ? set_op : " ?? ");
}

static void
print_reg_or_data(int immediate, int byte) {
  if (immediate) {
    int i = bits(byte,0,16);
    fprintf(stderr, "0x%x", i);
    if( isprint(i & 0xff) && isprint((i>>8) & 0xff) )
      fprintf(stderr, " (\"%c%c\")", (char)((i>>8) & 0xff), (char)(i & 0xff));
  } else {
    print_reg(bits(byte+1,0,8));
  }
}

static void
print_if_version_1() {
  uint8_t op = (bits(1,1,3));
  if(op) {
    fprintf(stderr, "if (");
    print_reg(bits(3,0,8));
    print_cmp_op(op);
    print_reg_or_data(bits(1,0,1), 4);
    fprintf(stderr, ") ");
  }
}

static void
print_special_instruction() {
  uint8_t op = bits(1,4,4);
  
  switch(op) {
    case 0: // NOP
      fprintf(stderr, "Nop");
      break;
    case 1: // Goto line
      fprintf(stderr, "Goto %" PRIu8, bits(7,0,8));
      break;
    case 2: // Break
      fprintf(stderr, "Break");
      break;
    case 3: // Parental level
      fprintf(stderr, "SetTmpPML %" PRIu8 ", Goto %" PRIu8, bits(6,4,4), bits(7,0,8));
      break;
    default:
      fprintf(stderr, "WARNING: Unknown special instruction (%i)", bits(1,4,4));
  }
}

static void
print_linksub_instruction() {
  int linkop = bits(7,3,5);
  int button = bits(6,0,6);
  
  if(linkop <= 0x10 && link_table[linkop] != NULL)
    fprintf(stderr, "%s (button %" PRIu8 ")", link_table[linkop], button);
  else
    fprintf(stderr, "WARNING: Unknown link instruction (%i)", linkop);
}

static void
print_link_instruction() {
  uint8_t op = bits(1,4,4);
  uint8_t button = bits(6,0,6);
  
  switch(op) {
    case 1:
      print_linksub_instruction();
      break;
    case 4:
      fprintf(stderr, "LinkPGCN %" PRIu16, bits(6,1,15));
      break;
    case 5:
      fprintf(stderr, "LinkPTT %" PRIu16 " (button %" PRIu8 ")", bits(6,6,10), button);
      break;
    case 6:
      fprintf(stderr, "LinkPGN %" PRIu8 " (button %" PRIu8 ")", bits(7,1,7), button);
      break;
    case 7:
      fprintf(stderr, "LinkCN %" PRIu8 " (button %" PRIu8 ")", bits(7,0,8), button);
      break;
    default:
      fprintf(stderr, "WARNING: Unknown link instruction");
  }
}

static void
print_if_version_2 () {
  uint8_t op = (bits(1,1,3));
  
  if(op) {
    fprintf(stderr, "if (");
    print_reg(bits(6,0,8));
    print_cmp_op(op);
    print_reg(bits(7,0,8));
    fprintf(stderr, ") ");
  }
}

static void
print_jump_instruction () {
  switch(bits(1,4,4)) {
    case 1:
      fprintf(stderr, "Exit");
      break;
    case 2:
      fprintf(stderr, "JumpTT %" PRIu8, bits(5,1,7));
      break;
    case 3:
      fprintf(stderr, "JumpVTS_TT %" PRIu8, bits(5,1,7));
      break;
    case 5:
      fprintf(stderr, "JumpVTS_PTT %" PRIu8 ":%" PRIu16, 
          bits(5,1,7), bits(2,6,10));
      break;
    case 6:
      switch(bits(5,0,2)) {
        case 0:
          fprintf(stderr, "JumpSS FP");
          break;
        case 1:
          fprintf(stderr, "JumpSS VMGM (menu %" PRIu8 ")", bits(5,4,4));
          break;
        case 2:
          fprintf(stderr, "JumpSS VTSM (vts %" PRIu8 ", title %" PRIu8 
                 ", menu %" PRIu8 ")",
                 bits(4,0,8), bits(3,0,8), bits(5,4,4));
          break;
        case 3:
          fprintf(stderr, "JumpSS VMGM (pgc %" PRIu8 ")", bits(2,1,15));
          break;
        }
      break;
    case 8:
      switch(bits(5,0,2)) {
        case 0:
          fprintf(stderr, "CallSS FP (rsm_cell %" PRIu8 ")",
              bits(4,0,8));
          break;
        case 1:
          fprintf(stderr, "CallSS VMGM (menu %" PRIu8 ", rsm_cell %" PRIu8 ")", 
              bits(5,4,4), bits(4,0,8));
          break;
        case 2:
          fprintf(stderr, "CallSS VTSM (menu %" PRIu8 ", rsm_cell %" PRIu8 ")", 
              bits(5,4,4), bits(4,0,8));
          break;
        case 3:
          fprintf(stderr, "CallSS VMGM (pgc %" PRIu8 ", rsm_cell %" PRIu8 ")", 
              bits(2,1,15), bits(4,0,8));
          break;
      }
      break;
    default:
      fprintf(stderr, "WARNING: Unknown Jump/Call instruction");
  }
}

static void
print_reg_or_data_2(int immediate, int byte) {
  if (immediate) {
    fprintf(stderr, "0x%x", bits(byte,1,7));
  } else {
    fprintf(stderr, "g[%" PRIu8 "]", bits(byte,4,4));
  }
}

static void
print_system_set () {
  int i;
  switch(bits(0,4,4)) {
    case 1: // Set system reg 1 &| 2 &| 3 (Audio, Subp. Angle)
      for(i = 1; i <= 3; i++) {
        if(bits(2+i,0,1)) {
          print_system_reg(i);
          fprintf(stderr, " = ");
          print_reg_or_data_2(bits(0,3,1), 2+i);
          fprintf(stderr, " ");
        }
      }
      break;
    case 2: // Set system reg 9 & 10 (Navigation timer, Title PGC number)
      print_system_reg(9);
      fprintf(stderr, " = ");
      print_reg_or_data(bits(0,3,1), 2);
      fprintf(stderr, " ");
      print_system_reg(10);
      fprintf(stderr, " = %" PRIu8, bits(5,0,8)); // ??
      break;
    case 3: // Mode: Counter / Register + Set
      fprintf(stderr, "SetMode ");
      if (bits(5,0,1))
	fprintf(stderr, "Counter ");
      else
	fprintf(stderr, "Register ");
      print_reg(bits(5,4,4));
      print_set_op(0x1); // =
      print_reg_or_data(bits(0,3,1), 2);
      break;
    case 6: // Set system reg 8 (Highlighted button)
      print_system_reg(8);
      if (bits(0,3,1)) // immediate
        fprintf(stderr, " = 0x%x (button no %d)", bits(4,0,16), bits(4,0,6));
      else
        fprintf(stderr, " = g[%" PRIu8 "]", bits(5,4,4));
      break;
  default:
      fprintf(stderr, "WARNING: Unknown system set instruction (%i)", bits(0,4,4));
  }
  if(bits(1,4,4)) {
    fprintf(stderr, ", ");
    print_link_instruction();
  }
}

static void
print_if_version_3 () {
  uint8_t op = (bits(1,1,3));
  if(op) {
    fprintf(stderr, "if (");
    print_reg(bits(2,4,4));
    print_cmp_op(op);
    print_reg_or_data(bits(1,0,1), 6);
    fprintf(stderr, ") ");
  }
}

static void
print_set () {
  uint8_t set_op = bits(0,4,4);
  print_reg(bits(3,0,8));
  print_set_op(set_op);
  print_reg_or_data(bits(0,3,1), 4);
  if(bits(1,4,4)) {
    fprintf(stderr, ", ");
    print_link_instruction();
  }
}

static void
print_set2 () {
  uint8_t set_op = bits(0,4,4);
  
  print_reg(bits(1,4,4));
  print_set_op(set_op);
  print_reg_or_data(bits(0,3,1), 2);
}

static void
print_if_version_4 () {
  uint8_t op = (bits(1,1,3));
  
  if(op) {
    fprintf(stderr, "if (");
    print_reg(bits(1,4,4));
    print_cmp_op(op);
    print_reg_or_data(bits(1,0,1), 4);
    fprintf(stderr, ") ");
  }
}

void
vmcmd(uint8_t *bytes)  {
  int i, extra_bits;
  for(i=0;i<8;i++) {
    cmd.bits[i] = bytes[i];
    cmd.examined[i] = 0;
  }

  switch(bits(0,0,3)) { /* three first bits */
    case 0: // special instructions
      print_if_version_1();
      print_special_instruction();
      break;
    case 1: // Link/jump instructions
      if(bits(0,3,1)) {
        print_if_version_2();
        print_jump_instruction();
      } else {
        print_if_version_1();
        print_link_instruction();
      }
      break;
    case 2: // System set instructions
      print_if_version_2();
      print_system_set();
      break;
    case 3: // Set instructions
      print_if_version_3();
      print_set();
      break;
    case 4: // Set, Compare -> LinkSIns instructions
      print_set2();
      fprintf(stderr, ", ");
      print_if_version_4();
      print_linksub_instruction();
      break;
    case 5: // Compare -> (Set and LinkSIns) instructions
      print_if_version_4();
      fprintf(stderr, "{ ");
      print_set2();
      fprintf(stderr, ", ");
      print_linksub_instruction();
      fprintf(stderr, " }");
      break;
    case 6: // Compare -> Set, always LinkSIns instructions
      print_if_version_4();
      fprintf(stderr, "{ ");
      print_set2();
      fprintf(stderr, " } ");
      print_linksub_instruction();
      break;
    default:
      fprintf(stderr, "WARNING: Unknown instruction type (%i)", bits(0,0,3)); 
  }
  // Check if there are bits not yet examined

  extra_bits = 0;
  for (i = 0; i < 8; i++)
    if (cmd.bits [i] & ~ cmd.examined [i])
    {
      extra_bits = 1;
      break;
    }
  if (extra_bits)
  {
    fprintf(stderr, " [WARNING, unknown bits:");
    for (i = 0; i < 8; i++)
      fprintf(stderr, " %02x", cmd.bits [i] & ~ cmd.examined [i]);
    fprintf(stderr, "]");
  }
}

void vmPrint_CMD(int row, vm_cmd_t *command) {
  int i;

  fprintf(stderr, "(%03d) ", row + 1);
  for(i = 0; i < 8; i++)
    fprintf(stderr, "%02x ", command->bytes[i]);
  fprintf(stderr, "| ");

  vmcmd(&command->bytes[0]);
  fprintf(stderr, "\n");
}
