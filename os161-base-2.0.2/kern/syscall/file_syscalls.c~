/*
 * AUthor: G.Cabodi
 * Very simple implementation of sys_read and sys_write.
 * just works (partially) on stdin/stdout
 */

#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <clock.h>
#include <syscall.h>
#include <current.h>
#include <lib.h>
#include <opt-file.h>
#include <synch.h>

#if OPT_FILE

#include <copyinout.h>
#include <vnode.h>
#include <vfs.h>
#include <limits.h>
#include <uio.h>
#include <proc.h>
//#include <kern/stdio.h>
#include <kern/fcntl.h>
#include <kern/seek.h>
#include <kern/stat.h>

/* max num of system wide open files */
#define SYSTEM_OPEN_MAX (10*OPEN_MAX)

#define USE_KERNEL_BUFFER 0

#define MAX_DIR_LEN 128

/* system open file table */
struct openfile {
  struct vnode *vn;
  off_t offset;	
  int accmode;
  unsigned int countRef;
  struct lock *lk;
};

struct openfile systemFileTable[SYSTEM_OPEN_MAX];

void openfileIncrRefCount(struct openfile *of) {
  if (of!=NULL)
    of->countRef++;
}

#if USE_KERNEL_BUFFER

static int
file_read(int fd, userptr_t buf_ptr, size_t size) {
  struct iovec iov;
  struct uio ku;
  int result, nread;
  struct vnode *vn;
  struct openfile *of;
  void *kbuf;

  if (fd<0||fd>OPEN_MAX) return -1;
  of = curproc->fileTable[fd];
  if (of==NULL) return -1;
  vn = of->vn;
  if (vn==NULL) return -1;

  kbuf = kmalloc(size);
  uio_kinit(&iov, &ku, kbuf, size, of->offset, UIO_READ);
  result = VOP_READ(vn, &ku);
  if (result) {
    return result;
  }
  of->offset = ku.uio_offset;
  nread = size - ku.uio_resid;
  copyout(kbuf,buf_ptr,nread);
  kfree(kbuf);
  return (nread);
}

static int
file_write(int fd, userptr_t buf_ptr, size_t size) {
  struct iovec iov;
  struct uio ku;
  int result, nwrite;
  struct vnode *vn;
  struct openfile *of;
  void *kbuf;

  if (fd<0||fd>OPEN_MAX) return -1;
  of = curproc->fileTable[fd];
  if (of==NULL) return -1;
  vn = of->vn;
  if (vn==NULL) return -1;

  kbuf = kmalloc(size);
  copyin(buf_ptr,kbuf,size);
  uio_kinit(&iov, &ku, kbuf, size, of->offset, UIO_WRITE);
  result = VOP_WRITE(vn, &ku);
  if (result) {
    return result;
  }
  kfree(kbuf);
  of->offset = ku.uio_offset;
  nwrite = size - ku.uio_resid;
  return (nwrite);
}

#else

static int
file_read(int fd, userptr_t buf_ptr, size_t size) {
  struct iovec iov;
  struct uio u;
  int result;
  struct vnode *vn;
  struct openfile *of;

  if (fd<0||fd>OPEN_MAX) return -1;
  of = curproc->fileTable[fd];
  if (of==NULL) return -1;
  vn = of->vn;
  if (vn==NULL) return -1;

  iov.iov_ubase = buf_ptr;
  iov.iov_len = size;

  u.uio_iov = &iov;
  u.uio_iovcnt = 1;
  u.uio_resid = size;          // amount to read from the file
  u.uio_offset = of->offset;
  u.uio_segflg =UIO_USERISPACE;
  u.uio_rw = UIO_READ;
  u.uio_space = curproc->p_addrspace;

  result = VOP_READ(vn, &u);
  if (result) {
    return result;
  }

  of->offset = u.uio_offset;
  return (size - u.uio_resid);
}

static int
file_write(int fd, userptr_t buf_ptr, size_t size) {
  struct iovec iov;
  struct uio u;
  int result, nwrite;
  struct vnode *vn;
  struct openfile *of;

  if (fd<0||fd>OPEN_MAX) return -1;
  of = curproc->fileTable[fd];
  if (of==NULL) return -1;
  vn = of->vn;
  if (vn==NULL) return -1;

  iov.iov_ubase = buf_ptr;
  iov.iov_len = size;

  u.uio_iov = &iov;
  u.uio_iovcnt = 1;
  u.uio_resid = size;          // amount to read from the file
  u.uio_offset = of->offset;
  u.uio_segflg =UIO_USERISPACE;
  u.uio_rw = UIO_WRITE;
  u.uio_space = curproc->p_addrspace;

  result = VOP_WRITE(vn, &u);
  if (result) {
    return result;
  }
  of->offset = u.uio_offset;
  nwrite = size - u.uio_resid;
  return (nwrite);
}

#endif

/*
 * file system calls for open/close
 */
bool
valid_flags(int flags){
	int count = 0;

	if((flags & O_RDONLY) == O_RDONLY) count++;

	if((flags & O_WRONLY) == O_WRONLY) count++;

	if((flags & O_RDWR) == O_RDWR) count ++;

	return count == 1;
}

