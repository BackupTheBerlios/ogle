#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <time.h>


int main()
{
  struct timespec t1, t2, t3, t4, t5, res, res_1_2, res2, zero;

  zero.tv_sec = 0;
  zero.tv_nsec = 0;

  clock_getres(CLOCK_REALTIME, &res);
  fprintf(stderr, "res: %ld.%09ld\n",
	  t1.tv_sec, t1.tv_nsec);
  res_1_2.tv_sec = 0;
  res_1_2.tv_nsec = res.tv_nsec/2;

  res2.tv_sec = 0;
  res2.tv_nsec = res.tv_nsec*2-res.tv_nsec/2;

  
  
  clock_gettime(CLOCK_REALTIME, &t1);
  clock_gettime(CLOCK_REALTIME, &t2);
  clock_gettime(CLOCK_REALTIME, &t3);
  clock_gettime(CLOCK_REALTIME, &t4);
  clock_gettime(CLOCK_REALTIME, &t5);
  
  
  fprintf(stderr, "t1: %ld.%09ld\n",
	  t1.tv_sec, t1.tv_nsec);	  
  fprintf(stderr, "t2: %ld.%09ld\n",
	  t2.tv_sec, t2.tv_nsec);	  
  fprintf(stderr, "t3: %ld.%09ld\n",
	  t3.tv_sec, t3.tv_nsec);	  
  fprintf(stderr, "t4: %ld.%09ld\n",
	  t4.tv_sec, t4.tv_nsec);	  
  fprintf(stderr, "t5: %ld.%09ld\n",
	  t5.tv_sec, t5.tv_nsec);	  


  clock_gettime(CLOCK_REALTIME, &t1);
  nanosleep(&zero, NULL);
  clock_gettime(CLOCK_REALTIME, &t3);
  
  
  fprintf(stderr, "t1: %ld.%09ld\n",
	  t1.tv_sec, t1.tv_nsec);	  
  fprintf(stderr, "zero: %ld.%09ld\n",
	  zero.tv_sec, zero.tv_nsec);	  
  fprintf(stderr, "t3: %ld.%09ld\n",
	  t3.tv_sec, t3.tv_nsec);	  


  clock_gettime(CLOCK_REALTIME, &t1);
  nanosleep(&res_1_2, NULL);
  clock_gettime(CLOCK_REALTIME, &t3);
  
  
  fprintf(stderr, "t1: %ld.%09ld\n",
	  t1.tv_sec, t1.tv_nsec);	  
  fprintf(stderr, "res_1_2: %ld.%09ld\n",
	  res_1_2.tv_sec, res_1_2.tv_nsec);	  
  fprintf(stderr, "t3: %ld.%09ld\n",
	  t3.tv_sec, t3.tv_nsec);	  


  clock_gettime(CLOCK_REALTIME, &t1);
  nanosleep(&res, NULL);
  clock_gettime(CLOCK_REALTIME, &t3);
  
  
  fprintf(stderr, "t1: %ld.%09ld\n",
	  t1.tv_sec, t1.tv_nsec);	  
  fprintf(stderr, "res: %ld.%09ld\n",
	  res.tv_sec, res.tv_nsec);	  
  fprintf(stderr, "t3: %ld.%09ld\n",
	  t3.tv_sec, t3.tv_nsec);	  

  clock_gettime(CLOCK_REALTIME, &t1);
  nanosleep(&res2, NULL);
  clock_gettime(CLOCK_REALTIME, &t3);
  
  
  fprintf(stderr, "t1: %ld.%09ld\n",
	  t1.tv_sec, t1.tv_nsec);	  
  fprintf(stderr, "res2: %ld.%09ld\n",
	  res2.tv_sec, res2.tv_nsec);	  
  fprintf(stderr, "t3: %ld.%09ld\n",
	  t3.tv_sec, t3.tv_nsec);	  


  res2.tv_nsec = 3000000;
  clock_gettime(CLOCK_REALTIME, &t1);
  nanosleep(&res2, NULL);
  clock_gettime(CLOCK_REALTIME, &t3);
  
  
  fprintf(stderr, "t1: %ld.%09ld\n",
	  t1.tv_sec, t1.tv_nsec);	  
  fprintf(stderr, "3ms: %ld.%09ld\n",
	  res2.tv_sec, res2.tv_nsec);	  
  fprintf(stderr, "t3: %ld.%09ld\n",
	  t3.tv_sec, t3.tv_nsec);	  




  res2.tv_nsec = 7000000;
  clock_gettime(CLOCK_REALTIME, &t1);
  nanosleep(&res2, NULL);
  clock_gettime(CLOCK_REALTIME, &t3);
  
  
  fprintf(stderr, "t1: %ld.%09ld\n",
	  t1.tv_sec, t1.tv_nsec);	  
  fprintf(stderr, "7ms: %ld.%09ld\n",
	  res2.tv_sec, res2.tv_nsec);	  
  fprintf(stderr, "t3: %ld.%09ld\n",
	  t3.tv_sec, t3.tv_nsec);	  


  
  
}


/*
  timing on ultrasparc60 solaris
  

  res = 0.01
  clock_gettime ~ 0.000000250, < 0.000001
  
  nanosleep(0)      .000019941
  nanosleep(res/2)  .013630529
  nanosleep(res)    .019684518
  nanosleep(res*2)  .029692460


 */
