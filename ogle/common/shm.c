/* Ogle - A video player
 * Copyright (C) 2004 Björn Englund
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
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>

#include "debug_print.h"
#include "shm.h"

#ifdef HAVE_POSIX_SHM

#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#endif //HAVE_POSIX_SHM



#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif

typedef struct {
  int *shmids;
  int len;
} shmid_array_t;

static shmid_array_t created_sysv_shmids = { NULL, 0 };


static int shmid_array_add(shmid_array_t *a, int shmid)
{
  int n;
  int *p;

  for(n = 0; n < a->len; n++) {
    if(a->shmids[n] == -1) {
      //we found a free position
      a->shmids[n] = shmid;
      return 0;
    }
  }
  
  //no free position, allocate some more
  p = realloc(a->shmids, (a->len + 8)*sizeof(int));
  if(p == NULL) {
    ERROR("shmid_array_add, realloc: %s\n", strerror(errno));
    return -1;
  }
  a->shmids = p;
  a->len += 8;
  a->shmids[n] = shmid;
  n++;

  //initialize the newly allocated positions
  for(; n < a->len; n++) {
    a->shmids[n] = -1;
  }
  
  return 0;
}


static int shmid_array_remove(shmid_array_t *a, int shmid)
{
  int n;

  if(a) {
    for(n = 0; n < a->len; n++) {
      if(a->shmids[n] == shmid) {
	//we found the position
	a->shmids[n] = -1;
	return 0;
      }
    }
  }
  return -1;
}

static int remove_all_shmids(shmid_array_t *a, int(*rmfunc)(int))
{
  int n;
  if(a) {
    if(a->shmids) {
      for(n = 0; n < a->len; n++) {
	if(a->shmids[n] != -1) {
	  //found a shmid, remove it
	  rmfunc(a->shmids[n]);
	}
      }
      free(a->shmids);
      a->shmids = NULL;
      a->len = 0;
    }
  }
  return 0;
}

#ifdef HAVE_POSIX_SHM


#if defined(__FreeBSD__)
/*
 * FreeBSD places posix shm objects in the normal filesystem.
 */
static const char *shmid_prefix = "/tmp/ogle";
#else
/*
 * Solaris, Linux puts posix shm objects outside normal filesystem
 * and the name must start with a '/' and not contain any more '/'.
 * Solaris man page suggest a max of 14 characters for portability.
 */
static const char *shmid_prefix = "/ogle";
#endif

typedef  struct _shm_seg_t {
  struct _shm_seg_t *next;
  void *shmaddr;
  size_t len;
} shm_seg_t;

static shm_seg_t *segment_list = NULL;

static shmid_array_t created_shmids = { NULL, 0 };

static int use_posix_shm = USE_POSIX_SHM;

static int ogle_shm_initialized = 0;


/*
 * If the environment variable OGLE_USE_POSIX_SHM
 * is set to '0', psoix shm will not be used, instead
 * the ogle_shm* functions are redirected to the ogle_sysv_shm* functions.
 * If it is set to anything other than '0', posix shm will be used.
 * If not set, the default will be used (depends on configure)
 */
static void init_ogle_shm(void)
{
  char *env_val;
  if((env_val = getenv("OGLE_USE_POSIX_SHM"))) {
    if(!strcmp(env_val, "0")) {
      use_posix_shm = 0;
    } else {
      use_posix_shm = 1;
    }
  }
  ogle_shm_initialized = 1;
}

static char *no_pshm_str = "OGLE_USE_POSIX_SHM=0";

/*
 * This is called from the ogle_shm* functions when they fail on
 * ENOSYS (function not implemented. 
 * Several Linux distros don't mount tmpfs on /dev/shm by default,
 * so they don't support POSIX shared memory.
 * After this call all shm functions are redirected to 
 * the ogle_sysv* functions instead.
 */
static void revert_to_sysv(void)
{  
  WARNING("%s", "Reverting to SYSV shm\n");
  use_posix_shm = 0;
  putenv(no_pshm_str);
}
 

/*
 * Creates a shared memory object
 * size - Requested size in bytes of the object
 * mode - permissions of object
 * Returns an int (shmid) that identifies the object or
 * -1 if the object couldn't be created.
 */
