#include <asm.h>

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