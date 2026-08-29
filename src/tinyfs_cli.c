#include "tinyfs_cli.h"
#include "tinyfs.h"
#include "tinyfs_storage.h"
#include "tinyfs_crc.h"
#include "tinyfs_record.h"
#include "tinyfs_metadata.h"
#include "tinyfs_gc.h"

#ifndef ARDUINO
#include "tinyfs_eeprom.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>

#ifdef __AVR__
#include <avr/pgmspace.h>
#endif

static TFS_Print_Callback cli_print_cb = NULL;
static char format_buf[80];

#ifdef __AVR__
static void cli_printf_p(const char *fmt_p, ...) {
    if (!cli_print_cb) return;
    va_list args;
    va_start(args, fmt_p);
    vsnprintf_P(format_buf, sizeof(format_buf), fmt_p, args);
    va_end(args);
    cli_print_cb(format_buf);
}
#define cli_printf(fmt, ...) cli_printf_p(PSTR(fmt), ##__VA_ARGS__)
#else
static void cli_printf(const char *fmt, ...) {
    if (!cli_print_cb) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(format_buf, sizeof(format_buf), fmt, args);
    va_end(args);
    cli_print_cb(format_buf);
}
#endif

void tfs_cli_init(TFS_Print_Callback print_cb) {
    cli_print_cb = print_cb;
}

/* Helper to strip leading and trailing whitespace */
static char *trim_whitespace(char *str) {
    while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') {
        str++;
    }
    if (*str == 0) {
        return str;
    }
    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        end--;
    }
    *(end + 1) = '\0';
    return str;
}

/* Helper to parse arguments, handles quoted strings for payloads */
static int parse_args(char *args, char *arg1_out, char *arg2_out, int max_len) {
    args = trim_whitespace(args);
    if (*args == '\0') return 0;

    /* Extract first argument (e.g. filename) */
    char *space = strchr(args, ' ');
    if (!space) {
        strncpy(arg1_out, args, max_len - 1);
        arg1_out[max_len - 1] = '\0';
        return 1;
    }

    /* Copy first arg */
    int len = space - args;
    if (len >= max_len) len = max_len - 1;
    memcpy(arg1_out, args, len);
    arg1_out[len] = '\0';

    /* Locate second argument */
    char *second = trim_whitespace(space + 1);
    if (*second == '\0') {
        return 1;
    }

    /* If it is a quoted string, strip quotes */
    if (*second == '"') {
        second++;
        char *end_quote = strrchr(second, '"');
        if (end_quote) {
            *end_quote = '\0';
        }
        strncpy(arg2_out, second, max_len - 1);
    } else {
        strncpy(arg2_out, second, max_len - 1);
    }
    arg2_out[max_len - 1] = '\0';
    return 2;
}
static void cmd_rename(char *args);
/* Command implementations */

static void cmd_ls(void) {
    cli_printf("Files:\n");
    if (tfs_ctx.file_count == 0) {
        cli_printf("  (empty)\n");
        return;
    }
    
    for (uint8_t i = 0; i < tfs_ctx.file_count; i++) {
        uint16_t offset = tfs_ctx.file_offsets[i];
        struct RecordHeader header;
        if (tfs_record_read_header(offset, &header) == 0) {
            cli_printf("  %-12s  %4d B  (offset: 0x%03X)\n", header.filename, header.data_len, offset);
        }
    }
}

