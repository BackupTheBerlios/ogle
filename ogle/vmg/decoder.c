#include <stdio.h>
#include <inttypes.h>

#include "ifo.h" // vm_cmd_t
#include "decoder.h"

typedef struct
{
  uint8_t bits[8];
  uint8_t examined[8];
} cmd_t;

static cmd_t cmd;
static registers_t *state;

/* Get count bits of command from byte and bit position. */
static uint32_t bits(int byte, int bit, int count) {
  uint32_t val = 0;
  int bit_mask;
  
  while(count--) {
    if(bit > 7) {
      bit = 0;
      byte++;
    }
    bit_mask = 0x01 << (7 - bit);
    val <<= 1;
    if(cmd.bits[byte] & bit_mask)
      val |= 1;
    cmd.examined[byte] |= bit_mask;
    bit++;
  }
  return val;
}

/* Eval register code, can either be system or general register.
   SXXX_XXXX, where S is 1 if it is system register. */
static uint16_t eval_reg(uint8_t reg) {
  if(reg & 0x80) {
    return state->SPRM[reg & 0x1f]; // FIXME max 24 not 32
  } else {
    return state->GPRM[reg & 0x0f];
  }
}

/* Eval register or immediate data.
   AAAA_AAAA BBBB_BBBB, if immediate use all 16 bits for data else use
   lower eight bits for the system or general purpose register. */
static uint16_t eval_reg_or_data(int imm, int byte) {
  if(imm) { // immediate
    return bits(byte, 0, 16);
  } else {
    return eval_reg(bits(byte + 1, 0, 8));
  }
}

/* Compare data using operation, return result from comparison. */
static bool eval_compare(uint8_t operation, uint16_t data1, uint16_t data2) {
  switch(operation) {
    case 1:
      return data1 & data2;
    case 2:
      return data1 == data2;
    case 3:
      return data1 != data2;
    case 4:
      return data1 >= data2;
    case 5:
      return data1 >  data2;
    case 6:
      return data1 <= data2;
    case 7:
      return data1 <  data2;
  }
  fprintf(stderr,"eval_compare: should not happen\n");
  //exit(1);
  return 0;
}


/* Evaluate the first version of if.
   Has comparison data in byte 3 and 4-5 (immediate or register) */
static bool eval_if_version_1() {
  uint8_t op = bits(1, 1, 3);
  if(op) {
    return eval_compare(op, eval_reg(bits(3, 0, 8)), 
                            eval_reg_or_data(bits(1, 0, 1), 4));
  }
  return 1;
}

/* Evaluate special instruction....this could mean goto
   Return new row number; 0 if no new row, 256 if Break or -1 if UNKNOWN. */
static int eval_special_instruction(bool cond) {
  int line, level;
  
  switch(bits(1, 4, 4)) {
    case 0: // NOP
      line = 0;
      return cond ? line : 0;
    case 1: // Goto line
      line = bits(7, 0, 8);
      return cond ? line : 0;
    case 2: // Break
      // max number of rows < 256, so we will end this set
      line = 256;
      return cond ? 256 : 0;
    case 3: // Set temporary parental level and goto
      line = bits(7, 0, 8); 
      level = bits(6, 4, 4);
      if(cond) {
	// This always succeeds now, if we want real parental protection
	// we need to ask the user and have passwords and stuff.
	state->SPRM[13] = level;
      }
      return cond ? line : 0;
  }
  return -1;
}

/* Evaluate link by subinstruction.
   Return 1 if link, or 0 if no link
   Actual link instruction is in return_values parameter */
static bool eval_link_subins(bool cond, link_t *return_values) {
  uint16_t button = bits(6, 0, 6);
  uint8_t  linkop = bits(7, 3, 5);
  
  if(linkop > 0x10) {
    // Unknown Link by Sub-Instruction command
    return 0;
  }
  // Assumes that the link_cmd_t enum has the same values as the LinkSIns codes.
  return_values->command = linkop;
  return_values->data1 = button;
  return cond;
}


