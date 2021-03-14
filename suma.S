#include <asm.h>

ENTRY(addAsm)
	push %ebp
	mov %esp, %ebp
	mov    0xc(%ebp),%eax
	add    0x8(%ebp),%eax
	pop    %ebp
		
