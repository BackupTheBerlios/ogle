/* Ogle - A video player
 * Copyright (C) 2000, 2001, 2002 Björn Englund, Håkan Hjort
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "debug_print.h"


/* Put all of these in a struct that is returned instead */
static char *audio_device = NULL;
/* number of speakers */
static int front = 2;
static int rear = 0;
static int sub = 0;

/* liba52 specific config */
static double a52_level = 1;
static int a52_adjust_level = 0;
static int a52_drc = 0;

static char *audio_driver = NULL;

/* sync */
static char *sync_type = NULL;
static int sync_resample = 0;

char *get_audio_driver(void)
{
  return audio_driver;
}

char *get_audio_device(void)
{
  return audio_device;
}

static void parse_device(xmlDocPtr doc, xmlNodePtr cur)
{
  xmlChar *s = NULL;
  
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("path", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  if(audio_device != NULL) {
	    free(audio_device);
	  }
	  audio_device = strdup(s);
	}
      } else if(!strcmp("driver", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  if(audio_driver != NULL) {
	    free(audio_driver);
	  }
	  audio_driver = strdup(s);
	}
      }
      if(s) {
	free(s);
	s = NULL;
      }
    }
    cur = cur->next;
  }
}

int get_num_front_speakers(void)
{
  return front;
}

int get_num_rear_speakers(void)
{
  return rear;
}

int get_num_sub_speakers(void)
{
  return sub;
}

static void parse_speakers(xmlDocPtr doc, xmlNodePtr cur)
{
  xmlChar *s = NULL;
  
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("front", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
          front = atoi(s);
	}
      } else if(!strcmp("rear", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
          rear = atoi(s);
	}
      } else if(!strcmp("sub", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
          sub = atoi(s);
	}
      }
      if(s) {
	free(s);
      }
    }
    cur = cur->next;
  }
}



double get_a52_level(void)
{
  return a52_level;
}

int get_a52_adjust_level(void)
{
  return a52_adjust_level;
}

int get_a52_drc(void)
{
  return a52_drc;
}

static void parse_liba52(xmlDocPtr doc, xmlNodePtr cur)
{
  xmlChar *s = NULL;
  
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("level", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
          a52_level = atof(s);
	}
      } else if(!strcmp("adjust_level", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  if(!strcmp("yes", s)) {
	    a52_adjust_level = 1;
	  } else {
	    a52_adjust_level = 0;
	  }
	}
      } else if(!strcmp("drc", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  if(!strcmp("yes", s)) {
	    a52_drc = 1;
	  } else {
	    a52_drc = 0;
	  }
	}
      }
      if(s) {
	free(s);
      }
    }
    cur = cur->next;
  }
}


char *get_sync_type(void)
{
  return sync_type;
}

int get_sync_resample(void)
{
  return sync_resample;
}


static void parse_sync(xmlDocPtr doc, xmlNodePtr cur)
{
  xmlChar *s = NULL;
  
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("type", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  if(sync_type != NULL) {
	    free(sync_type);
	  }
	  sync_type = strdup(s);
	}
      } else if(!strcmp("resample", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  if(!strcmp("yes", s)) {
	    sync_resample = 1;
	  } else {
	    sync_resample = 0;
	  }
	}
      }

      if(s) {
	free(s);
	s = NULL;
      }
      
    }
    cur = cur->next;
  }
}


static void parse_audio(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("device", cur->name)) {
	parse_device(doc, cur);
      } else if(!strcmp("speakers", cur->name)) {
        parse_speakers(doc, cur);
      } else if(!strcmp("liba52", cur->name)) {
        parse_liba52(doc, cur);
      } else if(!strcmp("sync", cur->name)) {
        parse_sync(doc, cur);
      }
    }
    cur = cur->next;
  }
}


static void parse_ogle_conf(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("audio", cur->name)) {
	parse_audio(doc, cur);
      }
    }
    cur = cur->next;
  }
}


int parse_oglerc(char *filename)
{
  xmlDocPtr doc;
  xmlNodePtr cur;
  int fd;

  if((fd = open(filename, O_RDONLY)) == -1) {
    if(errno != ENOENT) {
      perror(filename);
    }
    return -1;
  } else {
    close(fd);
  }

  doc = xmlParseFile(filename);

  if(doc == NULL) {
    return -1;
  }
  
  cur = xmlDocGetRootElement(doc); 
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("ogle_conf", cur->name)) {
	parse_ogle_conf(doc, cur);
      }
    }
    cur = cur->next;
  }
  
  xmlFreeDoc(doc);
  
  return 0;
}


int parse_config(void)
{
  int config_read = 0;
  char *home;
  

  if(parse_oglerc(CONFIG_FILE) != -1) {
    config_read |= 1;
  }

  
  home = getenv("HOME");
  if(home == NULL) {
    WARNING("No $HOME\n");
  } else {
    char *rcpath = NULL;
    char rcfile[] = ".oglerc";
    
    rcpath = malloc(strlen(home)+strlen(rcfile)+2);
    strcpy(rcpath, home);
    strcat(rcpath, "/");
    strcat(rcpath, rcfile);
    
    if(parse_oglerc(rcpath) != -1) {
      config_read |= 2;
    }
    if(!config_read) {
      ERROR("parse_config(): Couldn't read '%s'\n", rcpath);
    }
    free(rcpath);
  }
  
  if(!config_read) {
    ERROR("parse_config(): Couldn't read "CONFIG_FILE"\n");
    return -1;
  }

  
  return 0;
}


