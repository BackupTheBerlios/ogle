
/*
 * IFO VIRTUAL MACHINE
 *
 * Copyright (C) 2000  Thomas Mirlacher
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 * The author may be reached as <dent@linuxvideo.org>
 *
 *------------------------------------------------------------
 *
 */


#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

#include "ifo.h"

#define bswap_16
static char menu_id[][80] = {
        "-0-", 
        "-1-", 
        "Title (VTS menu)",
        "Root",
        "Sub-Picture",
        "Audio",
        "Angle",
        "Part of Title",
};      


/**
 *
 */

char *decode_menuname (char index)
{
        return menu_id[index&0x07];
}       

typedef struct {
#if BYTE_ORDER == BIG_ENDIAN
	uint8_t type		: 3;
	uint8_t direct		: 1;
	uint8_t cmd		: 4;

	uint8_t dir_cmp		: 1;
	uint8_t cmp		: 3;
	uint8_t sub_cmd		: 4;
#else
	uint8_t cmd		: 4;
	uint8_t direct		: 1;
	uint8_t type		: 3;

	uint8_t sub_cmd		: 4;
	uint8_t cmp		: 3;
	uint8_t dir_cmp		: 1;
#endif

	uint8_t val[6];
} op_t;	

typedef struct {
	uint16_t cmd		: 16;
	uint16_t v0		: 16;
	uint16_t v2		: 16;
	uint16_t v4		: 16;
} sh_op_t;


static void _cmd_goto		(uint8_t *ptr);
static void _cmd_lnk		(uint8_t *ptr);
static void _cmd_setsystem	(uint8_t *ptr);
static void _cmd_set		(uint8_t *ptr);
static void _cmd_4		(uint8_t *ptr);
static void _cmd_5		(uint8_t *ptr);
static void _cmd_unknown	(uint8_t *ptr);

static void _case_lnk		(uint8_t *ptr);
static void _case_jmp		(uint8_t *ptr);
static void _case_setsystem	(uint8_t *ptr);
static void _case_misc		(uint8_t *ptr);
static void _case_set		(uint8_t *ptr);

static struct {
	void (*cmd) (uint8_t *ptr);
} cmd_switch[] = {
	{_cmd_goto},		// goto
	{_cmd_lnk},		// link, jump
	{_cmd_setsystem},
	{_cmd_set},
	{_cmd_4},
	{_cmd_5},
	{_cmd_5},
	{_cmd_unknown},
	{_cmd_unknown},
	{_cmd_unknown},
	{_cmd_unknown},
	{_cmd_unknown},
	{_cmd_unknown},
	{_cmd_unknown},
	{_cmd_unknown},
	{_cmd_unknown},
	{_cmd_unknown},
	{_cmd_unknown},
	{_cmd_unknown},
	{_cmd_unknown},
};


/**
 * System register
 **/

static char reg_name[][80]={
	"Menu_Language_Code",
	"Audio_Stream_#",
	"SubPicture_Stream_#",
	"Angle_#",
	"VTS_#",
	"VTS_Title_#",
	"PGC_#",
	"PTT_#",
	"Highlighted_Button_#",
	"Nav_Timer",
	"TimedPGC",
	"Karaoke_audio_mixing_mode",
	"Parental_mgmt_country_code",
	"Parental_Level",
	"Player_Video_Cfg",
	"Player_Audio_Cfg",
	"Audio_language_code_setting",
	"Audio_language_extension_code",
	"SPU_language_code_setting",
	"SPU_language_extension_code",
	"?Player_Regional_Code",
	"Reserved_21",
	"Reserved_22",
	"Reserved_23"
};


/**
 *
 **/

static void _PrintRegister (uint16_t data, uint8_t direct)
{
	data = bswap_16 (data);

	if (direct) {
		if (isalpha (data>>8&0xff))
			printf("'%c%c'", data>>8&0xff, data&0xff);
		else
			printf("0x%02x", data);
	} else {
		if (data&0x80) {
			data &= 0x1f;

			if (data > 0x17)
				printf("s[ILL]");
			else
				printf("s[%s]", reg_name[data]);
		} else {
			data &= 0x1f;

			if (data > 0xf)
				printf("r[ILL]");
			else
				printf("r[0x%02x]", data);
		}
	}
}


/**
 *
 **/