int ogle_shmget(int size, int mode)
{
  int shmid;
  int n;
  int fd;
  char shmname[64];
  int retries = 20;
  int pid;
  static unsigned short shm_serial = 0;

  if(!ogle_shm_initialized) {
    init_ogle_shm();
  }
  
  if(!use_posix_shm) {
    return ogle_sysv_shmget(size, mode);
  }

  pid = getpid() & 0xffff;

  for(n=0; n < retries; n++) {
    shmid = pid << 16 | shm_serial;
    shm_serial++;
    snprintf(shmname, sizeof(shmname),
	     "%s%08x", shmid_prefix, shmid);
    
    if((fd = shm_open(shmname, O_RDWR | O_CREAT | O_EXCL, mode)) == -1) {
      switch(errno) {
      case ENOSYS:
	//shm_open not implemented, revert to sysv shm
	WARNING("ogle_shmget, shm_open '%s': %s\n", shmname, strerror(errno));
	revert_to_sysv();
	return ogle_sysv_shmget(size, mode);
	break;
      case EEXIST:
	continue;
      default:
	ERROR("ogle_shmget, shm_open '%s': %d %s\n",
	      shmname, errno, strerror(errno));
	return -1;
	break;
      }
    } else {
      break;
    }
  }
  if(n == retries) {
    ERROR("%s", "ogle_shmget: Failed to open a shm object\n");
    return -1;
  }
  
  if(ftruncate(fd, size) == -1) {
    ERROR("ogle_shmget, ftruncate: %s\n", strerror(errno));
    close(fd);
    shm_unlink(shmname);
    return -1;
  }

  close(fd);

  shmid_array_add(&created_shmids, shmid);

  return shmid;
}

/*
 * Remove a shared memory object
 * shmid -  Shared memory identifier produced by ogle_shmget
 * Returns -1 on error and 0 on success
 */
int ogle_shmrm(int shmid)
{
  char shmname[64];

  if(!ogle_shm_initialized) {
    init_ogle_shm();
  }
  
  if(!use_posix_shm) {
    return ogle_sysv_shmrm(shmid);
  }
  
  DNOTE("Removing shmid: %08x\n", shmid);

  snprintf(shmname, sizeof(shmname),
	   "%s%08x", shmid_prefix, shmid);
  if(shm_unlink(shmname) == -1) {
    switch(errno) {
    case ENOSYS:
      //shm_open not implemented, revert ot sysv shm
      WARNING("ogle_shmrm, shm_unlink '%s': %s\n", shmname, strerror(errno));
      revert_to_sysv();
      return ogle_sysv_shmrm(shmid);
      break;
    default:
      ERROR("ogle_shmrm, shm_unlink '%s': %s\n", shmname, strerror(errno));
      break;
    }
  }
  
  shmid_array_remove(&created_shmids, shmid);

  return 0;
}


/*
 * Attach a shared memory object.
 * shmid - Shared memory identifier produced by ogle_shmget.
 * len - Will be filled in with the len of the object if call successful
 * otherwise won't be changed.
 * Returns the address that the shared memory is attached at or
 * (void *)-1 on error.
 */
void *ogle_shmat(int shmid)
{
  char shmname[64];
  void *shmaddr;
  struct stat statbuf;
  int fd;
  shm_seg_t *shm_p;

  if(!ogle_shm_initialized) {
    init_ogle_shm();
  }
  
  if(!use_posix_shm) {
    return ogle_sysv_shmat(shmid);
  }

  snprintf(shmname, sizeof(shmname),
	   "%s%08x", shmid_prefix, shmid);
  
  if((fd = shm_open(shmname, O_RDWR, 0)) == -1) {
    switch(errno) {
    case ENOSYS:
      //shm_open not implemented, revert ot sysv shm
      WARNING("ogle_shmat, shm_open '%s': %s\n", shmname, strerror(errno));
      revert_to_sysv();
      return ogle_sysv_shmat(shmid);
      break;
    default:
      ERROR("ogle_shmat, shm_open '%s': %s\n", shmname, strerror(errno));
      return (void *)-1;    
    }
  }
  
  if(fstat(fd, &statbuf) == -1) {
    ERROR("ogle_shmat, fstat: %s\n", strerror(errno));
    close(fd);
    return (void *)-1;
  }
  
  shmaddr = mmap(NULL, statbuf.st_size, PROT_READ | PROT_WRITE,
		 MAP_SHARED, fd, 0);
  if(shmaddr == MAP_FAILED) {
    ERROR("ogle_shmat, mmap: %s\n", strerror(errno));
    close(fd);
    return (void *)-1;
  }
  close(fd);



  /* put the shmaddr and len in a list so we can
   * get the len from the shmaddr in ogle_shmdt
   */
  shm_p = malloc(sizeof(shm_seg_t));
  shm_p->next = segment_list;
  shm_p->shmaddr = shmaddr;
  shm_p->len = statbuf.st_size;

  segment_list = shm_p;


  
  return shmaddr;
}


