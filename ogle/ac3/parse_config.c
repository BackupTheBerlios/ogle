#include <stdlib.h>
#include <stdio.h>

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "debug_print.h"

extern char *program_name;


/* Put all of these in a struct that is returned instead */
static char *audio_device;
/* number of speakers */
static int front = 2;
static int rear = 0;
static int sub = 0;


char *get_audio_device(void)
{
  return audio_device;
}

static void parse_device(xmlDocPtr doc, xmlNodePtr cur)
{
  xmlChar *s;
  
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("path", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  if(audio_device != NULL) {
	    free(audio_device);
	  }
	  audio_device = strdup(s);
	  free(s);
	}
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

static void parse_audio(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("device", cur->name)) {
	parse_device(doc, cur);
      } else if(!strcmp("speakers", cur->name)) {
        parse_speakers(doc, cur);
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
  int r = 0;
  char *home;


  if((r += parse_oglerc(CONFIG_FILE)) == -1) {
    WARNING("parse_config(): Couldn't read "CONFIG_FILE"\n");
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
    
    if((r += parse_oglerc(rcpath)) == -1) {
      NOTE("parse_config(): Couldn't read '%s'\n", rcpath);
    }
    
    free(rcpath);
  }
  
  return r;
}