static void _hexdump (uint8_t *ptr, uint8_t num)
{
	int i;

	for (i=8-num; i<8; i++)
		printf ("%02x ", ptr[i]);
	printf ("\t");	
}


/**
 *
 **/

static char *_decode_calc (char val)
{
	static char calc_op[][10] = {
		"none",
		"=", 
		"<->",	// swap
		"+=",
		"-=",
		"*=",
		"/=",
		"%=",
		"rnd",	// rnd
		"&=",
		"|=",
		"^=",
		"??",	// invalid
		"??",	// invalid
		"??",	// invalid
		"??"	// invalid
};

	return (char *) calc_op[val&0x0f];
}

	
char cmp_op[][10] = {
	"none",
	"&&",
	"==",
	"!=",
	">=",
	">",
	"<",
	"<="
};


void advanced (uint8_t *op)
{
#if 1
	uint8_t cmd = op[0];

	printf(" { ");

	if (op[1]>>2)
		printf(" Highlight button %d; ", op[1]>>2);

//	if ((cmd == 0xff) || (cmd == 0x00))
	if (cmd == 0xff)
		printf (" Illegal ");

	if (cmd == 0x00) {
	  // was op[7] that is out of bounds!!!
	  printf ("DENT_GUESS link to %d", op[1]);
#if 0 
	  switch (op[1]) {
	  case 0x06:
	    printf ("HH_GUESS link to (next) cell/PG?");
	    break;
	  case 0x07:
	    printf ("HH_GUESS link to (prev) cell/PG?");
	    break;
	  case 0x0d:
	    printf ("HH_GUESS link to end of PGC? (resume playback?)");
	    break;
	  default:
	    printf ("DENT_GUESS link to %d", op[1]);
	  }
#endif
	}
	else
	if ((cmd & 0x06) == 0x02) {	// XX01Y
		printf ("Link to %s cell ", (cmd & 0x01) ? "prev" : "next");
	} else {
		printf ("advanced (0x%02x) ", cmd);
	}
#else
	uint8_t button=op[1]>>2;
	uint8_t cmd = op[0];

	printf(" { ");

	switch (command) {
		case 0:
		case 1:
		case 5:
		case 6:
		case 7:
		case 9:
		case 10:
		case 12:
		case 13:
		case 14:
		case 16:
			if (button)
				printf(" Highlight button %d; ", button);
			printf ("advanced (0x%02x)", command);
			break;
	
		case 2:
			if (button)
				printf ("Highlight button %d; ", button);
			printf ("Link to next cell ");
			break;

		case 3:
			if (button)
				printf ("Highlight button %d;", button);
			printf ("Link to prev cell ");
			break;
	
		case 4:
		case 8:
		case 11:
		case 15:
			printf(" Illegal ");
			break;
	}
#endif
	printf(" } ");
}


/**
 *
 **/

void _case_setsystem (uint8_t *opcode)
{
        op_t *op = (op_t *) opcode;
        sh_op_t *sh=(sh_op_t *)opcode;

        switch(op->cmd) {
        case 1: {
		int i;

		for (i=1; i<=3; i++) {
                        if (op->val[i]&0x80) {
                		if(op->direct)
                                	printf ("s[%s] = 0x%02x;", reg_name[i], op->val[i]&0xf);
				else
                                	printf ("s[%s] = r[0x%02x];", reg_name[i], op->val[i]&0xf);
			}
		}	

#if 0
                if(op->direct) {
                        if(op->val[1]&0x80)
                                printf ("s[%s] = 0x%02x;", reg_name[1], op->val[1]&0xf);
                        if(op->val[2]&0x80)
//DENT: lwhat about 0x7f here ???
                                printf ("s[%s] = 0x%02x;", reg_name[2], op->val[2]&0x7f);
                        if(op->val[3]&0x80)
                                printf ("s[%s] = 0x%02x;", reg_name[3], op->val[3]&0xf);
                } else {
                        if(op->val[1]&0x80)
                                printf ("s[%s] = r[0x%02x];", reg_name[1], op->val[1]&0xf);
                        if(op->val[2]&0x80)
                                printf ("s[%s] = r[0x%02x];", reg_name[2], op->val[2]&0xf);
                        if(op->val[3]&0x80)
                                printf ("s[%s] = r[0x%02x];", reg_name[3], op->val[3]&0xf);
                }
#endif
		}
                break;
        case 2:
                if(op->direct)
                        printf ("s[%s] = 0x%02x", reg_name[9], bswap_16(sh->v0));
                else
                        printf ("s[%s] = r[0x%02x]", reg_name[9], op->val[1]&0x0f);

		printf ("s[%s] = (s[%s]&0x7FFF)|0x%02x", reg_name[10], reg_name[10], bswap_16(sh->v2)&0x8000);
                break;
        case 3:
                if(op->direct)
                        printf ("r[0x%02x] = 0x%02x", op->val[3]&0x0f, bswap_16(sh->v0));
                else
                        printf ("r[r[0x%02x]] = r[0x%02x]", op->val[3]&0x0f, op->val[1]&0x0f);
                break;
        case 4:
                //actually only bits 00011100 00011100 are set
                if(op->direct)
                        printf ("s[%s] = 0x%02x", reg_name[11], bswap_16(sh->v2));
                else
                        printf ("s[%s] = r[0x%02x]", reg_name[11], op->val[3]&0x0f );
                break;
        case 6:
                //actually,
                //s[%s]=(r[%s]&0x3FF) | (0x%02x << 0xA);
                //but it is way too ugly
                if(op->direct)
                        printf ("s[%s] = 0x%02x", reg_name[8], op->val[2]>>2);
                else
                        printf ("s[%s] = r[0x%02x]", reg_name[8], op->val[3]&0x0f);
                break;
        default:
                printf ("unknown");
        }
}


