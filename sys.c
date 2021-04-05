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
#include <entry.h>
#include <stats.h>

#define LECTURA 0
#define ESCRIPTURA 1

extern long int zeos_ticks;
extern int MAX_PID;
extern int quantum;
extern struct list_head freequeue, readyqueue, readqueue;
extern int dir_pages_refs[NR_TASKS];

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
  // Check if there's space for a new process
  if (list_empty(&freequeue)) return -EAGAIN; 

  // Get a free task_struct for the process
  struct list_head *listHead = list_first(&freequeue);

  struct task_struct *new_task = list_head_to_task_struct(listHead);
  union task_union *new_task_union = (union task_union *)new_task;

  struct task_struct *current_task = current();
  union task_union *current_task_union = (union task_union *)current_task;

  // Delete process from freequeue list
  list_del(listHead);
  
  // Inherit system data: copy the parent’s task_union to the child
  copy_data(current_task_union, new_task_union, (int) sizeof(union task_union));

  // Initialize field dir_pages_baseAddr with a new directory to store the process address space
  if(allocate_DIR(new_task) < 0) return -ENOMEM;
  
  int page;
  int frames[NUM_PAG_DATA];
  // Search physical pages in which to map logical pages for data+stack of the child process
  for (page=0; page < NUM_PAG_DATA; page++) {
    frames[page] = alloc_frame();
    if (frames[page] == -1) {
      while (page > 0) free_frame(frames[--page]);
      return -ENOMEM;
    }
  }

  // Create new address space
  page_table_entry *new_pt = get_PT(new_task);
  page_table_entry *current_pt = get_PT(current_task);

  // Page table entries for the system code and data and for the user code 
  // can be a copy of the page table entries of the parent process
  for (page=0; page < NUM_PAG_CODE; page++) {
    new_pt[PAG_LOG_INIT_CODE+page].entry = current_pt[PAG_LOG_INIT_CODE+page].entry;
  }

  // Page table entries for the user data+stack have to  
  // point to new allocated pages which hold this region
  for (page=0; page < NUM_PAG_DATA; page++) {
		set_ss_pag(new_pt, PAG_LOG_INIT_DATA+page, frames[page]);
	}

  // Copy the user data+stack pages from the parent process to the child process
  for (page=0; page < NUM_PAG_DATA; page++) {
    // Use temporal free entries on the page table of the parent
    set_ss_pag(
      current_pt, 
      PAG_LOG_INIT_DATA+NUM_PAG_DATA+page, 
      frames[page]
    );

    // Copy data+stack pages
    copy_data(
      (int*)((PAG_LOG_INIT_DATA+page)<<12),
      (int*)((PAG_LOG_INIT_DATA+NUM_PAG_DATA+page)<<12),
      PAGE_SIZE
    );

    // Free temporal entries in the page table 
    del_ss_pag(
      current_pt, 
      PAG_LOG_INIT_DATA+NUM_PAG_DATA+page
    );
  }

  // Flush the TLB to really disable the parent process to access the child pages
  set_cr3(get_DIR(current_task));
  
  // Assign a new PID to the process
  new_task->PID = MAX_PID++;

  int index = (getEbp() - (int) current_task) / sizeof(int);
  
  // Prepare the child stack with a content that 
  // emulates the result of a call to task_switch
  new_task_union->stack[index-1] = 0;
  new_task_union->stack[index] = (int) ret_from_fork;

  // Initialize the fields of the task_struct 
  // that are not common to the child
  new_task->esp_register= &new_task_union->stack[index-1];

  // Insert the new process into the readyqueue list
  list_add_tail(new_task, &readyqueue); 

  // Return the pid of the child process
  return new_task->PID;
}

void sys_exit()
{
  struct task_struct * task = current();
  task->PID = -1;	
  
  int pos = ((int) task->dir_pages_baseAddr - (int) dir_pages) /(sizeof(page_table_entry)*1024);

  if(!(--dir_pages_refs[pos])) {
      page_table_entry * pt = get_PT(task);
      int page;
      for(page = 0; page < NUM_PAG_DATA; page++){
          free_frame(pt[PAG_LOG_INIT_DATA+page].bits.pbase_addr);
          del_ss_pag(pt, PAG_LOG_INIT_DATA+page);
      }
  }
  schedule();
}

int sys_gettime(){
	return zeos_ticks;
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
	    copy_from_user(buffer, sys_buffer, bytesLeft);
	    ret = sys_write_console(sys_buffer, bytesLeft);
	    bytesLeft -= ret;
    }
    int result = (size - bytesLeft);
    return result;
}

int sys_get_stats(int pid, struct stats *st){
    
    if(!access_ok(VERIFY_WRITE, st, sizeof(struct stats))) return -EFAULT;
    if (pid < 0) return -EINVAL;

    int i;
    struct task_struct *t;
    for (t = &(task[i=0].task); i < NR_TASKS; t = &(task[++i].task)){
        if (t->PID == pid){
            t->stats.remaining_ticks = quantum;
            copy_to_user(&(t->stats), st, sizeof(struct stats));	
            return 0;
        }
    }	
    return -ESRCH;
}
