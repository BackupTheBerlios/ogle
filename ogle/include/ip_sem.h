#ifndef IP_SEM_H
#define IP_SEM_H

#ifdef USE_POSIX_SEM
#include <semaphore.h>
#endif

#ifdef USE_SYSV_SEM
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
     /* union semun is defined by including <sys/sem.h> */
#else
     /* according to X/OPEN we have to define it ourselves */
     union semun {
       int val;                    /* value for SETVAL */
       struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
       unsigned short int *array;  /* array for GETALL, SETALL */
#ifdef __linux__
       struct seminfo *__buf;      /* buffer for IPC_INFO */
#endif
     };
#endif
#endif

typedef struct {
#ifdef USE_POSIX_SEM
  sem_t bufs[2];
#endif
#ifdef USE_SYSV_SEM
  int semid_bufs;
#endif
} ip_sem_t;


int ip_sem_init(ip_sem_t *q_head, int init_nr[2]);
int ip_sem_wait(ip_sem_t *q_head, int sem_nr);
int ip_sem_post(ip_sem_t *q_head, int sem_nr);
int ip_sem_getvalue(ip_sem_t *q_head, int sem_nr);
int ip_sem_trywait(ip_sem_t *q_head, int sem_nr);


#endif /* IP_SEM_H */