/**
 *
 **/

char parental[][10] = {
	"0",
	"G",
	"2",
	"PG",
	"PG-13",
	"5",
	"R",
	"NC-17"
};


void _case_misc (uint8_t *opcode)
{
	op_t *op = (op_t *) opcode;
	sh_op_t *sh = (sh_op_t *) opcode;

	switch(op->sub_cmd) {
	case 1:
		printf ("goto Line 0x%02x", bswap_16(sh->v4));
		break;
	case 2:
		printf ("stop VM");
		break;
	case 3:
		printf ("Set Parental Level To %s and goto Line 0x%02x", parental[op->val[4]&0x7], op->val[5]);
		break;
	default:
		printf("Illegal");
		break;
	}
}


/**
 *
 **/

static void _cmd_goto (uint8_t *opcode)
{
	op_t *op = (op_t *) opcode;
	sh_op_t *sh=(sh_op_t *)opcode;

	if (!opcode[1]) {
		printf("nop");
	} else {
		if (op->cmp) {
			printf ("if (r[0x%02x] %s ", op->val[1]&0x0f, cmp_op[op->cmp]);
			_PrintRegister (sh->v2, op->dir_cmp);
			printf (") ");
		}
		_case_misc(opcode);
	}
}


/**
 * 
 **/

static void _case_lnk (uint8_t *opcode)
{
	int button;
	op_t op;
	sh_op_t sh;
	memcpy (&op, opcode, sizeof (op_t));
	memcpy (&sh, opcode, sizeof (sh_op_t));

	button = op.val[4]>>2;

	printf ("lnk to ");

	switch (op.sub_cmd) {
		case 0x01:
			advanced (&op.val[4]);
			break;

		case 0x04:
			printf ("PGC 0x%02x", bswap_16 (sh.v4));
			break; 

		case 0x05:
			printf ("PTT 0x%02x", bswap_16 (sh.v4));
			break; 

		case 0x06:
			printf ("Program 0x%02x this PGC", op.val[5]);
			break;

		case 0x07:
			printf ("Cell 0x%02x this PGC", op.val[5]);
			break;
		default:
			return;
	}

	if (button)
		printf (", Highlight 0x%02x", op.val[4]>>2);
}


/**
 *
 **/

static void _cmd_lnk (uint8_t *opcode)
{
	op_t op;
	sh_op_t sh;
	memcpy (&op, opcode, sizeof (op_t));
	memcpy (&sh, opcode, sizeof (sh_op_t));

	if (!opcode[1]) {
		printf("nop");
	} else {
		if (op.direct) {
			if (op.cmp) {
				printf ("if (r[0x%02x] %s ", op.val[4]&0x0f, cmp_op[op.cmp]);
				_PrintRegister (op.val[5], 0);
				printf (") ");
			}
			_case_jmp (opcode);
		} else {
			if (op.cmp) {
				printf ("if (r[0x%02x] %s ", op.val[1]&0x0f, cmp_op[op.cmp]);
				_PrintRegister (sh.v2, op.dir_cmp);
				printf (") ");
			}
			_case_lnk (opcode);
        	}
	}
}


