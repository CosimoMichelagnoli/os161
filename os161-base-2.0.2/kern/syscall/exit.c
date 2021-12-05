/*
 * Authors: C.Michelagnoli, G.Piombino
 * Implementation of sys__exit.
 */
#include <types.h>
#include <lib.h>
#include <proc.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>



void
sys__exit(int status)
{
  KASSERT(curproc != NULL);
#if OPT_WAITPID
  struct proc *p = curproc;
  spinlock_acquire(&p->p_lock);
  p->p_status = status & 0xff; /* just lower 8 bits returned */
  spinlock_release(&p->p_lock);
  proc_remthread(curthread);
  p->exited = true;
  proc_signal_end(p);

#else
  /* get address space of current process and destroy */
  struct addrspace *as = proc_getas();
  as_destroy(as);
#endif
  thread_exit();

  panic("thread_exit returned (should not happen)\n");
  (void) status; 
}
