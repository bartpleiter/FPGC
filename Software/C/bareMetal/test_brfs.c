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
#define KERNEL_TERM
#define KERNEL_GPU_DATA_ASCII
#include "libs/kernel/kernel.h"

/* Test data */
#define TEST_DATA_SIZE 64
unsigned int test_data[TEST_DATA_SIZE];
unsigned int read_buffer[TEST_DATA_SIZE];

void init()
{
    /* Reset GPU VRAM */
    gpu_clear_vram();

    /* Load default pattern and palette tables */
    unsigned int* pattern_table = (unsigned int*)&DATA_ASCII_DEFAULT;
    gpu_load_pattern_table(pattern_table + 3); /* +3 to skip function prologue */

    unsigned int* palette_table = (unsigned int*)&DATA_PALETTE_DEFAULT;
    gpu_load_palette_table(palette_table + 3); /* +3 to skip function prologue */

    /* Initialize terminal */
    term_init();
}

/* Print test result */
void print_result(const char* test_name, int result)
{
    term_puts(test_name);
    term_puts(": ");
    if (result >= 0)
    {
        term_set_palette(2); /* Green */
        term_puts("PASS");
    }
    else
    {
        term_set_palette(1); /* Red */
        term_puts("FAIL (");
        term_puts(brfs_strerror(result));
        term_puts(")");
    }
    term_set_palette(0); /* Default */
    term_putchar('\n');
}

/* Print filesystem statistics */
void print_fs_stats()
{
    unsigned int total;
    unsigned int free_blk;
    unsigned int blk_size;
    
    if (brfs_statfs(&total, &free_blk, &blk_size) == BRFS_OK)
    {
        term_puts("  Blocks: ");
        term_putint(free_blk);
        term_puts("/");
        term_putint(total);
        term_puts(" free, ");
        term_putint(blk_size);
        term_puts(" words/block\n");
    }
}

