#ifdef CS333_P2
#include "types.h"
#include "user.h"
#include "uproc.h"

int
main(int argc, char* argv[])
{
  static int MAXNAME = 7;
  int max = atoi(argv[1]);
  if(max == NULL)
    max = NPROC;
  struct uproc *table = (struct uproc*)malloc(max*sizeof(struct uproc));
  int active_processes = getprocs(max, table);

  if(active_processes < 0)
    printf(2, "There are no processes to display.");
  printf(1,"\nPID\tName\tUID\tGID\tPPID\tPrio\tElapsed\tCPU\tState\tSize\t\n");
  for(int i = 0 ; i < active_processes ; ++i)
  {
    int j = 0;
    printf(1,"%d\t", table[i].pid);
    int len = strlen(table[i].name);
    if(len > MAXNAME)
    {
      table[i].name[MAXNAME] = '\0';
      len = MAXNAME;
    }
    printf(1,"%s", table[i].name);
    for(j=len; j<=MAXNAME; j++)
    {
      printf(1, " ");
    }

    uint milliseconds = table[i].elapsed_ticks%1000;
    uint cpu_milliseconds = table[i].CPU_total_ticks%1000;

    printf(1,"%d\t%d\t%d\t%d\t%d",
        table[i].uid, 
        table[i].gid,
        table[i].ppid, 
        table[i].priority,
        table[i].elapsed_ticks/1000
        );

    if(milliseconds >= 100)
      printf(1,".%d\t", milliseconds);
    else if(milliseconds < 100 || milliseconds >= 10)
      printf(1,".%d0\t", milliseconds);
    else if(milliseconds < 10)
      printf(1,".%d00\t", milliseconds);

    printf(1,"%d",table[i].CPU_total_ticks/1000);
    if(cpu_milliseconds >= 100)
      printf(1,".%d\t", cpu_milliseconds);
    else if(cpu_milliseconds < 100 || cpu_milliseconds >= 10)
      printf(1, ".0%d\t", cpu_milliseconds);
    else if(cpu_milliseconds < 10)
      printf(1,".00%d\t", cpu_milliseconds);

    printf(1,"%s\t%d\n",
        table[i].state,
        table[i].size
        );
  }

  exit();
}

#endif  //CS333_P2