/*
 * Detach a shared memory object
 * shmaddr - address of an object attached with ogle_shmat
 * Returns 0 on success and -1 on error.
 */
int ogle_shmdt(void *shmaddr)
{
  shm_seg_t *p, **lp;
  int len = 0;

  if(!ogle_shm_initialized) {
    init_ogle_shm();
  }
  
  if(!use_posix_shm) {
    return ogle_sysv_shmdt(shmaddr);
  }

  /* find the shmaddr previously stored in the segment list */
  for(lp = &segment_list, p = NULL; *lp != NULL; lp = &((*lp)->next)) {
    if((*lp)->shmaddr == shmaddr) {
      p = *lp;
      len = p->len;
      *lp = (*lp)->next;
      free(p);
      break;
    }
  }
  
  if(p == NULL) {
    /* shmaddr was not in list */
    ERROR("%s", "ogle_shmdt: Address not attached\n");
    return -1;
  }

  if(munmap(shmaddr, len) == -1) {
    ERROR("ogle_shmdt: %s\n", strerror(errno));
    return -1;
  }
  
  return 0;
}

/*
 * Returns 1 if POSIX shm will be  used or 0 if SYSV shm.
 * Should be called first, before any other processes that use ogle_shm
 * are fork()'d
 */
int ogle_shm_init(void)
{
  char shmname[64];
  int fd;

  if(!ogle_shm_initialized) {
    init_ogle_shm();
  }
  
  if(!use_posix_shm) {
    return 0;
  }
  
  snprintf(shmname, sizeof(shmname),
	   "%s.%08x", shmid_prefix, getpid());
  
  if((fd = shm_open(shmname, O_RDWR | O_CREAT | O_EXCL, 0600)) == -1) {
    switch(errno) {
    case ENOSYS:
      //shm_open not implemented, revert to sysv shm
      WARNING("ogle_shm_init, shm_open: %s\n", strerror(errno));
      revert_to_sysv();
      return 0;
      break;
    default:
      ERROR("ogle_shm_init, shm_open '%s': %d %s\n",
	    shmname, errno, strerror(errno));
      break;
    }
  } else {
    close(fd);
    shm_unlink(shmname);      
  }

  //shm_open works
  
  return 1;
}

#else

int ogle_shm_init(void)
{
  return 0;
}

#endif //HAVE_POSIX_SHM

/*
 * Create a SYSV shared memory object.
 * sise - Requested size of the object.
 * mode - permissions of object.
 * Returns a shmid or (void *)-1 on error.
 */
int ogle_sysv_shmget(size_t size, int mode)
{
  int shmid;
  
  shmid = shmget(IPC_PRIVATE, size, mode | IPC_CREAT);
  if(shmid == -1) {
    ERROR("ogle_sysv_shmget: %s\n", strerror(errno));
  }

  shmid_array_add(&created_sysv_shmids, shmid);

  return shmid;
}

/*
 * Attach a SYSV shared memory object.
 * shmid - identifier of object from ogle_sysv_shmget.
 * Returns address where object is attached or (void *)-1 on error.
 */
void *ogle_sysv_shmat(int shmid)
{
  void *shmaddr;
  
  shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU);

  if(shmaddr == (void *)-1) {
    ERROR("ogle_sysv_shmat: %s\n", strerror(errno));
  }
  
  return shmaddr;
}

/*
 * Detach a SYSV shared memory object
 * shmaddr - address of object to detach
 * Returns 0 on success or -1 on error
 */
int ogle_sysv_shmdt(void *shmaddr)
{
  int r;

  r = shmdt(shmaddr);
  if(r == -1) {
    ERROR("ogle_sysv_shmdt: %s\n", strerror(errno));
  }

  return r;
}

/*
 * Remove a SYSV shared memory object
 * shmid - identifier of object to remove
 * Returns 0 on success or -1 on failure.
 */
int ogle_sysv_shmrm(int shmid)
{
  int r;

  DNOTE("Removing sysv shmid: %d\n", shmid);

  r = shmctl(shmid, IPC_RMID, NULL);
  if(r == -1) {
    ERROR("ogle_sysv_shmrm: %s\n", strerror(errno));
  }

  shmid_array_remove(&created_sysv_shmids, shmid);

  return r;
}


int ogle_shmrm_all(void)
{
  int n;
  
#ifdef HAVE_POSIX_SHM
  remove_all_shmids(&created_shmids, ogle_shmrm);
#endif
  
  remove_all_shmids(&created_sysv_shmids, ogle_sysv_shmrm);
  
  return 0;
}
