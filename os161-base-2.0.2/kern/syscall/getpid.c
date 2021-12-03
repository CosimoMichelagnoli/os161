/*
 * Authors: C.Michelagnoli, G.Piombino
 * Implementation of sys_getpid.
 */
#include <types.h>
#include <lib.h>
#include <proc.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>

pid_t
sys_getpid(void)
{
#if OPT_WAITPID
  KASSERT(curproc != NULL);
  return curproc->p_pid;
#else
  return -1;
#endif
}
