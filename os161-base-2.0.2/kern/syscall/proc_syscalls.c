/*
 * AUthor: G.Cabodi
 * Very simple implementation of sys__exit.
 * It just avoids crash/panic. Full process exit still TODO
 * Address space is released
 */

#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <kern/fcntl.h> //here it's defined O_RDONLY
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <lib.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
//#include <vm.h>
#include <vfs.h>
#include <mips/trapframe.h>
#include <current.h>
#include <synch.h>
#include <test.h>


/*
 * system calls for process management
 */
void
sys__exit(int status)
{
#if OPT_WAITPID
  struct proc *p = curproc;
  p->p_status = status & 0xff; /* just lower 8 bits returned */
  proc_remthread(curthread);
  proc_signal_end(p);
#else
  /* get address space of current process and destroy */
  struct addrspace *as = proc_getas();
  as_destroy(as);
#endif
  thread_exit();

  panic("thread_exit returned (should not happen)\n");
  (void) status; // TODO: status handling
}

int
sys_waitpid(pid_t pid, userptr_t statusp, int options)
{
#if OPT_WAITPID
  struct proc *p = proc_search_pid(pid);
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

#if OPT_FORK
static void
call_enter_forked_process(void *tfv, unsigned long dummy) {
  struct trapframe *tf = (struct trapframe *)tfv;
  (void)dummy;
  enter_forked_process(tf); 
 
  panic("enter_forked_process returned (should not happen)\n");
}

int sys_fork(struct trapframe *ctf, pid_t *retval) {
  struct trapframe *tf_child;
  struct proc *newp;
  int result;
  pid_t	pid;
  struct proc *debug =curproc;
  struct thread *thread = curthread;
  KASSERT(curproc != NULL&&thread !=NULL);
  KASSERT(debug != NULL);

  newp = proc_create_runprogram(curproc->p_name);
  if (newp == NULL) {
    return ENOMEM;
  }
  pid = newp->p_pid;
  proc_addChild(curproc,pid); 


  /* done here as we need to duplicate the address space 
     of thbe current process */
  tf_child = kmalloc(sizeof(struct trapframe));
  if(tf_child == NULL){
    proc_destroy(newp);
    return ENOMEM; 
  }
  memcpy(tf_child, ctf, sizeof(struct trapframe));
  as_copy(curproc->p_addrspace, &(newp->p_addrspace));
  if(newp->p_addrspace == NULL){
    proc_destroy(newp); 
    return ENOMEM; 
  }


  proc_file_table_copy(newp,curproc);

  /* we need a copy of the parent's trapframe */

  /* TO BE DONE: linking parent/child, so that child terminated 
     on parent exit */

  result = thread_fork(
		 curthread->t_name, newp,
		 call_enter_forked_process, 
		 (void *)tf_child, (unsigned long)0/*unused*/);

  if (result){
    proc_destroy(newp);
    kfree(tf_child);
    return ENOMEM;
  }

  *retval = pid;

  return 0;
}
#endif

#if OPT_EXECV
int
sys_execv(char *progname, char **args){
	//TODO check to free all the memory in case of error
	int i = 0;
	int result, argc, arglen;
	char **kargs;
	char **uargs;
	char *kprogname;
	struct vnode *v;
	struct addrspace *as;
	vaddr_t entrypoint, stackptr;
	//userptr_t userArgs;
	KASSERT(args !=NULL);
	size_t wasteOfSpace;
	
	//--------------------copy arguments from user space into kernel------------------------- 
	
	for(i=0;args[i]!=NULL;i++);
	kprintf("start  %d\n", i);
	KASSERT(args[i] == NULL);
	if(i >= ARG_MAX)
		return E2BIG;
	kargs = (char **) kmalloc(sizeof(char **) *i);
	if(kargs==NULL)
		return ENOMEM;
	i=0;
	while(args[i] != NULL){

		kargs[i] = kmalloc(strlen(args[i])+1);
		if(kargs[i] == NULL)	
			return ENOMEM;	
		

		result = copyinstr((userptr_t)args[i], kargs[i], strlen(args[i])+1,&wasteOfSpace);
		if(result){
			//kfree_all(kargs);
			kfree(kargs);
			return result;
		}
		i++;
	}
	kargs[i] = NULL;

	argc=i;  //keep trac of #arg
	
	kprogname = (char *) kmalloc(strlen(progname)+1);
	if(kprogname == NULL)	
		return ENOMEM;
	result = copyinstr((userptr_t)progname, kprogname, strlen(progname)+1, &wasteOfSpace);
	if(result){
		//kfree_all(kargs);
		kfree(kargs);
		kfree(kprogname);
		return result;
	}



	//-----------open the executable, create a new address space and load the elf into it--------
	

	/* We should be a new process. */
	/* Open the file. */
	result = vfs_open(kprogname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}
	proc_setas(NULL);
	KASSERT(proc_getas() == NULL);
	//KASSERT(curthread->t_addrspace == NULL);
	
	/* Create a new address space. */
	as = as_create();
	if (as == NULL) {		
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	proc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}
	/* Done with the file now. */
	vfs_close(v);
	
	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}

	//-----------------------copy the arguments from kernel buffer into user stack-----------
	uargs = (char **) kmalloc(sizeof(char **) * argc);
	if(uargs == NULL)	
		return ENOMEM;
	uargs[argc] = NULL;

	for ( i=0; i<argc; ++i){

		arglen = strlen(kargs[i])+1;
		uargs[i] =kmalloc(sizeof(char*));

		//TODO padding
		stackptr -= arglen;
		if(stackptr & 0x3)
			stackptr -= (stackptr & 0x3); //padding
	
		uargs[i] = (char *)stackptr; //saving the addresso of the stackptr for each element


		result = copyoutstr(kargs[i], (userptr_t)stackptr , arglen, &wasteOfSpace); //copy into the user stack the first elemente of kernel args 
	
		if(result){
			//kfree_all(kargs);
			kfree(kargs);
			return result;
			}
	}

	//userArgs = (userptr_t)stackptr;

	kprintf("here %d\n",argc);
	for ( i=0; i<argc; ++i){
		stackptr -= sizeof(char *); //move the stack pointer for an address
		result = copyout(uargs[argc-(i+1)], (userptr_t)stackptr , sizeof(char *)); // 
		kprintf("maremma %d, argc:%d\n",i,argc);
	
		if(result){
			kprintf("maiala\n");
			//kfree_all(kargs);
			kfree(kargs);
			return result;
		}

	}
	
	

		
	
	

	//return to user mode using enter_new_process
	/* Warp to user mode. */
	enter_new_process(argc /*argc*/, (userptr_t)stackptr /*(void*)argsuserspace addr of argv*/,
			  NULL /*userspace addr of environment*/,
			  stackptr, entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}
#endif


