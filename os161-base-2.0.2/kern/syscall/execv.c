/*
 * Authors: C.Michelagnoli, G.Piombino
 * Implementation of sys_execv.
 */
#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <proc.h>
#include <copyinout.h> 
#include <thread.h>
#include <current.h>
#include <addrspace.h>
#include <kern/fcntl.h> //here it's defined O_RDONLY
#include <vfs.h>
#include <mips/trapframe.h>
#include <synch.h>
#include <syscall.h>

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

	
	for ( i=0; i<argc; ++i){
		stackptr -= sizeof(char *); //move the stack pointer for an address
		result = copyout(uargs[argc-(i+1)], (userptr_t)stackptr , sizeof(char *)); // 
		
	
		if(result){
			//kprintf("maiala\n");
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

