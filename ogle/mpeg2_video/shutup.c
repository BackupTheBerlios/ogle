/* 
 * This is a simple wrapper to test the xscreensaver-interfacing
 * functions.
 *
 *             /August.
 */


#include "xscreensaver-comm.c"

int
main()
{
  printf("A dot should appear every 50 seconds.\n\n");
  
  if(! look_for_good_xscreensaver())
    {
      printf("Found no screensaver!\n");
      exit(1);
    }
   else
    { 
      printf("Found a working screensaver.\n");

      while (1)
       {
         fprintf(stderr, ".");
         sleep(50);
         nudge_xscreensaver();
       }
    }
  exit(0);
}