static void cmd_write(char *args) {
    char filename[16] = {0};
    char payload[64] = {0};
    int parsed = parse_args(args, filename, payload, 64);
    
    if (parsed < 2) {
        cli_printf("Usage: write <filename> \"<content>\"\n");
        return;
    }
    
    int ret = tfs_write(filename, (const uint8_t *)payload, strlen(payload));
    if (ret == 0) {
        cli_printf("OK\n");
    } else if (ret == TFS_ERR_SECTOR_FULL) {
        cli_printf("ERROR: Filesystem full\n");
    } else if (ret == TFS_ERR_TOO_MANY_FILES) {
        cli_printf("ERROR: Max file limit reached\n");
    } else {
        cli_printf("ERROR: Write failed (%d)\n", ret);
    }
}
static void cmd_rename(char *args) {
    char old_name[16] = {0};
    char new_name[16] = {0};
    int parsed = parse_args(args, old_name, new_name, sizeof(old_name));

    if (parsed < 2) {
        cli_printf("Usage: rename <old_filename> <new_filename>\n");
        return;
    }

    int ret = tfs_rename(old_name, new_name);
    if (ret == 0) {
        cli_printf("OK\n");
    } else {
        cli_printf("ERROR: Rename failed (%d)\n", ret);
    }
}
static void cmd_cat(char *args) {
    char filename[16] = {0};
    int parsed = parse_args(args, filename, NULL, 16);
    if (parsed < 1) {
        cli_printf("Usage: cat <filename>\n");
        return;
    }
    
    uint8_t read_buf[32];
    uint16_t offset = 0;
    
    while (1) {
        int read_bytes = tfs_read(filename, read_buf, sizeof(read_buf) - 1, offset);
        if (read_bytes < 0) {
            if (offset == 0) {
                cli_printf("ERROR: File not found\n");
            }
            break;
        }
        if (read_bytes == 0) {
            break; /* EOF */
        }
        read_buf[read_bytes] = '\0';
        cli_printf("%s", (char *)read_buf);
        offset += read_bytes;
    }
    if (offset > 0) {
        cli_printf("\n");
    }
}

static void cmd_rm(char *args) {
    char filename[16] = {0};
    int parsed = parse_args(args, filename, NULL, 16);
    if (parsed < 1) {
        cli_printf("Usage: rm <filename>\n");
        return;
    }
    
    int ret = tfs_delete(filename);
    if (ret == 0) {
        cli_printf("OK\n");
    } else if (ret == TFS_ERR_FILE_NOT_FOUND) {
        cli_printf("ERROR: File not found\n");
    } else {
        cli_printf("ERROR: Delete failed (%d)\n", ret);
    }
}

static void cmd_stat(char *args) {
    char filename[16] = {0};
    int parsed = parse_args(args, filename, NULL, 16);
    if (parsed < 1) {
        cli_printf("Usage: stat <filename>\n");
        return;
    }
    
    struct TFS_Stat st;
    int ret = tfs_stat(filename, &st);
    if (ret == 0) {
        cli_printf("File:     %s\n", st.filename);
        cli_printf("Size:     %d bytes\n", st.size);
        cli_printf("Address:  0x%03X\n", st.offset);
        cli_printf("Status:   %s\n", (st.status == TFS_RECORD_STATUS_ACTIVE) ? "ACTIVE" : "DELETED");
    } else {
        cli_printf("ERROR: File not found\n");
    }
}

static void cmd_statfs(void) {
    struct TFS_StatFS sfs;
    tfs_statfs(&sfs);
    
    cli_printf("Filesystem Statistics\n");
    cli_printf("---------------------\n");
    cli_printf("Total EEPROM:      %4d B\n", sfs.total_eeprom);
    cli_printf("Usable Log Sector: %4d B\n", sfs.usable_storage);
    cli_printf("Active Files:      %4d\n", sfs.file_count);
    cli_printf("Live User Data:    %4d B\n", sfs.live_data);
    cli_printf("Metadata Overhead: %4d B\n", sfs.metadata_overhead);
    cli_printf("Free Log Space:    %4d B\n", sfs.free_space);
    cli_printf("Max Free Record:   %4d B\n", sfs.largest_free_record);
    cli_printf("Compaction Margin: %4d B\n", sfs.compaction_headroom);
}

