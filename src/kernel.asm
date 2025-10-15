;------------------------------------------------------------------------------
; kernel.asm - Entry point and setup for PeachOS kernel (32-bit)
;
; This file contains the initial bootstrapping code for the PeachOS kernel.
; It performs the following tasks:
;   - Sets up segment registers (DS, ES, FS, GS, SS) to the data segment.
;   - Initializes the stack pointer (ESP) and base pointer (EBP) to a known
;     memory location (0x00200000).
;   - Remaps the master Programmable Interrupt Controller (PIC) to avoid
;     conflicts with CPU exceptions by configuring its interrupt vector offset.
;   - Calls the main kernel entry point (kernel_main).
;   - Provides a utility routine (kernel_registers) to reset segment registers.
;   - Pads the file to 512 bytes for boot sector alignment.
;
; Symbols:
;   _start             - Kernel entry point, called by the bootloader.
;   kernel_registers   - Routine to reset segment registers to the data segment.
;   kernel_main        - External symbol, main kernel function implemented in C.
;
; Constants:
;   CODE_SEG           - Code segment selector (0x08).
;   DATA_SEG           - Data segment selector (0x10).
;------------------------------------------------------------------------------
[BITS 32]

global _start
global kernel_registers
extern kernel_main

CODE_SEG equ 0x08
DATA_SEG equ 0x10

_start:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov ebp, 0x00200000
    mov esp, ebp


    ; Remap the master PIC
    mov al, 00010001b
    out 0x20, al ; Tell master PIC

    mov al, 0x20 ; Interrupt 0x20 is where master ISR should start
    out 0x21, al

    mov al, 0x04 ; ICW3
    out 0x21, al

    mov al, 00000001b
    out 0x21, al
    ; End remap of the master PIC

    call kernel_main

    jmp $

kernel_registers:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov gs, ax
    mov fs, ax
    ret


times 512-($ - $$) db 0
