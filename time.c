#ifdef CS333_P2
#include "user.h"
#include "types.h"


int
main(int argc , char* argv[])
{
  uint start_ticks, end_ticks, running_ticks, pid;
  start_ticks = uptime();
  pid = fork();
  //child does the work then returns
  if(pid == 0)  //child
  {
    if((exec(argv[1], &argv[1])) < 0)
    {
      printf(2, "There was an issue executing the function %s.\n", argv[1]);
    }
  }
  else
  {
    //parent
    wait();
    end_ticks = uptime();
    running_ticks = end_ticks - start_ticks;
    printf(1, "\n%s ran in %d.", argv[1], running_ticks/1000);

    if(running_ticks%1000 >= 100)
      printf(1,"%d seconds.\n", running_ticks%1000); 
    if(running_ticks%1000 < 100 && running_ticks%1000 >= 10)
      printf(1,"%d0 seconds.\n", running_ticks%1000); 
    if(running_ticks%1000 < 10)
      printf(1,"%d00 seconds.\n", running_ticks%1000); 
  }


  exit();
}
#endif  //CS333_P2
