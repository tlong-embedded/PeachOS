/**
 * @file kernel.c
 * @author Tomer Lousky (<tomer.lousky@gmail.com>)
 * @brief Core kernel functions and global variables for PeachOS
 * @version 0.1
 * @date 2024-06-15
 * @details This file contains the core kernel functions and global variables for the PeachOS operating system.
 * It includes terminal handling, panic handling, kernel initialization, and paging management.
 * The kernel is responsible for initializing hardware, managing memory, handling interrupts,
 * and providing a basic environment for user processes to run.
 */

/** Includes */
#include "kernel.h"
#include <stddef.h>
#include <stdint.h>
#include "idt/idt.h"
#include "memory/heap/kheap.h"
#include "memory/paging/paging.h"
#include "memory/memory.h"
#include "keyboard/keyboard.h"
#include "string/string.h"
#include "isr80h/isr80h.h"
#include "task/task.h"
#include "task/process.h"
#include "fs/file.h"
#include "disk/disk.h"
#include "fs/pparser.h"
#include "disk/streamer.h"
#include "task/tss.h"
#include "gdt/gdt.h"
#include "config.h"
#include "status.h"

/*
 * Global variables and definitions for PeachOS kernel:
 *
 * - video_mem: Pointer to the start of video memory for text output.
 * - terminal_row: Current row position of the terminal cursor.
 * - terminal_col: Current column position of the terminal cursor.
 * - TERMINAL_COLOUR: Attribute byte for terminal text color (0x0F = white on black).
 * - kernel_chunk: Pointer to the kernel's 4GB paging chunk structure.
 * - tss: Task State Segment structure for CPU task management.
 * - gdt_real: Array holding the actual Global Descriptor Table (GDT) segments.
 */
uint16_t* video_mem = 0;

uint16_t terminal_row = 0;

uint16_t terminal_col = 0;

#define TERMINAL_COLOUR 0x0F  

static struct paging_4gb_chunk* kernel_chunk = 0;

struct tss tss;

struct gdt gdt_real[PEACHOS_TOTAL_GDT_SEGMENTS];

struct gdt_structured gdt_structured[PEACHOS_TOTAL_GDT_SEGMENTS] = {
    {.base = 0x00, .limit = 0x00, .type = 0x00},                // NULL Segment
    {.base = 0x00, .limit = 0xffffffff, .type = 0x9a},           // Kernel code segment
    {.base = 0x00, .limit = 0xffffffff, .type = 0x92},            // Kernel data segment
    {.base = 0x00, .limit = 0xffffffff, .type = 0xf8},              // User code segment
    {.base = 0x00, .limit = 0xffffffff, .type = 0xf2},             // User data segment
    {.base = (uintptr_t)&tss, .limit=sizeof(tss), .type = 0xE9}      // TSS Segment
};

/** 
 * @brief terminal_make_char - Combines a character and its colour into a single 16-bit value
 * @param[in] c The character to be displayed
 * @param[in] colour The colour attribute for the character
 * @return A 16-bit value combining the character and its colour
 * @details This function takes a character and its associated colour attribute,
 * shifts the colour to the higher byte, and combines it with the character in the lower byte.
 * This format is used for representing characters in VGA text mode.
 */
uint16_t terminal_make_char(char c, char colour)
{
    return (colour << 8) | c;
}

/** 
 * @brief terminal_putchar - Puts a character at a specific position in the terminal
 * @param[in] x The x coordinate (column) where the character will be placed
 * @param[in] y The y coordinate (row) where the character will be placed
 * @param[in] c The character to place at the specified position
 * @param[in] colour The colour of the character
 * @return void
 * @details This function places a character at the specified (x, y) position in the terminal's video memory.
 * It calculates the correct index in the video memory array based on the provided coordinates and sets the character
 * along with its colour attribute.
 */
void terminal_putchar(int x, int y, char c, char colour)
{
    video_mem[(y * VGA_WIDTH) + x] = terminal_make_char(c, colour);
}

/** 
 * @brief terminal_backspace - Handles backspace functionality in the terminal
 * @return void
 * @details This function moves the cursor back by one position and replaces the character at that position with a space.
 * It ensures that the cursor does not move beyond the start of the terminal.
 * If the cursor is at the beginning of a line, it moves to the end of the previous line.
 */
void terminal_backspace()
{
    if (terminal_row == 0 && terminal_col == 0)
    {
        return;
    }

    if (terminal_col == 0)
    {
        terminal_row -= 1;
        terminal_col = VGA_WIDTH;
    }

    terminal_col -=1;
    terminal_writechar(' ', 15);
    terminal_col -=1;
}

/** 
 * @brief terminal_writechar - Writes a character to the terminal
 * @param[in] c The character to write to the terminal
 * @param[in] colour The colour of the character
 * @return void
 * @details This function writes a single character to the terminal at the current cursor position.
 * It handles special characters like newline and backspace, and updates the cursor position accordingly.
 * If the cursor reaches the end of the line, it wraps to the next line.
 * If the cursor reaches the bottom of the screen, it wraps to the top.
 */
