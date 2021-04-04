/*
 * entry.S - Entry point to system mode from user mode
 */

#include <asm.h>
#include <segment.h>


/**************************************************/
/**** Save & Restore ******************************/
/**                                              **/
/** When we change to privilege level 0 (kernel) **/
/** (through an interrupt, a system call, an     **/
/** exception ...) we must save the state of the **/
/** currently running task (save).               **/
/**                                              **/
/** Stack layout in 'systemCall':                **/
/**                                              **/
/**   0(%esp) - %ebx    \                        **/
/**   4(%esp) - %ecx     |                       **/
/**   8(%esp) - %edx     |                       **/
/**   C(%esp) - %esi     | Register saved        **/
/**  10(%esp) - %edi     |  by 'save'            **/
/**  14(%esp) - %ebp     |                       **/
/**  18(%esp) - %eax     |                       **/
/**  1C(%esp) - %ds      |                       **/
/**  20(%esp) - %es      |                       **/
/**  24(%esp) - %fs      |                       **/
/**  28(%esp) - %gs     /                        **/
/**  2C(%esp) - %eip    \                        **/
/**  30(%esp) - %cs      |                       **/
/**  34(%esp) - %eflags  |  Return context saved **/
/**  38(%esp) - %oldesp  |   by the processor.   **/
/**  3C(%esp) - %oldss  /                        **/
/**                                              **/
/**************************************************/

#define SAVE_ALL \
      pushl %gs; \
      pushl %fs; \
      pushl %es; \
      pushl %ds; \
      pushl %eax; \
      pushl %ebp; \
      pushl %edi; \
      pushl %esi; \
      pushl %edx; \
      pushl %ecx; \
      pushl %ebx; \
      movl $__KERNEL_DS, %edx;    \
      movl %edx, %ds;           \
      movl %edx, %es

#define RESTORE_ALL \
	popl %ebx;\
	popl %ecx;\
	popl %edx;\
	popl %esi;\
	popl %edi;\
	popl %ebp;\
	popl %eax;\
	popl %ds; \
	popl %es; \
	popl %fs; \
	popl %gs

#define EOI \
	movb $0x20, %al; \
	outb %al, $0x20;

#define SYSCALL(syscalln) \
	leal syscall_ret##syscalln, %eax; \
	push %eax; \
	movl $syscalln, %eax; \
	push %ebp; \
	movl %esp, %ebp; \
	sysenter; \
	syscall_ret##syscalln: pop %ebp; \
	addl $4, %esp

ENTRY(clock_handler)
	SAVE_ALL;
	EOI;
	call clock_routine;
	RESTORE_ALL;
	iret

ENTRY(gettime)
	push %ebp
	movl %esp, %ebp
	SYSCALL(10);      
	pop %ebp
	ret

ENTRY(keyboard_handler)
	SAVE_ALL
	EOI
	call keyboard_routine
	RESTORE_ALL
	iret;

ENTRY(system_call_handler)
	SAVE_ALL
	cmpl $0, %eax
	jl err
	cmpl $MAX_SYSCALL, %eax
	jg err	// Save the current context
	call *sys_call_table( , %eax, 0x04)
	jmp end
err:
	movl $-38, %eax
end: 
	movl %eax, 0x18(%esp)
	RESTORE_ALL
	iret

ENTRY(writeMSR)
	push %ebp
	movl %esp, %ebp
	movl 8(%ebp), %eax //content of MSR register
	movl 12(%ebp), %ecx //register number
	xor %edx, %edx
	wrmsr
	movl %ebp, %esp
	popl %ebp
	ret

ENTRY(task_switch)
	pushl %ebp;
	movl  %esp, %ebp;
	pushl %esi;
	pushl %edi;
	pushl %ebx;
	pushl 8(%ebp);
	call inner_task_switch;
	addl  $4, %esp;
	popl  %ebx;
	popl  %edi;
	popl  %esi;
	movl  %ebp, %esp;
	popl  %ebp;
	ret;

ENTRY(setEsp)
	movl 4(%esp), %esp
	popl %ebp;
	ret;

ENTRY(getEbp)
	movl %ebp, %eax
	ret;
