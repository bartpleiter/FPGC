/*
 * Read only test of BRFS
 * Assumes the filesystem is already initialized using the other test_brfs.c file
 */

#define COMMON_STDLIB
#define COMMON_STRING
#include "libs/common/common.h"

#define KERNEL_BRFS
#define KERNEL_MEM_DEBUG
#include "libs/kernel/kernel.h"

void dump_fs_hex_uart()
{
    uart_puts("\nFilesystem Superblock Dump:\n");
    debug_mem_dump(brfs_get_superblock(), 16);
    uart_puts("\nFilesystem FAT Dump (first 64 words):\n");
    debug_mem_dump(brfs_get_fat(), 64);
    uart_puts("\nFilesystem Data Block 0 Dump (first 64 words):\n");
    debug_mem_dump(brfs_get_data_block(0), 64);
}

/* Print filesystem statistics */
void print_fs_stats()
{
    unsigned int total;
    unsigned int free_blk;
    unsigned int blk_size;
    
    if (brfs_statfs(&total, &free_blk, &blk_size) == BRFS_OK)
    {
        uart_puts("  Blocks: ");
        uart_putint(free_blk);
        uart_puts("/");
        uart_putint(total);
        uart_puts(" free, ");
        uart_putint(blk_size);
        uart_puts(" words/block\n");
    }
}

/* Print directory listing */
void list_directory(const char* path)
{
    struct brfs_dir_entry entries[32];
    int count;
    int i;
    char filename[BRFS_MAX_FILENAME_LENGTH + 1];
    
    uart_puts("Directory ");
    uart_puts(path);
    uart_puts(":\n");
    
    count = brfs_read_dir(path, entries, 32);
    
    if (count < 0)
    {
        uart_puts("  Error: ");
        uart_puts(brfs_strerror(count));
        uart_putchar('\n');
        return;
    }
    
    for (i = 0; i < count; i++)
    {
        brfs_decompress_string(filename, entries[i].filename, 4);
        uart_puts("  ");
        if (entries[i].flags & BRFS_FLAG_DIRECTORY)
        {
            uart_puts("[DIR]  ");
        }
        else
        {
            uart_puts("[FILE] ");
        }
        uart_puts(filename);
        uart_puts(" (");
        uart_putint(entries[i].filesize);
        uart_puts(" words)\n");
    }
}

int main()
{
    int result = brfs_init(SPI_FLASH_1);
    result = brfs_mount();
    
    if (result == BRFS_OK)
    {
        print_fs_stats();

        list_directory("/");
        list_directory("/testdir");

        int fd = brfs_open("/testdir/data.bin");
        if (fd >= 0)
        {
            // Read the entire file and print the output to UART
            unsigned int buffer[128];
            int filesize = brfs_file_size(fd);
            if (filesize > 128) filesize = 128;
            int read_words = brfs_read(fd, buffer, (unsigned int)filesize);
            buffer[127] = 0; // Null-terminate for printing as string
            if (read_words >= 0)
            {
                uart_puts("   File Data:\n");
                uart_puts(buffer);
            }
        }
    }

    
    
    return 1;
}

void interrupt()
{
    /* Empty interrupt handler */
}