/**
 *
 **/

static void _case_jmp (uint8_t *opcode)
{
	op_t op;
	memcpy (&op, opcode, sizeof (op_t));

	printf ("jmp ");

	switch (op.sub_cmd) {
		case 0x01:
			printf ("Exit");
			break;
		case 0x02:
			printf ("VTS 0x%02x", op.val[3]);
			break;
		case 0x03:
			printf ("This VTS Title 0x%02x", op.val[3]);
			break;
		case 0x05:
			printf ("This VTS Title 0x%02x Part 0x%04x", op.val[3], op.val[0]<<8|op.val[1]);
			break;
		case 0x06:
#if 0
			printf ("in SystemSpace ");
			switch (op.val[3]>>4) {
				case 0x00:
					printf ("to play first PGC");
					break;
				case 0x01: {
					printf ("to menu \"%s\"", decode_menuname (op.val[3]));
				}
					break;
				case 0x02:
					printf ("to VTS 0x%02x and TTN 0x%02x", op.val[1], op.val[2]);
					break;
				case 0x03:
					printf ("to VMGM PGC number 0x%02x", op.val[0]<<8 | op.val[1]);
					break;
				case 0x08:
					printf ("vts 0x%02x lu 0x%02x menu \"%s\"", op.val[2], op.val[1], decode_menuname (op.val[3]));
					break;
#else
			switch (op.val[3]>>6) {
				case 0x00:
					printf("to play first PGC");
					break;				
				case 0x01:
					printf ("to VMG title menu (?)");
					break;
				case 0x02:
					printf ("vts 0x%02x lu 0x%02x menu \"%s\"", op.val[2], op.val[1], decode_menuname (op.val[3]&0xF));
					break;				
				case 0x03:
					printf ("vmg pgc 0x%04x (?)", (op.val[0]<<8)|op.val[1]);
					break;
#endif
			}
			break;
		case 0x08:
#if 0
			switch(op.val[3]>>4) {
				case 0x00:
					printf ("system first pgc");
					break;
				case 0x01:
					printf ("system title menu");
					break;
				case 0x02:
					printf ("system menu \"%s\"", decode_menuname (op.val[3]));
					break;
				case 0x03:
					printf ("system vmg pgc %02x ????", op.val[0]<<8|op.val[1]);
					break;
				case 0x08:
					printf ("system lu 0x%02x menu \"%s\"", op.val[2], decode_menuname (op.val[3]));
					break;
				case 0x0c:
					printf ("system vmg pgc 0x%02x", op.val[0]<<8|op.val[1]);
					break;
			}
#else
			// op.val[2] is number of cell
			// it is processed BEFORE switch
			// under some conditions, it is ignored
			// I don't understand exactly what it means
			printf(" ( spec cell 0x%02X ) ", op.val[2]); 

			switch(op.val[3]>>6) {
				case 0:
					printf("to FP PGC");
					break;
				case 1:
					printf("to VMG root menu (?)");
					break;
				case 2:
					printf("to VTS menu \"%s\" (?)", decode_menuname(op.val[3]&0xF));
					break; 
				case 3:
					printf("vmg pgc 0x%02x (?)", (op.val[0]<<8)|op.val[1]);
					break;
			}	
#endif
			break;
	}
}


/**
 *
 **/

static void _case_set (uint8_t *opcode)
{
#if 1
	op_t *op = (op_t *) opcode;
	sh_op_t *sh=(sh_op_t *) opcode;

	_PrintRegister (sh->v0, 0);
	printf (" %s ", _decode_calc (op->cmd));
	_PrintRegister (sh->v2, op->direct);
#else
	op_t *op = (op_t *) opcode;
	sh_op_t *sh=(sh_op_t *)opcode;

	if(op->cmd!=2) {
		_PrintRegister (op->val[1] & 0x0F);
		printf (" %s ", _decode_calc (op->cmd));
		_PrintRegister (sh->v2, op->direct);
	} else
		printf("Swap r[0x%X] and r[0x%X] ", op->val[1] & 0x0F, bswap_16 (sh->v2));
#endif
}


/**
 *
 **/

static void _cmd_set (uint8_t *opcode)
{
	op_t *op = (op_t *) opcode;
	sh_op_t *sh = (sh_op_t *) opcode;

	if (!opcode[1]) {
		_case_set (opcode);
	} else if (op->cmp && !op->sub_cmd) {
		printf ("if (r[0x%02x] %s ", op->val[0]&0x0f, cmp_op[op->cmp]);
		_PrintRegister (sh->v4, op->dir_cmp);
		printf (") ");
		_case_set (opcode);
	} else if (!op->cmp && op->sub_cmd) {
		printf ("if (");
		_case_set (opcode);
		printf (") ");
		_case_lnk (opcode);
	} else
		printf ("nop");
}


/**
 *
 **/

static void _cmd_setsystem (uint8_t *opcode)
{
	op_t *op = (op_t *) opcode;

	if (!opcode[1]) {
		_case_setsystem (opcode);
	} else if (op->cmp && !op->sub_cmd) {
		printf ("if (r[0x%02x] %s ", op->val[4]&0x0f, cmp_op[op->cmp]);
		_PrintRegister (op->val[5], 0);
		printf (") ");
		_case_setsystem (opcode);
	} else if (!op->cmp && op->sub_cmd) {
		printf ("if (");
		_case_setsystem (opcode);
		printf (") ");
		_case_lnk (opcode);
	} else
		printf("nop");
}


/**
 *
 **/

static void _cmd_unknown (uint8_t *opcode)
{
	printf ("unknown (0x%02x)", *opcode);
}


/**
 *
 **/

void _cmd_4 (uint8_t *opcode)
{
        // math command on r[opcode[1]] and direct?bswap_16(sh->v0):reg[op->val[1]] is executed
        // ( unless command is swap; then r[opcode[1]] and r[op->val[1]] are swapped )
        //
        //  boolean operation cmp on r[opcode[1]] and dir_cmp?bswap_16(sh->v2):reg[op->val[3]] is executed
        //  on true result, buttons(c[6], c[7]) is called
        //  problem is 'what is buttons()'
        //
		op_t *op = (op_t *) opcode;
		sh_op_t *sh=(sh_op_t *) opcode;

		printf("r[0x%X] ", opcode[1]);
		printf (" %s ", _decode_calc (op->cmd));
		if(op->cmd==2)
			printf("r[0x%X] ", op->val[1]);
		else
			_PrintRegister (sh->v0, op->direct);
		printf("; ");

        printf("if ( r[%d] %s ", opcode[1], cmp_op[op->cmp]);
		_PrintRegister (sh->v2, op->dir_cmp);
		printf(" )  then {");
		advanced (&op->val[4]);
		printf("}");
}


/**
 *
 **/

void _cmd_5 (uint8_t *opcode)
{
        //
        // opposite to dump_4: boolean, math and buttons
        //
	op_t *op = (op_t *) opcode;
	sh_op_t *sh=(sh_op_t *) opcode;
	
	printf("if (");
		
	if((!op->direct) && op->dir_cmp )
		printf("0x%X", bswap_16 (sh->v2));
	else {
		_PrintRegister (op->val[3], 0);
		if(op->val[3]&0x80)
			printf("s[%s]", reg_name[op->val[3]&0x1F]);
		else
			printf("r[0x%X]", op->val[3]&0x1F);	// 0x1F is either not a mistake,
								// or Microsoft programmer's mistake!!!
	}

	printf(" %s r[0x%X] ", cmp_op[op->cmp], op->direct?op->val[2]:op->val[1]);
	printf(" )  then {");
	printf("r[0x%X] ", opcode[1]&0xF);
	printf (" %s ", _decode_calc (op->cmd));

	if(op->cmd==2)
		printf("r[0x%X] ", op->val[0]&0x1F);
	else {
		if(op->direct)
			printf("0x%X", bswap_16 (sh->v0));
		else {
			if(op->val[0]&0x80)
				printf("s[%s]", reg_name[op->val[0]&0x1F]);
			else
				printf("r[0x%X]", op->val[0]&0x1F);
		}
	}

	printf("; ");
	advanced (&op->val[4]);
	printf("}");
}


/**
 * 8 bytes opcode
 */

void ifoPrintVMOP (uint8_t *opcode)
{
	op_t *op = (op_t *) opcode;

	//_hexdump (opcode, 8);
//	printf ("\n\t");

	cmd_switch [op->type].cmd (opcode);
	//printf (";");
}

