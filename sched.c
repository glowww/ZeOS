/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>

#include <sched.h>
#include <mm.h>
#include <io.h>
#include <entry.h>
#include <types.h>
#include <stats.h>

union task_union task[NR_TASKS]
  __attribute__((__section__(".data.task")));


struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, list);
}

int MAX_PID = 1;
int quantum = 0;
extern TSS tss; 
extern struct list_head blocked;
extern int dir_pages_refs[NR_TASKS];
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
	int i;
	for(i = 0; i < NR_TASKS; i++){
		if (!dir_pages_refs[i]){
			t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[i];
			dir_pages_refs[i]++;	
			return 1;
		}
	}
	return -1;
}

void cpu_idle(void)
{
	__asm__ __volatile__("sti": : :"memory");

	while(1)
	{
	;
	}
}

void init_stats(struct stats *st)
{
	st->user_ticks = 0;
	st->system_ticks = 0;
	st->blocked_ticks = 0;
	st->ready_ticks = 0;
	st->elapsed_total_ticks = get_ticks();
	st->total_trans = 0;
	st->remaining_ticks = 0;
}

void init_idle (void)
{
	struct list_head * idleTaskUnionLH = list_first(&freequeue);
	list_del (idleTaskUnionLH);
	struct task_struct * idleTaskStruct = list_head_to_task_struct(idleTaskUnionLH); //get task_struct
  	union task_union * idleTaskUnion = (union task_union*)idleTaskStruct;			//get task_union
	idleTaskStruct -> PID = 0; 
	allocate_DIR(idleTaskStruct);

	idleTaskStruct->quantum = QUANTUM_DEFAULT;
	idleTaskStruct->state = ST_READY;
	init_stats(&idleTaskStruct->statistics);

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

	initTaskStruct->quantum = QUANTUM_DEFAULT;
	initTaskStruct->state = ST_RUN;
	init_stats(&initTaskStruct->statistics);
	initTaskStruct->statistics.remaining_ticks = initTaskStruct->quantum;

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

void update_sched_data_rr()
{
	quantum--;
}

int needs_sched_rr()
{	
	int no_quantum = (quantum <= 0);
	int is_valid_task = (current()->PID > 0);
	int processes_available = !list_empty(&readyqueue);
	return (no_quantum || is_valid_task) && processes_available;
}

void update_process_state_rr(struct task_struct *t, struct list_head *dest)
{
	if(t->state != ST_RUN) list_del(&t->list);

	if(dest == NULL) t->state = ST_RUN;
	else {
		list_add_tail(&t->list, dest);	
		if(dest == &readyqueue) {
			t->state = ST_READY;
		}
		else{
			t->state = ST_BLOCKED;
		}
	} 
}

void sched_next_rr()
{
	if (list_empty(&readyqueue)){
		task_switch((union task_union *) idle_task);
	}
	else {
		struct list_head * next_lh = list_first(&readyqueue);
		list_del (next_lh);
		struct task_struct * next_task = list_head_to_task_struct(next_lh);
		next_task->state=ST_RUN;
		next_task->statistics.total_trans += 1;
		quantum = get_quantum(next_task);

		// update_process_state_rr(next_task, NULL);
		task_switch((union task_union*) next_task);
	}
}

int get_quantum(struct task_struct *t){
	return t->quantum;
}

void set_quantum(struct task_struct *t, int new_quantum){
	t->quantum = new_quantum;
}

void schedule(){
	update_sched_data_rr();
	if(needs_sched_rr()){
		update_process_state_rr(current(), &readyqueue);
		sched_next_rr();
	}
}

void update_stats_user_to_system(struct stats *statistics)
{
	update_stats(&statistics->user_ticks, &statistics->elapsed_total_ticks);
}

void update_stats_system_to_user(struct stats *statistics)
{
	update_stats(&statistics->system_ticks, &statistics->elapsed_total_ticks);
}

void update_stats_run_to_ready(struct stats *statistics)
{
	update_stats(&statistics->system_ticks, &statistics->elapsed_total_ticks);
}

void update_stats_ready_to_run(struct stats *statistics)
{
	update_stats(&statistics->ready_ticks, &statistics->elapsed_total_ticks);
}

void update_stats(unsigned long *v, unsigned long *elapsed)
{
  *v += get_ticks() - *elapsed;
  *elapsed = get_ticks();
}