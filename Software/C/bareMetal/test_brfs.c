/*
 * BRFS Filesystem Test Program
 * 
 * This test program exercises the BRFS filesystem implementation:
 * 1. Format a new filesystem on SPI Flash
 * 2. Create directories and files
 * 3. Write data to files
 * 4. Read back and verify data
 * 5. Sync changes to flash
 * 6. Remount and verify persistence
 */

#define COMMON_STDLIB
#define COMMON_STRING
#include "libs/common/common.h"

#define KERNEL_BRFS
#define KERNEL_MEM_DEBUG
#include "libs/kernel/kernel.h"

/* Test data */
#define TEST_DATA_SIZE 64
unsigned int test_data[TEST_DATA_SIZE];
unsigned int read_buffer[TEST_DATA_SIZE];

void dump_fs_hex_uart()
{
    uart_puts("\nFilesystem Superblock Dump:\n");
    debug_mem_dump(brfs_get_superblock(), 16);
    uart_puts("\nFilesystem FAT Dump (first 64 words):\n");
    debug_mem_dump(brfs_get_fat(), 64);
    uart_puts("\nFilesystem Data Block 0 Dump (first 64 words):\n");
    debug_mem_dump(brfs_get_data_block(0), 64);
}

/* Print test result */
void print_result(const char* test_name, int result)
{
    uart_puts(test_name);
    uart_puts(": ");
    if (result >= 0)
    {
        uart_puts("PASS");
    }
    else
    {
        uart_puts("FAIL (");
        uart_puts(brfs_strerror(result));
        uart_puts(")");
    }
    uart_putchar('\n');
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
    int result;
    int fd;
    int i;
    int words_written;
    int words_read;
    int verify_ok;
    
    uart_puts("=== BRFS Filesystem Test ===\n\n");
    
    /* Initialize test data */
    for (i = 0; i < TEST_DATA_SIZE; i++)
    {
        test_data[i] = 0xDEADBEEF + i;
    }
    
    /* === Test 1: Initialize BRFS === */
    uart_puts("1. Initializing BRFS...\n");
    result = brfs_init(SPI_FLASH_1);
    print_result("   brfs_init", result);
    if (result != BRFS_OK)
    {
        uart_puts("Cannot continue without init!\n");
        return 1;
    }
    
    /* === Test 2: Format filesystem === */
    uart_puts("\n2. Formatting filesystem...\n");
    /* 256 blocks * 256 words = 64K words = 256KB data
     * Plus 256 word FAT and 16 word superblock */
    result = brfs_format(256, 256, "TESTFS", 1);
    print_result("   brfs_format", result);
    if (result != BRFS_OK)
    {
        uart_puts("Cannot continue without format!\n");
        return 1;
    }
    print_fs_stats();
    
    /* === Test 3: Create directory === */
    uart_puts("\n3. Creating directories...\n");
    result = brfs_create_dir("/testdir");
    print_result("   brfs_create_dir /testdir", result);
    
    result = brfs_create_dir("/testdir/subdir");
    print_result("   brfs_create_dir /testdir/subdir", result);

    /* Test duplicate */
    result = brfs_create_dir("/testdir");
    if (result == BRFS_ERR_EXISTS)
    {
        uart_puts("   Duplicate check: ");
        uart_puts("PASS\n");
    }
    else
    {
        uart_puts("   Duplicate check: ");
        uart_puts("FAIL\n");
    }
    
    /* === Test 4: Create files === */
    uart_puts("\n4. Creating files...\n");
    result = brfs_create_file("/test.txt");
    print_result("   brfs_create_file /test.txt", result);
    
    result = brfs_create_file("/testdir/data.bin");
    print_result("   brfs_create_file /testdir/data.bin", result);
    
    /* List root directory */
    uart_putchar('\n');
    list_directory("/");
    uart_putchar('\n');
    list_directory("/testdir");
    
    /* === Test 5: Open and write file === */
    uart_puts("\n5. Writing to file...\n");
    fd = brfs_open("/testdir/data.bin");
    print_result("   brfs_open", fd);
    
    if (fd >= 0)
    {
        words_written = brfs_write(fd, test_data, TEST_DATA_SIZE);
        uart_puts("   brfs_write: ");
        if (words_written == TEST_DATA_SIZE)
        {
            uart_puts("PASS (");
            uart_putint(words_written);
            uart_puts(" words)\n");
        }
        else
        {
            uart_puts("FAIL\n");
        }
        
        result = brfs_close(fd);
        print_result("   brfs_close", result);
    }
    
    /* === Test 6: Read and verify file === */
    uart_puts("\n6. Reading and verifying...\n");
    fd = brfs_open("/testdir/data.bin");
    print_result("   brfs_open", fd);
    
    if (fd >= 0)
    {
        /* Check file size */
        result = brfs_file_size(fd);
        uart_puts("   File size: ");
        uart_putint(result);
        uart_puts(" words\n");
        
        /* Read data */
        words_read = brfs_read(fd, read_buffer, TEST_DATA_SIZE);
        uart_puts("   brfs_read: ");
        if (words_read == TEST_DATA_SIZE)
        {
            uart_puts("PASS (");
            uart_putint(words_read);
            uart_puts(" words)\n");
        }
        else
        {
            uart_puts("FAIL\n");
        }
        
        /* Verify data */
        verify_ok = 1;
        for (i = 0; i < TEST_DATA_SIZE; i++)
        {
            if (read_buffer[i] != test_data[i])
            {
                verify_ok = 0;
                uart_puts("   Mismatch at ");
                uart_putint(i);
                uart_puts(": ");
                uart_puthex(read_buffer[i], 1);
                uart_puts(" != ");
                uart_puthex(test_data[i], 1);
                uart_putchar('\n');
                break;
            }
        }
        uart_puts("   Data verify: ");
        if (verify_ok)
        {
            uart_puts("PASS\n");
        }
        else
        {
            uart_puts("FAIL\n");
        }
        
        result = brfs_close(fd);
        print_result("   brfs_close", result);
    }

    dump_fs_hex_uart();
    
    /* === Test 7: Sync to flash === */
    uart_puts("\n7. Syncing to flash...\n");
    result = brfs_sync();
    print_result("   brfs_sync", result);
    
    /* === Test 8: Unmount and remount === */
    uart_puts("\n8. Testing persistence...\n");
    result = brfs_unmount();
    print_result("   brfs_unmount", result);
    
    result = brfs_mount();
    print_result("   brfs_mount", result);
    
    if (result == BRFS_OK)
    {
        /* Verify data persisted */
        fd = brfs_open("/testdir/data.bin");
        if (fd >= 0)
        {
            words_read = brfs_read(fd, read_buffer, TEST_DATA_SIZE);
            verify_ok = 1;
            for (i = 0; i < TEST_DATA_SIZE && words_read == TEST_DATA_SIZE; i++)
            {
                if (read_buffer[i] != test_data[i])
                {
                    verify_ok = 0;
                    break;
                }
            }
            uart_puts("   Persistence verify: ");
            if (verify_ok && words_read == TEST_DATA_SIZE)
            {
                uart_puts("PASS\n");
            }
            else
            {
                uart_puts("FAIL\n");
            }
            brfs_close(fd);
        }
        
        /* List directory after remount */
        uart_putchar('\n');
        list_directory("/");
    }
    
    /* === Test 9: Delete file === */
    uart_puts("\n9. Testing delete...\n");
    result = brfs_delete("/test.txt");
    print_result("   brfs_delete /test.txt", result);
    
    /* Try to delete non-empty directory */
    result = brfs_delete("/testdir");
    if (result == BRFS_ERR_NOT_EMPTY)
    {
        uart_puts("   Non-empty dir check: ");
        uart_puts("PASS\n");
    }
    else
    {
        uart_puts("   Non-empty dir check: ");
        uart_puts("FAIL\n");
    }
    
    /* Print final stats */
    uart_puts("\nFinal filesystem state:\n");
    print_fs_stats();
    list_directory("/");
    
    uart_puts("\n=== Test Complete ===\n");
    
    return 1;
}

void interrupt()
{
    /* Empty interrupt handler */
}