int
sys_open(userptr_t path, int openflags, mode_t mode, int *errp)
{
  int fd, i;
  char kbuf[NAME_MAX];
  struct vnode *v;
  struct openfile *of=NULL;; 	
  int result;
  size_t actual;

  if(path == NULL){
	*errp = EFAULT;
	return -1;
  }
  
  if(valid_flags(openflags)){//too bee fixed
	   
	  /* search system open file table */
  	  for (i=0; i<SYSTEM_OPEN_MAX; i++) 
	    if (systemFileTable[i].vn==NULL) break;

	  if(i >= SYSTEM_OPEN_MAX){
		*errp = ENFILE;
		return -1;
	  } 

          if((result = copyinstr(path, kbuf, NAME_MAX, &actual)) != 0) return -1;
		  

	  result = vfs_open(kbuf, openflags, mode, &v);
	  if (result) {
	    *errp = ENOENT;
	    return -1;
	  }
	   
	  of = &systemFileTable[i]; // kmalloc(sizeof(struct openfile));

	  if (of==NULL) { 
		    // no free slot in system open file table
		    *errp = ENFILE;
	  }
	  else {
		  of->lk = lock_create("file lock");
		  if(of->lk == NULL){
		  	vfs_close(v);
			*errp = ENOMEM;
			kfree(of);
			return -1;		  
		  }
		  of->vn = v;
		  of->countRef = 1;
		  of->offset = 0;
		  of->accmode = openflags & O_ACCMODE;

		  if((openflags & O_APPEND) == O_APPEND){
			   struct stat file_stat;
			   VOP_STAT(of->vn, &file_stat);
			   of->offset = file_stat.st_size;
		  }		      
		  
		  for (fd=STDERR_FILENO+1; fd<OPEN_MAX; fd++) {
		      if (curproc->fileTable[fd] == NULL) {
				curproc->fileTable[fd] = of;
				return fd;
		      }
		    }
		    // no free slot in process open file table
		    *errp = EMFILE;
	  }
	  
	  vfs_close(v);
	  return -1;
  }
  else {
	*errp = EINVAL;
	return -1;
  }
}

/*
 * file system calls for open/close
 */
int
sys_close(int fd)
{
  struct openfile *of=NULL; 
  struct vnode *vn;

  if (fd<0||fd>OPEN_MAX) return -1;
  of = curproc->fileTable[fd];
  if (of==NULL) return -1;
  curproc->fileTable[fd] = NULL;

  if (--of->countRef > 0) return 0; // just decrement ref cnt
  vn = of->vn;
  of->vn = NULL;
  if (vn==NULL) return -1;

  vfs_close(vn);	
  return 0;
}

#endif

/*
 * simple file system calls for write/read
 */
int
sys_write(int fd, userptr_t buf_ptr, size_t size)
{
  int i;
  char *p = (char *)buf_ptr;

  if (fd!=STDOUT_FILENO && fd!=STDERR_FILENO) {
#if OPT_FILE
    return file_write(fd, buf_ptr, size);
#else
    kprintf("sys_write supported only to stdout\n");
    return -1;
#endif
  }

  for (i=0; i<(int)size; i++) {
    putch(p[i]);
  }

  return (int)size;
}

int
sys_read(int fd, userptr_t buf_ptr, size_t size)
{
  int i;
  char *p = (char *)buf_ptr;

  if (fd!=STDIN_FILENO) {
#if OPT_FILE
    return file_read(fd, buf_ptr, size);
#else
    kprintf("sys_read supported only to stdin\n");
    return -1;
#endif
  }

  for (i=0; i<(int)size; i++) {
    p[i] = getch();
    if (p[i] < 0) 
      return i;
  }

  return (int)size;
}


int 
sys_dup2(int oldfd, int newfd)
{
#if OPT_FILE
	struct proc *p=NULL;
	struct openfile *fold,*fnew;

	KASSERT( curthread!=NULL);
	KASSERT( curthread->t_proc!=NULL);

	p=curthread->t_proc;

	if( (oldfd<0 || newfd<0 || oldfd>OPEN_MAX || newfd>OPEN_MAX))
		return EBADF;

	fold=p->fileTable[oldfd];
	fnew=p->fileTable[newfd];
	if(fnew!=NULL)
	{
		sys_close(newfd);
	}
	fnew=fold;


	p->fileTable[newfd]=fnew;

	fold->countRef++;
	VOP_INCREF(fold->vn);

	return newfd;
#else
    kprintf("sys_dup2 not supported\n");
    return -1;

#endif

}

int
sys_chdir(userptr_t path){
#if OPT_FILE
	char kbuf[MAX_DIR_LEN];
	int err;

	KASSERT(curthread!=NULL);
	KASSERT(curthread->t_proc!=NULL);

	err=copyinstr(path,kbuf,strlen((char*)path)*sizeof(char),NULL);
	if(err)
		return err;

	err=vfs_chdir(kbuf);
	if(err)
		return err;

	return 0;
#else
	kprintf("sys_chdir not supported\n");
	return -1;

#endif

}

int
sys___getcwd(userptr_t buf,size_t len,int *retval){
#if OPT_FILE
	struct uio ruio;
	struct iovec iov;
	int err;

	KASSERT(curthread!=NULL);
	KASSERT(curthread->t_proc!=NULL);

	uio_kinit(&iov,&ruio,buf,len,0,UIO_READ);

	//the given pointer lives in userspace
	ruio.uio_space=curthread->t_proc->p_addrspace;
	ruio.uio_segflg=UIO_USERSPACE;

	err=vfs_getcwd(&ruio);
	if(err)
		return err;

	*retval=len-ruio.uio_resid;
	return 0;

#else
	kprintf("sys___getcwd not supported\n");
	return -1;
#endif

}

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
