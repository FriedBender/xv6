#ifdef CS333_P2
#include "types.h"
#include "user.h"
#include "uproc.h"

int
main(int argc, char* argv[])
{
  int max = 64;
  struct uproc *table = malloc(max*sizeof(struct uproc));
  int active_processes = getprocs(max, table);

  if(active_processes < 0)
    printf(1, "There are no processes to display.");
  printf(1,"\nPID\tName         UID\tGID\tPPID\tElapsed\tCPU\tState\tSize\t PCs\n");
  for(int i = 0 ; i < active_processes ; ++i)
  {
    printf(1,"%d\t%s         %d\t%d\t%d\t%d.%d\t%d.%d\t%s\t%d\t ",
          table[i].pid, table[i].name, table[i].uid, table[i].gid,
          table[i].ppid, table[i].elapsed_ticks%1000, table[i].elapsed_ticks/1000,
          table[i].CPU_total_ticks/1000, table[i].CPU_total_ticks%1000, table[i].state,
          table[i].size
          );
  }
  
  exit();
}

#endif  //CS333_P2
