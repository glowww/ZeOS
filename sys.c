/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>
#include <errno.h>

#include <mm_address.h>

#include <sched.h>

#define LECTURA 0
#define ESCRIPTURA 1

int check_fd(int fd, int permissions)
{
  if (fd!=1) return -9; /*EBADF*/
  if (permissions!=ESCRIPTURA) return -13; /*EACCES*/
  return 0;
}

int sys_ni_syscall()
{
	return -38; /*ENOSYS*/
}

int sys_getpid()
{
	return current()->PID;
}

int sys_fork()
{
  int PID=-1;

  // creates the child process
  
  return PID;
}

void sys_exit()
{  
}

#define buff 512
char sys_buffer[buff];
int sys_write(int fd, char * buffer, int size){
    int ret;
    int fd_valid = check_fd(fd, ESCRIPTURA);
    if (fd_valid != 0) return fd_valid; //checks whether file descriptor and operation are correct

    if (buffer == NULL) return -EFAULT;

    if (size < 0) return -EINVAL;

    int bytesLeft = size;
    while(bytesLeft > buff) {
	    copy_from_user(buffer,sys_buffer,size);
	    ret = sys_write_console(sys_buffer, buff);
	    bytesLeft -= ret;
	    buffer += ret;
    }

    if (bytesLeft > 0) {
	    copy_from_user(buffer,sys_buffer,size);
	    ret = sys_write_console(sys_buffer, buff);
	    bytesLeft -= ret;
    }
    int result = (size - bytesLeft);
    return result;
}
