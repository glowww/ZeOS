#include <asm.h>

#define SYSCALL(syscalln) \
	leal syscall_ret##syscalln, %eax; \
	push %eax; \
	movl $syscalln, %eax; \
	push %ebp; \
	movl %esp, %ebp; \
	sysenter; \
	syscall_ret##syscalln: pop %ebp; \
	addl $4, %esp

#define CHECK_ERROR(end) \
	cmpl $0, %eax; \
	jge  end; \
	negl %eax; \
	leal errno, %ebx; \
	movl %eax, (%ebx); \
	movl $-1, %eax


ENTRY(write)
	pushl %ebp
	movl %esp, %ebp	
	pushl %ebx
    movl 8(%ebp), %ebx
	movl 12(%ebp), %ecx
	movl 16(%ebp), %edx 
	movl $0x04, %eax
	pushl %ecx
	pushl %edx
	pushl $retorn
	pushl %ebp
	movl %esp, %ebp	
	sysenter
retorn:   
	popl %ebp
	addl $4, %esp
	popl %edx
	popl %ecx
	popl %ebx
	test %eax, %eax
	js errore // %eax < 0
	//movl %ebp, %esp
	popl %ebp
	ret
errore:
    neg %eax
	mov %eax, errno
	mov $-1, %eax
	popl %ebp
	ret

ENTRY(gettime)
	push %ebp
	movl %esp, %ebp
	SYSCALL(10);      
	pop %ebp
	ret

ENTRY(getpid)
	push %ebp
	movl %esp, %ebp
	SYSCALL(20)
	pop  %ebp
	ret

ENTRY(fork)
	push %ebp
	movl %esp, %ebp
	push %ebx
	SYSCALL(2)
	CHECK_ERROR(fork.no_error)
fork.no_error:
	pop %ebx
	pop %ebp
	ret

ENTRY(get_stats)
	push %ebp
	movl %esp, %ebp
	push %ebx
	movl 8(%ebp), %ebx
	movl 12(%ebp), %ecx
	SYSCALL(35)
	CHECK_ERROR(get_stats.no_error)
get_stats.no_error:
	pop %ebx
	pop %ebp
	ret

ENTRY(exit)
	push %ebp
	movl %esp, %ebp
	SYSCALL(1)
	pop %ebp
	ret