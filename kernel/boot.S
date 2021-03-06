#include "kernel.h"
#include "sizes.h"
#include "multiboot2.h"

.SET HEADER_LENGTH, header_end - header_start
.SET CHECKSUM, -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + HEADER_LENGTH)
.section .multiboot
.code32
.align 8
header_start:
    .long MULTIBOOT2_HEADER_MAGIC
    .long MULTIBOOT_ARCHITECTURE_I386
    .long HEADER_LENGTH
    .long CHECKSUM

    // multiboot tags go here

    .short MULTIBOOT_HEADER_TAG_END
    .short 0    // flags, none set
    .long 8     // size, including itself (short + short + long)
header_end:

  // Global Descriptor Table
  .section .rodata
  .align 4
gdt64:
  .quad 0 // zero entry

gdt64_code_entry:
  .set gdt64_code_seg, gdt64_code_entry - gdt64
  .quad (1<<44) | (1<<47) | (1<<41) | (1<<43) | (1<<53) // code segment

gdt64_data_entry:
  .set gdt64_data_seg, gdt64_data_entry - gdt64
  .quad (1<<44) | (1<<47) | (1<<41) // data segment

gdt64_pointer:
  .set gdt64_limit, gdt64_pointer - gdt64 - 1
  .word gdt64_limit
  .quad gdt64

.data
multiboot_magic:
	.long 0
multiboot_info:
	.long 0

  // Entry code
  .global _start
  .extern cmain

  .section .text
  .code32
_start:
    movl $stack_top, %esp

    //movl %ebx, %edi

    mov %eax, multiboot_magic
    mov %ebx,  multiboot_info

    call check_multiboot
    call check_cpuid
    call check_long_mode

    call set_up_page_tables
    call enable_paging

    call set_up_SSE

    // load the 64-bit GDT
    lgdt (gdt64_pointer)

    // update selectors
    movw $gdt64_data_seg, %ax
    movw %ax, %ss
    movw %ax, %ds
    movw %ax, %es

    ljmp  $gdt64_code_seg, $_start64//$cmain

set_up_page_tables:

    movl $p3_table, %eax
    orl $0b11, %eax       // present + writable
    mov %eax, (p4_table)
    movl $0b10000011, (p3_table)


    movl $1, %ebx
    movl $0b10000011, %eax
    movl %eax, p3_table(,%ebx,8)


    movl $1, %ebx
    movl p3_table, %eax
    movl %eax, p4_table(,%ebx,8)

    ret

enable_paging:
    // load P4 to cr3 register (cpu uses this to access the P4 table)
    movl $p4_table, %eax
    movl %eax, %cr3

    // enable PAE-flag in cr4 (Physical Address Extension)
    movl %cr4, %eax
    orl $1 << 5, %eax
    mov %eax, %cr4

    // set the long mode bit in the EFER MSR (model specific register)
    mov $0xC0000080, %ecx
    rdmsr
    orl $1 << 8, %eax
    wrmsr

    // enable paging in the cr0 register
    movl %cr0, %eax
    orl $1 << 31, %eax
    mov %eax, %cr0

    ret

// Prints `ERR: ` and the given error code to screen and hangs.
// parameter: error code (in ascii) in al
error:
    movl $0x4f524f45, (0xb8000)
    movl $0x4f3a4f52, (0xb8004)
    movl $0x4f204f20, (0xb8008)
    movb %al, (0xb800a)
    hlt

// Throw error 0 if eax doesn't contain the Multiboot 2 magic value (0x36d76289).
check_multiboot:
    cmpl $0x36d76289, %eax
    jne no_multiboot
    ret
no_multiboot:
    movb $'0', %al
    jmp error

// Throw error 1 if the CPU doesn't support the CPUID command.
check_cpuid:
    pushf                // Store the FLAGS-register.
    pop %eax             // Restore the A-register.
    mov %eax, %ecx       // Set the C-register to the A-register.
    xor $1 << 21, %eax   // Flip the ID-bit, which is bit 21.
    push %eax            // Store the A-register.
    popf                 // Restore the FLAGS-register.
    pushf                // Store the FLAGS-register.
    pop %eax             // Restore the A-register.
    push %ecx            // Store the C-register.
    popf                 // Restore the FLAGS-register.
    xor %ecx, %eax       // Do a XOR-operation on the A-register and the C-register.
    jz no_cpuid          // The zero flag is set, no CPUID.
    ret                  // CPUID is available for use.
no_cpuid:
    mov $'1', %al
    jmp error

// Throw error 2 if the CPU doesn't support Long Mode.
check_long_mode:
    movl $0x80000000, %eax  // Set the A-register to 0x80000000.
    cpuid                   // CPU identification.
    cmp $0x80000001, %eax   // Compare the A-register with 0x80000001.
    jb no_long_mode         // It is less, there is no long mode.
    movl $0x80000001, %eax  // Set the A-register to 0x80000001.
    cpuid                   // CPU identification.
    test $1 << 29, %edx     // Test if the LM-bit, which is bit 29, is set in the D-register.
    jz no_long_mode         // They aren't, there is no long mode.
    ret
no_long_mode:
    mov $'2', %al
    jmp error

// Check for SSE and enable it. If it's not supported throw error "a".
set_up_SSE:
    // check for SSE
    movl $0x1, %eax
    cpuid
    testl $1<<25, %edx
    jz no_SSE

    // enable SSE
    movl %cr0, %eax
    andw  $0xFFFB, %ax      // clear coprocessor emulation CR0.EM
    orw $0x2, %ax          // set coprocessor monitoring  CR0.MP
    movl %eax, %cr0
    movl %cr4, %eax
    orw $3 << 9, %ax       // set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
    movl %eax, %cr4

    ret
no_SSE:
    movb $'a', %al
    jmp error


.code64

.global _start64
.type _start64, @function
_start64:
        // Setup segment selectors
        movw $0, %ax
        movw %ax, %ds
        movw %ax, %es
        movw %ax, %fs
        movw %ax, %gs
        movw %ax, %ss

	mov    multiboot_magic, %rsi
	mov    multiboot_info, %rdi

	call cmain

        // Should never reach here
        cli
        hlt
1:
        jmp 1b


  // Uninitialized data section contain the page tables and stack
  .section .bss
  .align 4096
p4_table:
    .skip 4096
p3_table:
    .skip 4096
p2_table:
    .skip 4096
stack_bottom:
    .skip 4096 * 4
stack_top:

