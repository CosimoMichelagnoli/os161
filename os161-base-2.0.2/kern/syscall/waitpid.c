/*
 * AUthors: C.Michelagnoli, G.Piombino
 * Implementation of sys_waitpid.
 */

#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <proc.h>
#include <copyinout.h> 
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <syscall.h>

int
sys_waitpid(pid_t pid, userptr_t statusp, int options)
{
  if(((uintptr_t)statusp % 4) != 0) return EFAULT;

  if(statusp == NULL) return EFAULT;

  if(pid < 0 || pid >= PID_MAX) return ESRCH;

  if(options != 0 && options != 1 && options != 2) return EINVAL;

#if OPT_WAITPID

  struct proc *p = proc_search_pid(pid);

  if(p->p_proc != curproc) return ECHILD;

  int s;
  (void)options; /* not handled */

  if (p==NULL) return -1;

  s = proc_wait(p);

  if (statusp!=NULL) 
    *(int*)statusp = s;
  return pid;
#else
  (void)options; /* not handled */
  (void)pid;
  (void)statusp;
  return -1;
#endif
}
