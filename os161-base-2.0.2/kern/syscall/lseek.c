/*
 * AUthors: C.Michelagnoli, G.Piombino
 * Implementation of sys_waitpid.
 */
#include <types.h>
#include <lib.h>
#include <proc.h>
#include <kern/errno.h>
#include <syscall.h>
#include <current.h>
#if OPT_FILE
#include <vnode.h>
#include <vfs.h>
#include <kern/stat.h>
#include <kern/fcntl.h>
#include <kern/seek.h>
#endif
int
sys_lseek(int fd, off_t offset, int whence, off_t *retval){
#if OPT_FILE
	/*if whence is:
	SEEK_SET -> the new position is offset
	SEEK_CUR -> the new position is current + offset
	SEEK_END -> the new position is end-of-file + offset
	else FAIL*/
	/*struct vnode *vn;
	int err;*/
	struct openfile *of = NULL;
	off_t newoffset;
	
	//fd is not a valid file handle
	if (fd<0||fd>OPEN_MAX) return EBADF;
  	of = curproc->fileTable[fd];
  	if ((of==NULL) || (of->countRef == 0)) return EBADF;

	if(whence == SEEK_SET){
		newoffset = offset;
	} else if(whence == SEEK_CUR){
			newoffset = of->offset + offset;
	} else if(whence == SEEK_END){
			struct stat *file_stat = kmalloc(sizeof(struct stat));
			VOP_STAT(of->vn, file_stat);
			newoffset = file_stat->st_size + offset;
	} else{
		return EINVAL;
	}
	if (newoffset < 0) /*|| (newoffset > EOF)*/ return EINVAL;


	/*vn = of->vn;
	err = VOP_TRYSEEK(vn, newoffset);
	if (err) return err;*/
	
	of->offset = newoffset;
	*retval = newoffset;

	return 0;

#endif
}