/* Evaluate link instruction.
   Return 1 if link, or 0 if no link
   Actual link instruction is in return_values parameter */
static bool eval_link_instruction(bool cond, link_t *return_values) {
  uint8_t op = bits(1, 4, 4);
  
  switch(op) {
    case 1:
	return eval_link_subins(cond, return_values);
    case 4:
	return_values->command = LinkPGCN;
	return_values->data1   = bits(6, 1, 15);
	return cond;
    case 5:
	return_values->command = LinkPTTN;
	return_values->data1 = bits(6, 6, 10);
	return_values->data2 = bits(6, 0, 6);
	return cond;
    case 6:
	return_values->command = LinkPGN;
	return_values->data1 = bits(7, 1, 7);
	return_values->data2 = bits(6, 0, 6);
	return cond;
    case 7:
	return_values->command = LinkCN;
	return_values->data1 = bits(7, 0, 8);
	return_values->data2 = bits(6, 0, 6);
	return cond;
  }
  return 0;
}

/* Evaluate if version 2.
   This version only compares register which are in byte 6 and 7 */
static bool eval_if_version_2() {
  uint8_t op = bits(1, 1, 3);
  if(op) {
    return eval_compare(op, eval_reg(bits(6, 0, 8)), 
                            eval_reg(bits(7, 0, 8)));
  }
  return 1;
}

/* Evaluate a jump instruction.
   returns 1 if jump or 0 if no jump
   actual jump instruction is in return_values parameter */
static bool eval_jump_instruction(bool cond, link_t *return_values) {
  
  switch(bits(1, 4, 4)) {
    case 1:
      return_values->command = Exit;
      return cond;
    case 2:
      return_values->command = JumpTT;
      return_values->data1 = bits(5, 1, 7);
      return cond;
    case 3:
      return_values->command = JumpVTS_TT;
      return_values->data1 = bits(5, 1, 7);
      return cond;
    case 5:
      return_values->command = JumpVTS_PTT;
      return_values->data1 = bits(5, 1, 7);
      return_values->data2 = bits(2, 6, 10);
      return cond;
    case 6:
      switch(bits(5,0,2)) {
        case 0:
          return_values->command = JumpSS_FP;
          return cond;
        case 1:
          return_values->command = JumpSS_VMGM_MENU;
          return_values->data1 =  bits(5, 4, 4);
          return cond;
        case 2:
          return_values->command = JumpSS_VTSM;
          return_values->data1 =  bits(4, 0, 8);
          return_values->data2 =  bits(3, 0, 8);
          return_values->data3 =  bits(5, 4, 4);
          return cond;
        case 3:
          return_values->command = JumpSS_VMGM_PGC;
          return_values->data1 =  bits(2, 1, 15);
          return cond;
        }
      break;
    case 8:
      switch(bits(5,0,2)) {
        case 0:
          return_values->command = CallSS_FP;
          return_values->data1 = bits(4, 0, 8);
          return cond;
        case 1:
          return_values->command = CallSS_VMGM_MENU;
          return_values->data1 = bits(5, 4, 4);
          return_values->data2 = bits(4, 0 ,8);
          return cond;
        case 2:
          return_values->command = CallSS_VTSM;
          return_values->data1 = bits(5, 4, 4);
          return_values->data2 = bits(4, 0, 8);
          return cond;
        case 3:
          return_values->command = CallSS_VMGM_PGC;
          return_values->data1 = bits(2, 1, 15);
          return_values->data2 = bits(4, 0, 8);
          return cond;
      }
      break;
  }
  return 0;
}

/* Evaluates gprm or data depending on bit, data is in byte n */
uint16_t eval_reg_or_data_2(int imm, int byte) {
  if(imm) /* immediate */
    return bits(byte, 1, 7);
  else
    return state->GPRM[bits(byte, 4, 4)];
}