void terminal_writechar(char c, char colour)
{
    if (c == '\n')
    {
        terminal_row += 1;
        terminal_col = 0;
        return;
    }

    if (c == 0x08)
    {
        terminal_backspace();
        return;
    }

    terminal_putchar(terminal_col, terminal_row, c, colour);
    terminal_col += 1;
    if (terminal_col >= VGA_WIDTH)
    {
        terminal_col = 0;
        terminal_row += 1;
    }
    if(terminal_row >= VGA_HEIGHT)
    {
        terminal_row = 0;
    }
}

/** 
 * @brief terminal_initialize - Initializes the terminal
 * @return void
 * @details This function initializes the terminal by setting the video memory pointer,
 * resetting the cursor position, and clearing the screen.
 */
void terminal_initialize()
{
    video_mem = (uint16_t*)(0xB8000);
    terminal_row = 0;
    terminal_col = 0;
    for (int y = 0; y < VGA_HEIGHT; y++)
    {
        for (int x = 0; x < VGA_WIDTH; x++)
        {
            terminal_putchar(x, y, ' ', TERMINAL_COLOUR);
        }
    }   
}

/** 
 * @brief _putchar - Writes a character to the terminal
 * @param[in] character The character to write to the terminal
 * @return void
 * @details This function writes a single character to the terminal
 * using the terminal_writechar function with a predefined colour.
 * It is typically used by higher-level functions to output characters like printf.
 * This function does not handle special characters like newline or backspace.
 * This function does not handle UTF-8 characters.
 * This function does not handle cursor positioning.
 * This function does not handle colours.
 */
void _putchar(char character)
{
    terminal_writechar(character, TERMINAL_COLOUR);
}

/** 
 * @brief print - Prints a string to the terminal
 * @param[in] str The string to print to the terminal
 * @return void
 * @details This function prints a null-terminated string to the terminal
 * by iterating through each character in the string and using the terminal_writechar function.
 * It does not handle special characters like newline or backspace.
 * It does not handle UTF-8 characters.
 * It does not handle cursor positioning.
 * It does not handle colours.
 */
void print(const char* str)
{
    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++)
    {
        terminal_writechar(str[i], TERMINAL_COLOUR);
    }
}

/** 
 * @brief panic - Handles kernel panic situations
 * @param[in] msg The panic message to display
 * @return void
 * @details This function displays a panic message on the terminal and halts the system.
 * It enters an infinite loop to prevent further execution of the kernel.
 * This function is typically called when a critical error occurs that cannot be recovered from.
 */
void panic(const char* msg)
{
    print(msg);
    while(1) {}
}

/** 
 * @brief kernel_page - Switches to the kernel paging chunk
 * @return void
 * @details This function switches the current paging context to the kernel's 4GB paging chunk.
 * It also registers the kernel's CPU registers for proper context management.
 * This function is typically called during the kernel initialization process to ensure
 * that the kernel operates within its own memory space.
 */
void kernel_page()
{
    kernel_registers();
    paging_switch(kernel_chunk);
}

/** 
 * @brief kernel_main - The main entry point for the kernel
 * @return void
 * @details This function is the main entry point for the kernel.
 * It initializes various subsystems including the terminal, GDT, heap, filesystems,
 * disks, IDT, TSS, and paging. It also registers kernel commands and initializes keyboards.
 * Finally, it loads and switches to the initial user process and starts task scheduling.
 * This function does not return and enters an infinite loop at the end to keep the kernel running.
 */
void kernel_main()
{
    terminal_initialize();
    memset(gdt_real, 0x00, sizeof(gdt_real));
    gdt_structured_to_gdt(gdt_real, gdt_structured, PEACHOS_TOTAL_GDT_SEGMENTS);

    // Load the gdt
    gdt_load(gdt_real, sizeof(gdt_real)-1);

    // Initialize the heap
    kheap_init();
    print("Welcome to PeachOS!\n");

    // Initialize filesystems
    fs_init();

    // Search and initialize the disks
    disk_search_and_init();

    // Initialize the interrupt descriptor table
    idt_init();

    // Setup the TSS
    memset(&tss, 0x00, sizeof(tss));
    tss.esp0 = 0x600000;
    tss.ss0 = KERNEL_DATA_SELECTOR;

    // Load the TSS
    tss_load(0x28);

    // Setup paging
    kernel_chunk = paging_new_4gb(PAGING_IS_WRITEABLE | PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL);
    
    // Switch to kernel paging chunk
    paging_switch(kernel_chunk);

    // Enable paging
    enable_paging();

    // Register the kernel commands
    isr80h_register_commands();

    // Initialize all the system keyboards
    keyboard_init();

    struct process* process = NULL;

    int res = process_load_switch("0:/blank.elf", &process);
    if (res != PEACHOS_ALL_OK)
    {
        panic("Failed to load blank.elf\n");
    }


    struct command_argument argument;
    memset(&argument, 0x00, sizeof(argument));

    strcpy(argument.argument, "Testing!");
    argument.next = NULL;

    process_inject_arguments(process, &argument);

    res = process_load_switch("0:/blank.elf", &process);
    if (res != PEACHOS_ALL_OK)
    {
        panic("Failed to load blank.elf\n");
    }

    strcpy(argument.argument, "Abc!");
    argument.next = NULL;
    process_inject_arguments(process, &argument);

    task_run_first_ever_task();

    while(1) {}
}