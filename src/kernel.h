/**
 * @file kernel.h
 * @brief Core kernel interface definitions for PeachOS.
 *
 * Defines constants, macros, and function prototypes for kernel operations,
 * terminal output, error handling, and system initialization.
 *
 * Constants:
 *   - VGA_WIDTH: Width of the VGA text buffer.
 *   - VGA_HEIGHT: Height of the VGA text buffer.
 *   - PEACHOS_MAX_PATH: Maximum path length supported.
 *
 * Functions:
 *   - kernel_main(): Entry point for the kernel.
 *   - print(const char* str): Print a string to the terminal.
 *   - terminal_writechar(char c, char colour): Write a character with color.
 *   - panic(const char* msg): Halt the system with an error message.
 *   - kernel_page(): Set up kernel paging.
 *   - kernel_registers(): Initialize or display CPU registers.
 *
 * Error Handling Macros:
 *   - ERROR(value): Casts a value to a void pointer for error signaling.
 *   - ERROR_I(value): Casts a value to int for error signaling.
 *   - ISERR(value): Checks if a value represents an error (negative integer).
 */
#ifndef KERNEL_H
#define KERNEL_H

#define VGA_WIDTH 80
#define VGA_HEIGHT 20

#define PEACHOS_MAX_PATH 108

void kernel_main();
void print(const char* str);
void terminal_writechar(char c, char colour);

void panic(const char* msg);
void kernel_page();
void kernel_registers();

#define ERROR(value) (void*)(value)
#define ERROR_I(value) (int)(value)
#define ISERR(value) ((int)value < 0)

#endif