/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>

union task_union task[NR_TASKS]
  __attribute__((__section__(".data.task")));


struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, list);
}

extern TSS tss; 
extern struct list_head blocked;
struct list_head freequeue;
struct list_head readyqueue;
struct task_struct *idle_task;

void setEsp(DWord * data);

DWord getEbp();

/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry * get_DIR (struct task_struct *t) 
{
	return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry * get_PT (struct task_struct *t) 
{
	return (page_table_entry *)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr))<<12);
}


int allocate_DIR(struct task_struct *t) 
{
	int pos;

	pos = ((int)t-(int)task)/sizeof(union task_union);

	t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[pos]; 

	return 1;
}

void cpu_idle(void)
{
	__asm__ __volatile__("sti": : :"memory");

	while(1)
	{
	;
	}
}

void init_idle (void)
{
	struct list_head * idleTaskUnionLH = list_first(&freequeue);
	list_del (idleTaskUnionLH);
	struct task_struct * idleTaskStruct = list_head_to_task_struct(idleTaskUnionLH); //get task_struct
  	union task_union * idleTaskUnion = (union task_union*)idleTaskStruct;			//get task_union
	idleTaskStruct -> PID = 0; 
	allocate_DIR(idleTaskStruct);

	//INITIALIZE CONTEXT TO RESTORE
	idleTaskUnion->stack[KERNEL_STACK_SIZE - 1] = &cpu_idle; //RETURN ADDRESS
	idleTaskUnion->stack[KERNEL_STACK_SIZE - 2] = 0; //REGISTER EBP
	idleTaskStruct->esp_register = &(idleTaskUnion->stack[KERNEL_STACK_SIZE - 2]);

	idle_task = idleTaskStruct;
}

//FIRST USER PROCESS TO BE EXECUTED AFTER BOOTING THE OS
void init_task1(void) //parent of all processes of the system
{
	struct list_head * initTaskUnionLH = list_first(&freequeue);
	list_del (initTaskUnionLH);
	struct task_struct * initTaskStruct = list_head_to_task_struct(initTaskUnionLH); //get task_struct
	union task_union * initTaskUnion = (union task_union*)initTaskStruct;
	initTaskStruct -> PID = 1;
	allocate_DIR(initTaskStruct);

	set_user_pages(initTaskStruct); //inits pages for process

	tss.esp0 = KERNEL_ESP(initTaskUnion); // Point the TSS to the new_task system stack

	set_cr3(initTaskStruct->dir_pages_baseAddr); //set cr3 to directory base address
}


void init_sched()
{
	INIT_LIST_HEAD(&readyqueue); //ready queue initialized as an empty queue
	initialize_freequeue();
	initialize_readyqueue();
}
 
void initialize_freequeue() //must initialize freequeue to  empty
{ 
	INIT_LIST_HEAD(&freequeue);
	for(int i = 0; i < NR_TASKS; i++){
		list_add_tail(&(task[i].task.list), &freequeue); //task[i].task.list is the HEAD and freequeue is the TAIL of the queue
		task[i].task.PID = -1;
	}
}

void initialize_readyqueue(void) {
	INIT_LIST_HEAD(&readyqueue);
}

struct task_struct* current()
{
  int ret_value;
  
  __asm__ __volatile__(
  	"movl %%esp, %0"
	: "=g" (ret_value)
  );
  return (struct task_struct*)(ret_value&0xfffff000);
}

void inner_task_switch(union task_union *new){
	
	tss.esp0 = KERNEL_ESP(new);
	writeMsr(0x175, (int) KERNEL_ESP(new));

	struct task_struct * current_task_struct = current();
	page_table_entry * dir_new = get_DIR((struct task_struct *) new);
	page_table_entry * dir_current = get_DIR(current_task_struct);

	if(dir_new != dir_current) set_cr3(dir_new);
	
	current()->esp_register = (DWord *) getEbp(); 
	setEsp(new->task.esp_register);
}