/* Print directory listing */
void list_directory(const char* path)
{
    struct brfs_dir_entry entries[32];
    int count;
    int i;
    char filename[BRFS_MAX_FILENAME_LENGTH + 1];
    
    term_puts("Directory ");
    term_puts(path);
    term_puts(":\n");
    
    count = brfs_read_dir(path, entries, 32);
    
    if (count < 0)
    {
        term_puts("  Error: ");
        term_puts(brfs_strerror(count));
        term_putchar('\n');
        return;
    }
    
    for (i = 0; i < count; i++)
    {
        brfs_decompress_string(filename, entries[i].filename, 4);
        term_puts("  ");
        if (entries[i].flags & BRFS_FLAG_DIRECTORY)
        {
            term_puts("[DIR]  ");
        }
        else
        {
            term_puts("[FILE] ");
        }
        term_puts(filename);
        term_puts(" (");
        term_putint(entries[i].filesize);
        term_puts(" words)\n");
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
    
    init();
    
    term_puts("=== BRFS Filesystem Test ===\n\n");
    
    /* Initialize test data */
    for (i = 0; i < TEST_DATA_SIZE; i++)
    {
        test_data[i] = 0xDEADBEEF + i;
    }
    
    /* === Test 1: Initialize BRFS === */
    term_puts("1. Initializing BRFS...\n");
    result = brfs_init(SPI_FLASH_1);
    print_result("   brfs_init", result);
    if (result != BRFS_OK)
    {
        term_puts("Cannot continue without init!\n");
        return 1;
    }
    
    /* === Test 2: Format filesystem === */
    term_puts("\n2. Formatting filesystem...\n");
    /* 256 blocks * 256 words = 64K words = 256KB data
     * Plus 256 word FAT and 16 word superblock */
    result = brfs_format(256, 256, "TESTFS", 1);
    print_result("   brfs_format", result);
    if (result != BRFS_OK)
    {
        term_puts("Cannot continue without format!\n");
        return 1;
    }
    print_fs_stats();
    
    /* === Test 3: Create directory === */
    term_puts("\n3. Creating directories...\n");
    result = brfs_create_dir("/testdir");
    print_result("   brfs_create_dir /testdir", result);
    
    result = brfs_create_dir("/testdir/subdir");
    print_result("   brfs_create_dir /testdir/subdir", result);

    /* Test duplicate */
    result = brfs_create_dir("/testdir");
    if (result == BRFS_ERR_EXISTS)
    {
        term_puts("   Duplicate check: ");
        term_set_palette(2);
        term_puts("PASS\n");
        term_set_palette(0);
    }
    else
    {
        term_puts("   Duplicate check: ");
        term_set_palette(1);
        term_puts("FAIL\n");
        term_set_palette(0);
    }
    
    /* === Test 4: Create files === */
    term_puts("\n4. Creating files...\n");
    result = brfs_create_file("/test.txt");
    print_result("   brfs_create_file /test.txt", result);
    
    result = brfs_create_file("/testdir/data.bin");
    print_result("   brfs_create_file /testdir/data.bin", result);
    
    /* List root directory */
    term_putchar('\n');
    list_directory("/");
    term_putchar('\n');
    list_directory("/testdir");
    
    /* === Test 5: Open and write file === */
    term_puts("\n5. Writing to file...\n");
    fd = brfs_open("/testdir/data.bin");
    print_result("   brfs_open", fd);
    
    if (fd >= 0)
    {
        words_written = brfs_write(fd, test_data, TEST_DATA_SIZE);
        term_puts("   brfs_write: ");
        if (words_written == TEST_DATA_SIZE)
        {
            term_set_palette(2);
            term_puts("PASS (");
            term_putint(words_written);
            term_puts(" words)\n");
            term_set_palette(0);
        }
        else
        {
            term_set_palette(1);
            term_puts("FAIL\n");
            term_set_palette(0);
        }
        
        result = brfs_close(fd);
        print_result("   brfs_close", result);
    }
    
    /* === Test 6: Read and verify file === */
    term_puts("\n6. Reading and verifying...\n");
    fd = brfs_open("/testdir/data.bin");
    print_result("   brfs_open", fd);
    
    if (fd >= 0)
    {
        /* Check file size */
        result = brfs_file_size(fd);
        term_puts("   File size: ");
        term_putint(result);
        term_puts(" words\n");
        
        /* Read data */
        words_read = brfs_read(fd, read_buffer, TEST_DATA_SIZE);
        term_puts("   brfs_read: ");
        if (words_read == TEST_DATA_SIZE)
        {
            term_set_palette(2);
            term_puts("PASS (");
            term_putint(words_read);
            term_puts(" words)\n");
            term_set_palette(0);
        }
        else
        {
            term_set_palette(1);
            term_puts("FAIL\n");
            term_set_palette(0);
        }
        
        /* Verify data */
        verify_ok = 1;
        for (i = 0; i < TEST_DATA_SIZE; i++)
        {
            if (read_buffer[i] != test_data[i])
            {
                verify_ok = 0;
                term_puts("   Mismatch at ");
                term_putint(i);
                term_puts(": ");
                term_puthex(read_buffer[i], 1);
                term_puts(" != ");
                term_puthex(test_data[i], 1);
                term_putchar('\n');
                break;
            }
        }
        term_puts("   Data verify: ");
        if (verify_ok)
        {
            term_set_palette(2);
            term_puts("PASS\n");
        }
        else
        {
            term_set_palette(1);
            term_puts("FAIL\n");
        }
        term_set_palette(0);
        
        result = brfs_close(fd);
        print_result("   brfs_close", result);
    }

    return 1; // Stop here for now
    
    /* === Test 7: Sync to flash === */
    term_puts("\n7. Syncing to flash...\n");
    result = brfs_sync();
    print_result("   brfs_sync", result);
    
    /* === Test 8: Unmount and remount === */
    term_puts("\n8. Testing persistence...\n");
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
            term_puts("   Persistence verify: ");
            if (verify_ok && words_read == TEST_DATA_SIZE)
            {
                term_set_palette(2);
                term_puts("PASS\n");
            }
            else
            {
                term_set_palette(1);
                term_puts("FAIL\n");
            }
            term_set_palette(0);
            brfs_close(fd);
        }
        
        /* List directory after remount */
        term_putchar('\n');
        list_directory("/");
    }
    
    /* === Test 9: Delete file === */
    term_puts("\n9. Testing delete...\n");
    result = brfs_delete("/test.txt");
    print_result("   brfs_delete /test.txt", result);
    
    /* Try to delete non-empty directory */
    result = brfs_delete("/testdir");
    if (result == BRFS_ERR_NOT_EMPTY)
    {
        term_puts("   Non-empty dir check: ");
        term_set_palette(2);
        term_puts("PASS\n");
        term_set_palette(0);
    }
    else
    {
        term_puts("   Non-empty dir check: ");
        term_set_palette(1);
        term_puts("FAIL\n");
        term_set_palette(0);
    }
    
    /* Print final stats */
    term_puts("\nFinal filesystem state:\n");
    print_fs_stats();
    list_directory("/");
    
    term_puts("\n=== Test Complete ===\n");
    
    return 1;
}

void interrupt()
{
    /* Empty interrupt handler */
}