/* Evaluate a set sytem register instruction 
   May contain a link so return the same as eval_link */
static bool eval_system_set(int cond, link_t *return_values) {
  int i;
  uint16_t data, data2;
  switch(bits(0, 4, 4)) {
    case 1: // Set system reg 1 &| 2 &| 3 (Audio, Subp. Angle)
      for(i = 1; i <= 3; i++) {
        if(bits(2 + i, 0, 1)) {
          data = eval_reg_or_data_2(bits(0, 3, 1), 2 + i);
          if(cond) {
            state->SPRM[i] = data;
          }
        }
      }
      break;
    case 2: // Set system reg 9 & 10 (Navigation timer, Title PGC number)
      data = eval_reg_or_data(bits(0, 3, 1), 2);
      data2 = bits(5, 0, 8); // ?? size
      if(cond) {
	state->SPRM[9] = data; // time
	state->SPRM[10] = data2; // pgcN
      }
      break;
    case 3: // Mode: Counter / Register + Set
      data = eval_reg_or_data(bits(0, 3, 1), 2);
      data2 = bits(5, 4, 4);
      if(bits(5, 0, 1)) {
	fprintf(stderr, "Detected SetGPRMMD Counter!! This is unsupported.\n");
	//exit(-1);
      } else {
	;
      }
      if(cond) {
	state->GPRM[data2] = data;
      }
      break;
    case 6: // Set system reg 8 (Highlighted button)
      data = eval_reg_or_data(bits(0, 3, 1), 4); // Not system reg!!
      if(cond) {
	state->SPRM[8] = data;
      }
      break;
  }
  if(bits(1, 4, 4)) {
    return eval_link_instruction(cond, return_values);
  }
  return 0;
}

/* Evaluate if version 3 
   data is in bit 2 and 6-7*/
static bool eval_if_version_3() {
  uint8_t op = bits(1, 1, 3);
  if(op) {
    return eval_compare(op, eval_reg(bits(2, 0, 8)), 
                            eval_reg_or_data(bits(1, 0, 1), 6));
  }
  return 1;
}

/* Evaluate set operation
   Sets the register given to the value indicated by op and data.
   For the swap case the contents of reg is stored in reg2.
*/
static void eval_set_op(int op, int reg, int reg2, int data) {
  switch(op) {
    case 1:
      state->GPRM[reg] = data;
      break;
    case 2: /* SPECIAL CASE - SWAP! */
      state->GPRM[reg2] = state->GPRM[reg];
      state->GPRM[reg] = data;
      break;
    case 3:
      state->GPRM[reg] += data;
      break;
    case 4:
      state->GPRM[reg] -= data;
      break;
    case 5:
      state->GPRM[reg] *= data;
      break;
    case 6:
      state->GPRM[reg] /= data;
      break;
    case 7:
      state->GPRM[reg] %= data;
      break;
    case 8: /* SPECIAL CASE - RND! */
      state->GPRM[reg] += data; // TODO FIXME
      break;
    case 9:
      state->GPRM[reg] &= data;
      break;
    case 10:
      state->GPRM[reg] |= data;
      break;
    case 11:
      state->GPRM[reg] ^= data;
      break;
  }
}

/* Evaluate set instruction
   May contain a link, so it returns the same as the eval_link */
static bool eval_set(int cond, link_t *return_values) {
  uint8_t  op   = bits(0, 4, 4);
  uint8_t  reg  = bits(3, 4, 4); // Erhumm..
  uint8_t  reg2 = bits(5, 4, 4);
  uint16_t data = eval_reg_or_data(bits(0, 3, 1), 4);

  if(cond) {
    eval_set_op(op, reg, reg2, data);
  }
  if(bits(1,4,4)) {
    return eval_link_instruction(cond, return_values);
  }
  return 0;
}


/* Evaluate if version 4 
   reg is in byte 1 and data is in byte 4-5 */
