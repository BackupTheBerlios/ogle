#include <stdio.h>
#include <stdlib.h>

#include "../include/ip_sem.h"



int ip_sem_init(ip_sem_t *q_head, int init_nr[2]) {
  int i;
  
#if defined USE_POSIX_SEM
  
  fprintf(stderr, "sem_init\n");
  for(i = 0; i < 2; i++) {
    if(sem_init(&q_head->bufs[i], 1, init_nr[i]) == -1) {
      perror("create_q(), sem_init()");
      return -1;
    }
  }
  return 0;
  
#elif defined USE_SYSV_SEM

  if((q_head->semid_bufs =
      semget(IPC_PRIVATE, 2, (IPC_CREAT | IPC_EXCL | 0700))) == -1) {
    perror("create_q(), semget(semid_bufs)");
    return -1;
  } else {
    union semun arg;
    
    for(i = 0; i < 2; i++) {
      arg.val = init_nr[i];
      if(semctl(q_head->semid_bufs, i, SETVAL, arg) == -1) {
	perror("create_q() semctl()");
	return -1;
      }
    }
  }
  return 0;
  
#else
#error No semaphore type set
#endif

}



int ip_sem_wait(ip_sem_t *q_head, int sem_nr) {

#if defined USE_POSIX_SEM  
  
  if(sem_wait(&q_head->bufs[sem_nr]) == -1) {
    perror(": get_q(), sem_wait()");
    return -1;
  }
  return 0;
  
#elif defined USE_SYSV_SEM
  
  {
    struct sembuf sops;
    sops.sem_num = sem_nr;
    sops.sem_op = -1;
    sops.sem_flg = 0;

    if(semop(q_head->semid_bufs, &sops, 1) == -1) {
      perror(": get_q(), semop() wait");
      return -1;
    }
  }
  return 0;
  
#else
#error No semaphore type set
#endif

}


int ip_sem_post(ip_sem_t *q_head, int sem_nr) {

#if defined USE_POSIX_SEM
  
  if(sem_post(&q_head->bufs[sem_nr]) == -1) {
    perror(": get_q(), sem_post()");
    return -1;
  }
  return 0;

#elif defined USE_SYSV_SEM
  
  {
    struct sembuf sops;
    sops.sem_num = sem_nr;
    sops.sem_op = 1;
    sops.sem_flg = 0;
    
    if(semop(q_head->semid_bufs, &sops, 1) == -1) {
      perror("ac3: get_q(), semop() post");
      return -1;
    }
  }
  return 0;
  
#else
#error No semaphore type set
#endif

}