static void cmd_check(void) {
    struct TFS_CheckReport report;
    int ret = tfs_check(&report);
    if (ret < 0) {
        cli_printf("Filesystem: UNFORMATTED / FULLY CORRUPTED\n");
        return;
    }
    
    cli_printf("Filesystem check\n");
    cli_printf("----------------\n");
    cli_printf("Superblock 1:     %s\n", report.sb1_valid ? "OK" : "CORRUPT");
    cli_printf("Superblock 2:     %s\n", report.sb2_valid ? "OK" : "CORRUPT");
    cli_printf("Active Sector SH: %s (Sector %d)\n", report.active_sh_valid ? "OK" : "CORRUPT / Stuck", report.active_sector);
    cli_printf("Inactive Sector:  %s\n", report.inactive_sh_valid ? "OK" : "CORRUPT / Building");
    cli_printf("Valid records:    %d\n", report.valid_records);
    cli_printf("Corrupt records:  %d\n", report.corrupt_records);
    cli_printf("Free Space:       %d B\n", report.free_space);
    cli_printf("\nFilesystem: %s\n", report.healthy ? "HEALTHY" : "NEEDS REPAIR");
}

static void cmd_repair(void) {
    cli_printf("Initiating filesystem repair...\n");
    int ret = tfs_repair();
    if (ret == 0) {
        cli_printf("Repair completed successfully. Filesystem remounted.\n");
    } else {
        cli_printf("ERROR: Repair failed (%d)\n", ret);
    }
}

static void cmd_health(void) {
    struct Superblock active_sb;
    tfs_sb_select(&active_sb, NULL);
    
    cli_printf("EEPROM Health Report\n");
    cli_printf("--------------------\n");
    cli_printf("Capacity:         1024 B\n");
    cli_printf("Generation (GCs): %d\n", active_sb.generation);
    
    /* Write metrics */
    cli_printf("Logical Writes:   %d\n", tfs_metrics.logical_writes);
    cli_printf("Logical Bytes:    %d B\n", tfs_metrics.logical_bytes_written);
    cli_printf("Physical Bytes:   %d B\n", tfs_metrics.physical_bytes_written);
    cli_printf("Write Amplification: %.2f\n\n", tfs_metrics_get_write_amplification());

    /* Wear analysis */
    uint32_t min_w = 0, max_w = 0, avg_w = 0, spread = 0;
    
#ifdef ARDUINO
    /* 
     * On actual Arduino hardware, we estimate wear mathematically since 
     * we cannot inspect physical EEPROM cell cycles directly.
     */
    cli_printf("Wear map [ESTIMATED]\n");
    cli_printf("--------------------\n");
    
    uint32_t sb1_w = (active_sb.generation + 1) / 2;
    uint32_t sb2_w = active_sb.generation / 2;
    
    uint32_t secA_w = (active_sb.generation + 1) / 2;
    uint32_t secB_w = active_sb.generation / 2;
    
    /* Determine min and max estimated writes */
    min_w = sb2_w; /* Even superblock or inactive sector has fewer writes */
    max_w = secA_w;
    if (active_sb.active_sector == 0 && tfs_ctx.write_ptr > (TFS_SECTOR_A_ADDR + TFS_SECTOR_HEADER_SIZE)) {
        max_w += 1; /* Some bytes in active sector A got written in this cycle */
    }
    
    /* Compute simple averages */
    avg_w = (sb1_w * 16 + sb2_w * 16 + secA_w * 496 + secB_w * 496) / 1024;
    spread = max_w - min_w;
    
#else
    /* On the Host PC, we extract the exact wear statistics from the mock write counters */
    cli_printf("Wear map [EXACT - MEASURED]\n");
    cli_printf("---------------------------\n");
    
    min_w = 0xFFFFFFFF;
    max_w = 0;
    uint64_t total_writes = 0;
    
    for (uint16_t i = 0; i < TFS_EEPROM_SIZE; i++) {
        uint32_t wc = tfs_eeprom_mock_get_write_count(i);
        if (wc < min_w) min_w = wc;
        if (wc > max_w) max_w = wc;
        total_writes += wc;
    }
    avg_w = total_writes / TFS_EEPROM_SIZE;
    spread = max_w - min_w;
#endif

    cli_printf("Minimum:          %d\n", min_w);
    cli_printf("Maximum:          %d\n", max_w);
    cli_printf("Average:          %d\n", avg_w);
    cli_printf("Spread:           %d\n", spread);
}

