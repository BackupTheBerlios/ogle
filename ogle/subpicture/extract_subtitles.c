/*
 * vob2subt.c - extracts DVD subtitles from a VOB file
 * Copyright (C) 2000   Samuel Hocevar <sam@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *                                                     
 */

/* open */
#include <fcntl.h>
/* printf */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main (int argc, char *argv[])
{
	unsigned char c[2048];
	unsigned int id, type, dummy, howmany = 0, useful = 0, notforme = 0;
	int valid;
	
	if (argc != 2) {
		fprintf(stderr,
						"usage: %s ID\nID : subtitle ID stream (0, 1, 2...)\nstdin : VOB stream\nstdout : extracted subtitles\n",	argv[0]);
		exit (1);
	}
	
	id = atoi(argv[1]) + 0x20;
	
	/* read the stream - 2048 bytes each time */
	while (read(0, &c, 2048)) {
		howmany += 2048;
		valid=1;
		if ((c[0] != 0x00) || (c[1] != 0x00) ||	(c[2] != 0x01) || (c[3] != 0xba))	{
			//fprintf (stderr,"Sorry, doesn't appear to be a valid VOB stream\n");
			//fprintf (stderr, "\t\t\t%02x %02x %02x %02x\n",c[0],c[1],c[2],c[3]);
			valid = 0;
			//exit (0);
		}
		//fprintf (stderr,"PS-packet\n");
		
		if ( valid && (c[14] == 0x00) && (c[15] == 0x00) && (c[16] == 0x01) && (c[17] == 0xbd))	{
			//fprintf (stderr,"PES-packet\n");
			type = c [ 23 + c[22] ] ;
			if (type == 0x80) { 
				//fprintf(stderr, "AC3\n");
			}
			if (type == id) /* 0x2* is subtitles
												 0x80 would have been AC3 */
				
				//if ((type & 0xf0) == 0x20) /* 0x2* is subtitles
				//0x80 would have been AC3 */
				{
					int outfile;
					fprintf (stderr,"subtitles  type:%02x\n", type);
					fprintf(stderr, "   %i bytes read, %i kept, %i trashed subtitles\n",
									howmany, useful, notforme);
					dummy = 24 + c[22];
					outfile = open("out", O_WRONLY | O_CREAT);
					write(outfile, c + dummy , 2048 - dummy);
					system("./subt2xpm out | xv -");
					
					/* we may choose to dump the entire packet, like real men do */
					//write(1, c, 2048);
					
					useful += 2048;
				} else {
					if ((type & 0xf0) != 0x20) notforme += 2048;
				}
		}
		//fprintf(stderr, "%i bytes read, %i kept, %i trashed subtitles\n",
		//howmany, useful, notforme);
	}
	fprintf(stderr, "\n");
	return 0;
}

