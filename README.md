# OS161 - Project c2: Shell (SHELL)
> Source Code for the OS161. OS-161 is an instructional OS created by Harvard University as a standalone kernel and a simple userland, all written in C. 


---
The shell is a user program which works as a command interpreter. The main function of the command interpreter is to get and execute the next user-specified command.

### System calls for create, destroy, manage processes

- fork():  creates a new process (the child) that is a clone of the original (the parent).

- execv(): changes the program that a process is running.

- getpid(): It’s the simplest one, return the pid of the executing process.

- waitpid(): lets a process wait for another to terminate, and retrieve its exit status code.

- exit(): Causes the current process to terminate.

## Documentation

All code changes for this assignment can be enabled/disable by #if **OPT\_SHELLC2** statements.

- **execv()**: the first step consists in copying the argument from the user space to the kernel one by means of the copyinstr. Once done NULL is assigned to kargs after the last argument, so that the NULL value can be used as a string termination. Then also the progname is saved in the kernel by means of kprogname. Afterwards the file kprogname is opened with vsf\_open and a new address space is created, assigned to the proc and activated. Then the executable is loaded to set the entrypoint and the stack of the process is set thanks to as\_define\_stack. At this point it is possible to proceed by copying the arguments from the kernel to the user stack. Then the arguments are copied into the user stack by means of copyoutstr and at the end of the cycle the addresses of each argument. The stack is organised by putting the arguments at the bottom and the addresses at the top. In order to have the pointers aligned by 4 it is necessary to perform a padding. At last the function returns to the user mode using enter\_new\_process and passing the number of the arguments, the stack pointer and the entry point. In case the OS can’t manage a broken user program it calls kill\_curthread, contained in arch/mips/locore/trap.c. The function has been modified adding a sys\_\_exit().
- **fork()**: In the same way of common\_prog the first thing to do is to create a new process. The new process is added to the array of children owned by the curproc. proc\_addChild manipulates the curproc’s attribute p\_children of struct type array. After that we have to copy the address space(as\_copy()) of the father(curproc) into the new process. If all went well we just have to copy the file table of the father into the child. Eventually we call the thread\_fork() function passing the new process(copy of the father). A new thread is created in the new process, calling the function call\_enter\_forked\_process in the file arch/mips/syscall/syscall.c. The aim of enter\_forked\_process() is to activate the address space, prepare the passing trapframe and forward it to mips\_usermode, which allows the newborn thread to enter the user mode.
- **sys\_open()**: after checking if the path is not null and the flags are valid (this step is performed thanks to a function that masks the openflags and checks if only one of them is triggered), the function searches for a free space in the system file table. Then the path is copied in the kernel space and by means of the vfs\_open the file is opened. Now an openfile is created. To do so a lock is created, the vnode of the opened file and the mode are assigned and the references counter and the offset set (if the file already exists and needs to append the new data to the end of the file, thanks to VOP\_STAT is possible to recover the informations about the offset. Finally the openfile is assigned to the table of the process in the first free space. The function returns the indicator of the process file table which is the file descriptor. In case of error the file is closed.
- **sys\_close()**: first of all the function checks if the file descriptor is inside the boundaries. Then recover the openfile pointer from the process file table. At this point it is possible to reset everything while protecting the operations by means of a lock. At first it is set as null the pointer inside the process file table, then the references counter is decreased (if greater than zero the function doesn’t need to do anything else), the vnode of the openfile is set zero and the file is closed thanks to vsf\_close. Finally the lock is released and destroyed.
- **sys\_read()**: if the file descriptor is STDIN\_FILENO and OPT\_FILE is active, then the called function is file\_read. If the input is the buffer then it is possible to read by means of getch(). The function file\_read() allows the system to read from a file by means of the function VOP\_READ. To do so, first of all it has to recover the pointer of the vnode from the struct openfile pointed by the correspondent cell of the file table of the process. Then the struct uio under the variable ku and a kernel buffer are set thanks to uio\_kinit. After these operations, the offset of the openfile is updated, exploiting the information of the ku. Finally, the kernel buffer is transferred to the user space with the function copyout.
- **sys\_write()**: if the file descriptor is STDOUT\_FILENO or STDERR\_FILENO and the OPT\_FILE is active then it is called file\_write. If the output is an error or the buffer then it is simply used putch(). The function fule\_write allows the system to write a file by means of the function VOP\_WRITE. To do so, first of all it has to recover the pointer of the vnode from the struct openfile pointed by the correspondent cell of the file table of the process. Then the user buffer is copied in the kernel one. One last step before performing the write is to set the struct uio under the variable ku. Once done the offset of the struct openfile is updated and the number of written bytes is returned.

- **sys\_dup2()**: start with some checks on the two file descriptors received in input. We close the file corresponding to the new file descriptor(newfd) from the curproc’s filetable. After that we copy the old file descriptor(oldfd) into the curproc’s filetable at the index of newfd. Eventually we increment the reference count variable and return the new file descriptor.
- **sys\_chdir()**: first thing is to copy the value of path from user space to kernel(kbuf). Then we call the vfs\_chdir translate the path to a vnode and set the current directory with the value of kbuf.
- **sys\_\_\_getcwd()**: initialize an iovec and uio for kernel I/O and then pass it to vfs\_getcwd() that copies the name of the working directory from a kernel buffer to a data region defined by a uio struct.
- **sys\_lseek()**: the function simply sets the offset of the openfile, which is recovered from the process table. The offset can be different in relation to the whence, for this reason a cascade of if distinguishes the cases in set, cur and end. In the case of end, the end of the file is available thanks to the function VOP\_STAT. After this, the function checks that the file is seekable and finally sets the offset of the openfile. When the function is called by arch/mips/syscall/syscall.c it is used a different strategy since the offset is a 64 bit variable instead of 32, for this reason a variable offset e whence have been used. Moreover, to manage the result, it is loaded in retval64 and a flag handle64 is set, so that the result can be split in the fields tf\_v0 and tf\_v1 of trapframe tf.
- **sys\_waitpid()**: first of all the function searches for the pid in the process table, then waits for its termination by means of proc\_wait(). This function does a wait on the semaphore of the process which is meant to wait and finally destroys it. As last operation the status of the process is copied from the kernel space to the user one. The function returns the pid of the terminated process if the operation succeeds.
- **sys\_getpid()**: this function simply returns the pid of the curproc.
- **sys\_exit()**: if the OPT\_WAITPID is active the function sets the status of the current proc, removes the current thread from it and signals the semaphore of the curproc that the thread has been removed. Finally the current thread is terminated with thread\_exit().

## Testing

In order to test the OS several default tests have been used:

- `/testbin/farm`: farm uses the syscalls fork, execv, write, \_exit. Moreover the cat subprocess uses the open, read and close. In order to run this file properly it is necessary to create in pds-os161/root a file called catfile that contains an example of output. As output we expect to print the file catfile and a period of waiting due to the program testbin/hog.
- `/testbin/forktest`: uses fork, getpid, write and \_exit. As output we expect “testbin/forktest: Starting. Expecting this many: |--------------------------------------------------------------| AABBBBCCCCCCCCDDDDDDDDDDDDDDDD testbin/forktest: Complete.”
- `/testbin/palin`: uses write and \_exit.
- `/testbin/tail`: uses open, read, write, lseek, close and \_exit. This test is used mostly to test the lseek function. As input it requires the name of a file to be written and an offset. An example of run could be: “/testbin/tail catfile 0” The expected result of this test should be the same as the operation “cat catfile”.
- `/testbin/dup2test`: to use this test it is required to run the command bmake in the folder /testbin/dup2test. It is a test realized specifically for the function dup2().

To test the shell the operation “p bin/sh” is run. From this moment it will be possible to run the already listed tests and the commands(cat, cp, false, pwd, and true). To be able to test the correctness of commands, we created a directory "testDirectory" in which we have saved a file "testFile" with some random text.

Eventually we tested the command as shown below (all the following commands procedure are executed starting from the root directory):

- the <kbd>true</kbd> command has no output while <kbd>false</kbd> returns *“Exit 1”.*
- running the command <kbd>pwd</kbd> the output is *“emu0:”.*
- to verify the command *cat* we firstly entered in the *testDirectory* through the command <kbd>cd</kbd> *testDirectory*. Then we executed <kbd>cat</kbd> *testFile* obtaining as output the random text that we previously saved in *testFile.*
- the <kbd>cp</kbd> command was used to copy the content written in the *testFile* into a new file *cpTestFile.* To be sure of the correctness of this operation we executed again the <kbd>cat</kbd> command for the new file.cat
