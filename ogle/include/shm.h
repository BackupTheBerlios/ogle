#ifndef OGLE_SHM_H
#define OGLE_SHM_H

#ifdef HAVE_POSIX_SHM

int ogle_shmget(int size, int mode);
int ogle_shmrm(int shmid);
void *ogle_shmat(int shmid);
int ogle_shmdt(void *shmaddr);

#else

#define ogle_shmget(size, mode) ogle_sysv_shmget((size), (mode))
#define ogle_shmat(shmid) ogle_sysv_shmat((shmid))
#define ogle_shmdt(shmaddr) ogle_sysv_shmdt((shmaddr))
#define ogle_shmrm(shmid) ogle_sysv_shmrm((shmid))

#endif //HAVE_POSIX_SHM

int ogle_sysv_shmget(size_t size, int mode);
void *ogle_sysv_shmat(int shmid);
int ogle_sysv_shmdt(void *shmaddr);
int ogle_sysv_shmrm(int shmid);

int ogle_shmrm_all(void);

#endif //OGLE_SHM_H
