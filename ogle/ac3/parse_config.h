#ifndef PARSE_CONFIG_H
#define PARSE_CONFIG_H

/* Ogle - A video player
 * Copyright (C) 2000, 2001, 2002 Bj�rn Englund, H�kan Hjort
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
 */

int parse_config(void);

char *get_audio_device(void);
char *get_audio_driver(void);

int get_num_sub_speakers(void);
int get_num_rear_speakers(void);
int get_num_front_speakers(void);

double get_a52_level(void);
int get_a52_adjust_level(void);
int get_a52_drc(void);

#endif /* PARSE_CONFIG_H */
