/*
 * sched.h - Estructures i macros pel tractament de processos
 */

#ifndef __SCHED_H__
#define __SCHED_H__

#include <list.h>
#include <types.h>
#include <mm_address.h>
#include <stats.h>

#define NR_TASKS      10
#define KERNEL_STACK_SIZE	1024
#define QUANTUM_DEFAULT 10

enum state_t { ST_RUN, ST_READY, ST_BLOCKED };

extern struct list_head freequeue; //freequeue -- do not mangle its name

struct task_struct {
  int PID;			/* Process ID. This MUST be the first field of the struct. */
  page_table_entry * dir_pages_baseAddr; //to load cr3?
  struct list_head list; //used to enqueue the structure into a queue
  DWord esp_register;
  enum state_t state;
  struct stats stats;
  int quantum;
};

union task_union {
  struct task_struct task;
  unsigned long stack[KERNEL_STACK_SIZE];    /* pila de sistema, per procés */
};

extern union task_union task[NR_TASKS]; /* Vector de tasques */


#define KERNEL_ESP(t)       	(DWord) &(t)->stack[KERNEL_STACK_SIZE]

#define INITIAL_ESP       	KERNEL_ESP(&task[1])

/* Inicialitza les dades del proces inicial */
void init_task1(void);

void init_idle(void);

void init_sched(void);

void initialize_freequeue(void);

void initialize_readyqueue(void);

struct task_struct * current();

void task_switch(union task_union*t);

void inner_task_switch(union task_union*t); //where magic happens

struct task_struct *list_head_to_task_struct(struct list_head *l);

int allocate_DIR(struct task_struct *t);

page_table_entry * get_PT (struct task_struct *t) ;

page_table_entry * get_DIR (struct task_struct *t) ;

/* Headers for the scheduling policy */
void sched_next_rr();
void update_process_state_rr(struct task_struct *t, struct list_head *dest);
int needs_sched_rr();
void update_sched_data_rr();

void update_stats_user_to_system(struct stats *stats);
void update_stats_system_to_user(struct stats *stats);
void update_stats_ready_to_run(struct stats *stats);
void update_stats_run_to_ready(struct stats *stats);

void schedule(void);

#endif  /* __SCHED_H__ */