static bool eval_if_version_4() {
  uint8_t op = bits(1, 1, 3);
  if(op) {
    return eval_compare(op, eval_reg(bits(1, 4, 4)), 
                            eval_reg_or_data(bits(1, 0, 1), 4));
  }
  return 1;
}

/* Evaluate set instruction for Compare/Set/Link */
static void eval_set_sub(int cond) {
  uint8_t  op   = bits(0, 4, 4);
  uint8_t  reg  = bits(1, 4, 4);
  uint8_t  reg2 = bits(3, 4, 4); // Erhumm..
  uint16_t data = eval_reg_or_data(bits(0, 3, 1), 2);

  if(cond) {
    eval_set_op(op, reg, reg2, data);
  }
}



/* Evaluate a command
   returns row number of goto, 0 if no goto, -1 if link.
   Link command in return_values */
static int eval_command(uint8_t *bytes, link_t *return_values) {
  int i, extra_bits;
  int cond, res = 0;

  for(i = 0; i < 8; i++) {
    cmd.bits[i] = bytes[i];
    cmd.examined[i] = 0;
  }

  switch(bits(0, 0, 3)) { /* three first bits */
    case 0: // Special instructions
      cond = eval_if_version_1();
      res = eval_special_instruction(cond);
      if(res == -1) {
	fprintf(stderr, "Unknown Instruction!\n");
	//exit(0);
      }
      break;
    case 1: // Link/jump instructions
      if(bits(0, 3, 1)) {
        cond = eval_if_version_2();
        res = eval_jump_instruction(cond, return_values);
      } else {
        cond = eval_if_version_1();
        res = eval_link_instruction(cond, return_values);
      }
      if(res)
	res = -1;
      break;
    case 2: // System set instructions
      cond = eval_if_version_2();
      res = eval_system_set(cond, return_values);
      if(res)
	res = -1;
      break;
    case 3: // Set instructions
      cond = eval_if_version_3();
      res = eval_set(cond, return_values);
      if(res)
	res = -1;
      break;
    case 4: // Set, Compare -> Link Sub-Instruction
      eval_set_sub(/*True*/ 1);
      cond = eval_if_version_4();
      res = eval_link_subins(cond, return_values);
      if(res)
	res = -1;
      break;
    case 5: // Compare -> (Set and Link Sub-Instruction)
      cond = eval_if_version_4();
      eval_set_sub(cond);
      res = eval_link_subins(cond, return_values);
      if(res)
	res = -1;
      break;
    case 6: // Compare -> Set, allways Link Sub-Instruction
      cond = eval_if_version_4();
      eval_set_sub(cond);
      res = eval_link_subins(/*True*/ 1, return_values);
      if(res)
	res = -1;
      break;
  }
  // Check if there are bits not yet examined

  extra_bits = 0;
  for(i = 0; i < 8; i++)
    if(cmd.bits [i] & ~cmd.examined [i]) {
      extra_bits = 1;
      break;
    }
  if(extra_bits) {
    printf ("[WARNING, unknown bits:");
    for(i = 0; i < 8; i++)
      printf(" %02x", cmd.bits [i] & ~cmd.examined [i]);
    printf("]\n");
  }

  return res;
}

/* Evaluate a set of commands in the given register set (which is
 * modified */
bool eval(vm_cmd_t commands[], int num_commands, 
	  registers_t *registers, link_t *return_values) {
  int i = 0;
  int total = 0;
  
  state = registers; // TODO FIXME

  while(i < num_commands && total < 100000) {
    int line;
    
    memset(return_values, 0, sizeof(link_t));
    line = eval_command(&commands[i].bytes[0], return_values);
    
    if (line < 0) // Link command
      return 1;
    
    if (line > 0) // Goto command
      i = line - 1;
    else // Just continue on the next line
      i++;

    total++;
  }
  
  memset(return_values, 0, sizeof(link_t));
  return 0;
}
