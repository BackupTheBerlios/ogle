/* xvattr - get/set Xv attributes
 * Copyright (C) 2001-2002 Björn Englund
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
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xvlib.h>

#include "getopt.h"

static char *program_name;
static const char *short_options = "a:v:p:h?V?";

static struct option long_options[] = {
  {"help"           , no_argument      , 0, 'h' },
  {"attribute"      , required_argument, 0, 'a' },
  {"port"           , required_argument, 0, 'p' },
  {"value"          , required_argument, 0, 'v' },
  {"version"        , no_argument,       0, 'V' },
  {"display"        , required_argument, 0, 'd' }
};

extern char *optarg;


void usage(void)
{
  fprintf(stderr, "usage: %s [-a <attribute name>] [-v <value>] [-p <port nr>]\n", program_name);
  fprintf(stderr, "       %s {--display} <display>\n", program_name);
  fprintf(stderr, "       %s {-h | -help}\n", program_name);
  fprintf(stderr, "       %s {-p | --port} <port nr>\n", program_name);
  fprintf(stderr, "       %s {-V | --version}\n", program_name);
}

void show_version(void)
{
  fprintf(stderr, "%s version %s\n", program_name, VERSION);
}

int main(int argc, char **argv)
{
  Display *dpy;
  int c;
  char *attr_name = NULL;
  char *dpy_name = NULL;
  int num_adaptors = 0;
  XvAdaptorInfo *adaptor_info;
  int n;
  int val = -1111111;
  int port_nr = -1;
  unsigned int xv_version, xv_release;
  unsigned int xv_request_base, xv_event_base, xv_error_base;

  int option_index = 0;
  int seen_val = 0;
  int seen_attr = 0;
  
  program_name = argv[0];

  
  while((c = getopt_long(argc, argv, short_options,
			 long_options, &option_index)) != EOF) {
    switch(c) {
    case 'a':
      attr_name = optarg;
      seen_attr = 1;
      break;
    case 'v':
      sscanf(optarg, "%i", &val);
      seen_val = 1;
      break;
    case 'V':
      show_version();
      exit(0);
      break;
    case 'p':
      sscanf(optarg, "%i", &port_nr);
      break;
    case 'd':
      dpy_name = optarg;
      break;
    default:
      usage();
      exit(0);
    }
  }

  if (seen_val && !seen_attr) {
    fprintf(stderr, "%s: you need to give an attribute to set a value.\n", 
	    program_name);
    exit(2);
  }
  
  dpy = XOpenDisplay(dpy_name);
  if(dpy == NULL) {
    fprintf(stderr, "%s: Couldn't open display '%s'\n",
	    program_name, dpy_name ? dpy_name :
	    (getenv("DISPLAY") ? getenv("DISPLAY") : ""));
    exit(-1);
  }

  if(XvQueryExtension(dpy, &xv_version, &xv_release, &xv_request_base,
		      &xv_event_base, &xv_error_base) == Success) {
    fprintf(stdout, "Found Xv %d.%d\n", xv_version, xv_release);
  } else {
    fprintf(stderr, "%s: Xv not found\n", program_name);
    exit(-1);
  }

  XvQueryAdaptors(dpy, DefaultRootWindow(dpy),
		  &num_adaptors, &adaptor_info);  
  
  if(attr_name == NULL) {
    for(n = 0; n < num_adaptors; n++) {
      int base_id;
      int num_ports;
      int m;
      base_id = adaptor_info[n].base_id;
      num_ports = adaptor_info[n].num_ports;
      
      fprintf(stdout, "Adaptor: %d\n", n);
      fprintf(stdout, "Name: %s\n", adaptor_info[n].name);

      for(m = base_id; m < base_id+num_ports; m++) {
	int num;
	int k;
	int cur_val;
	Atom val_atom;
	XvAttribute *xvattr;
	
	fprintf(stdout, " Port: %d\n", m);
	xvattr = XvQueryPortAttributes(dpy, m, &num);
	for(k = 0; k < num; k++) {
	  fprintf(stdout, "  Name: %s\n", xvattr[k].name);
	  fprintf(stdout, "   Flags: %s%s\n",
		  (xvattr[k].flags & XvGettable) ? "XvGettable " : "",
		  (xvattr[k].flags & XvSettable) ? "XvSettable " : "");
	  fprintf(stdout, "   Min value: %d\n", xvattr[k].min_value);
	  fprintf(stdout, "   Max value: %d\n", xvattr[k].max_value);
	  val_atom = XInternAtom(dpy, xvattr[k].name, False);
	  if(xvattr[k].flags & XvGettable) {
	    if(XvGetPortAttribute(dpy, m, val_atom, &cur_val) != Success) {
	      fprintf(stderr, "Couldn't get attribute\n");
	      exit(-1);
	    }
	    fprintf(stdout, "   Current value: %d\n", cur_val);
	  }
	}
      }
    }
  } else {
    Atom attr_atom;
    attr_atom = XInternAtom(dpy, attr_name, False);
  
    if(port_nr == -1) {
      if(num_adaptors == 0) {
	fprintf(stderr, "No adaptor\n");
	exit(-1);
      }
      port_nr = adaptor_info[0].base_id;
    }
    if(val == -1111111) {
      if(XvGetPortAttribute(dpy, port_nr, attr_atom, &val) != Success) {
	fprintf(stderr, "%s: Couldn't get attribute\n", program_name);
	exit(-1);
      }
      fprintf(stdout,"%s = %d\n", attr_name, val);
    } else {
      if(XvSetPortAttribute(dpy, port_nr, attr_atom, val) != Success) {
	fprintf(stderr, "%s: Couldn't set attribute\n", program_name);
	exit(-1);
      }
      XFlush(dpy);
      if(XvGetPortAttribute(dpy, port_nr, attr_atom, &val) != Success) {
	fprintf(stderr, "%s: Couldn't get attribute\n", program_name);
	exit(-1);
      }
      fprintf(stdout,"%s set to %d\n", attr_name, val);
    }
  } 
  exit(0);
}