static void cmd_benchmark(void) {
    cli_printf("Starting benchmark laboratory...\n");
    
    /* 1. Format the media */
    tfs_format();
    tfs_metrics_reset();
    
    cli_printf("Writing 4 files of 40 bytes each...\n");
    uint8_t payload[40];
    memset(payload, 0xAA, 40);
    
    tfs_write("/b1", payload, 40);
    tfs_write("/b2", payload, 40);
    tfs_write("/b3", payload, 40);
    tfs_write("/b4", payload, 40);
    
    cli_printf("Updating files to trigger Compaction...\n");
    /* Updating files repeatedly will exceed 488 bytes and force GC */
    tfs_write("/b1", payload, 40);
    tfs_write("/b2", payload, 40);
    tfs_write("/b3", payload, 40);
    tfs_write("/b4", payload, 40); /* This should trigger a compaction! */
    
    cli_printf("Deleting files...\n");
    tfs_delete("/b1");
    tfs_delete("/b2");
    
    /* Display metrics */
    struct Superblock active_sb;
    tfs_sb_select(&active_sb, NULL);
    
    cli_printf("\nBenchmark Results:\n");
    cli_printf("------------------\n");
    cli_printf("Logical Writes:      %d\n", tfs_metrics.logical_writes);
    cli_printf("Logical User Bytes:  %d B\n", tfs_metrics.logical_bytes_written);
    cli_printf("Physical EEPROM Chars: %d B\n", tfs_metrics.physical_bytes_written);
    cli_printf("Write Amplification:  %.2f\n", tfs_metrics_get_write_amplification());
    cli_printf("Compactions Triggered: %d\n", active_sb.generation - 1);
    cli_printf("Filesystem Health:     %s\n", tfs_exists("/b3") ? "OK" : "ERROR");
}

int tfs_cli_execute(const char *cmd_line) {
    char cmd_copy[128];
    strncpy(cmd_copy, cmd_line, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';
    
    char *trimmed = trim_whitespace(cmd_copy);
    if (*trimmed == '\0') {
        return 0;
    }
    
    /* Split into command and arguments */
    char *space = strchr(trimmed, ' ');
    char *cmd = trimmed;
    char *args = "";
    
    if (space) {
        *space = '\0';
        args = trim_whitespace(space + 1);
    }
    
    if (strcmp(cmd, "ls") == 0) {
        cmd_ls();
    } else if (strcmp(cmd, "write") == 0) {
        cmd_write(args);
    } else if (strcmp(cmd, "cat") == 0) {
        cmd_cat(args);
    } else if (strcmp(cmd, "rename") == 0) {
        cmd_rename(args);
    } else if (strcmp(cmd, "rm") == 0) {
        cmd_rm(args);
    } else if (strcmp(cmd, "stat") == 0) {
        cmd_stat(args);
    } else if (strcmp(cmd, "statfs") == 0) {
        cmd_statfs();
    } else if (strcmp(cmd, "check") == 0) {
        cmd_check();
    } else if (strcmp(cmd, "repair") == 0) {
        cmd_repair();
    } else if (strcmp(cmd, "health") == 0) {
        cmd_health();
    } else if (strcmp(cmd, "benchmark") == 0) {
        cmd_benchmark();
    } else if (strcmp(cmd, "reboot") == 0) {
        int ret = tfs_mount();
        if (ret == 0) {
            cli_printf("Reboot OK. Remounted.\n");
        } else {
            cli_printf("Reboot FAILED (%d)\n", ret);
        }
    } else {
        cli_printf("Unknown command: %s\n", cmd);
        return -1;
    }
    return 0;
}
