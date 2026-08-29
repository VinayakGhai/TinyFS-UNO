#include "tinyfs.h"
#include "tinyfs_storage.h"
#include "tinyfs_crc.h"
#include "tinyfs_record.h"
#include "tinyfs_metadata.h"
#include "tinyfs_gc.h"
#include <string.h>
#include <stddef.h>

/* Define global filesystem context */
struct TFS_Context tfs_ctx;

/* Helper function to search the active files in RAM */
static int tfs_find_active_file(const char *name, uint8_t *idx_out) {
    for (uint8_t i = 0; i < tfs_ctx.file_count; i++) {
        uint16_t offset = tfs_ctx.file_offsets[i];
        char existing_name[TFS_MAX_FILENAME];
        
        /* Read the filename field directly from the record header in EEPROM */
        uint16_t name_addr = offset + offsetof(struct RecordHeader, filename);
        tfs_storage_read_buffer(name_addr, (uint8_t *)existing_name, TFS_MAX_FILENAME);
        
        if (strcmp(existing_name, name) == 0) {
            if (idx_out) *idx_out = i;
            return 0; /* Found */
        }
    }
    return -1; /* Not found */
}

/* Helper to remove an active file entry from the in-RAM list */
static void tfs_remove_ram_entry(uint8_t idx) {
    for (uint8_t i = idx; i < tfs_ctx.file_count - 1; i++) {
        tfs_ctx.file_offsets[i] = tfs_ctx.file_offsets[i + 1];
    }
    tfs_ctx.file_count--;
}

int tfs_init(void) {
    tfs_storage_init();
    memset(&tfs_ctx, 0, sizeof(struct TFS_Context));
    return 0;
}

int tfs_mount(void) {
    struct Superblock active_sb;
    uint16_t sb_addr = 0;
    
    /* 1. Locate and validate active superblock */
    int ret = tfs_sb_select(&active_sb, &sb_addr);
    if (ret < 0) {
        return ret; /* No valid superblock found */
    }
    
    /* 2. Read and validate sector header for the active sector */
    struct SectorHeader sh;
    ret = tfs_sector_read_header(active_sb.active_sector, &sh);
    if (ret < 0) {
        return ret;
    }
    if (!tfs_sector_validate_header(&sh, active_sb.active_sector)) {
        return TFS_ERR_INVALID_SECTOR;
    }
    if (sh.state != TFS_SECTOR_STATE_VALID) {
        return TFS_ERR_INVALID_SECTOR; /* A building/corrupt sector cannot be active */
    }
    
    /* 3. Initialize mounting context */
    tfs_ctx.active_sector = active_sb.active_sector;
    tfs_ctx.file_count = 0;
    
    uint16_t sector_start = (active_sb.active_sector == 0) ? TFS_SECTOR_A_ADDR : TFS_SECTOR_B_ADDR;
    tfs_ctx.write_ptr = sector_start + TFS_SECTOR_HEADER_SIZE;
    uint16_t log_end = sector_start + TFS_SECTOR_SIZE;
    
    /* 4. Sequentially scan the log area */
    while (tfs_ctx.write_ptr < log_end) {
        uint8_t status = tfs_storage_read_byte(tfs_ctx.write_ptr);
        
        /* Stop scanning if we hit unwritten/empty space */
        if (status == TFS_RECORD_STATUS_EMPTY) {
            break;
        }
        
        /* Validate the record structure */
        struct RecordHeader header;
        ret = tfs_record_validate(tfs_ctx.write_ptr, &header);
        if (ret < 0) {
            /* 
             * Interrupted write or corruption detected. 
             * Reclaim space starting from this point by halting the scan 
             * and marking this as the current write pointer.
             */
            break;
        }
        
        uint16_t record_len = sizeof(struct RecordHeader) + header.data_len + 2 + 1;
        uint8_t existing_idx = 0;
        int found = tfs_find_active_file(header.filename, &existing_idx);
        
        if (header.status == TFS_RECORD_STATUS_DELETED) {
            if (found == 0) {
                tfs_remove_ram_entry(existing_idx);
            }
        } else if (header.status == TFS_RECORD_STATUS_ACTIVE) {
            if (found == 0) {
                /* Update existing file pointer to the newer version */
                tfs_ctx.file_offsets[existing_idx] = tfs_ctx.write_ptr;
            } else {
                /* Create a new file entry in RAM */
                if (tfs_ctx.file_count >= TFS_MAX_FILES) {
                    return -1; /* Exceeded maximum active files limit */
                }
                tfs_ctx.file_offsets[tfs_ctx.file_count] = tfs_ctx.write_ptr;
                tfs_ctx.file_count++;
            }
        }
        
        tfs_ctx.write_ptr += record_len;
    }
    
    return 0;
}

int tfs_format(void) {
    /* 
     * Write brand-new Superblocks:
     * SB1 active, generation 1.
     */
    struct Superblock sb;
    sb.magic[0] = TFS_MAGIC_SB_0;
    sb.magic[1] = TFS_MAGIC_SB_1;
    sb.generation = 1;
    sb.active_sector = 0;
    memset(sb.padding, 0, sizeof(sb.padding));
    
    int ret = tfs_sb_write(0, &sb);
    if (ret < 0) return ret;
    
    /* Write backup superblock (SB2) with generation 0 (initially invalid/inactive) */
    sb.generation = 0;
    sb.active_sector = 0;
    ret = tfs_sb_write(1, &sb);
    if (ret < 0) return ret;
    
    /* Write Sector A Header as VALID with generation 1 */
    ret = tfs_sector_write_header(0, 1, TFS_SECTOR_STATE_VALID);
    if (ret < 0) return ret;
    
    /* Write Sector B Header as EMPTY with generation 0 */
    ret = tfs_sector_write_header(1, 0, TFS_SECTOR_STATE_EMPTY);
    if (ret < 0) return ret;
    
    /* 
     * Invalidate the log areas. By writing 0xFF (TFS_RECORD_STATUS_EMPTY) to the first
     * byte of both log areas, we ensure that sequential scanning stops immediately,
     * effectively clearing all existing files from the logical directory.
     */
    tfs_storage_write_byte(TFS_SECTOR_A_ADDR + TFS_SECTOR_HEADER_SIZE, TFS_RECORD_STATUS_EMPTY);
    tfs_storage_write_byte(TFS_SECTOR_B_ADDR + TFS_SECTOR_HEADER_SIZE, TFS_RECORD_STATUS_EMPTY);
    
    /* Re-mount the empty filesystem */
    return tfs_mount();
}

int tfs_write(const char *filename, const uint8_t *data, uint16_t len) {
    uint8_t idx = 0;
    int found = tfs_find_active_file(filename, &idx);
    
    /* Check if file is new and if we already reached the limit */
    if (found < 0 && tfs_ctx.file_count >= TFS_MAX_FILES) {
        return TFS_ERR_TOO_MANY_FILES;
    }
    
    uint16_t sector_start = (tfs_ctx.active_sector == 0) ? TFS_SECTOR_A_ADDR : TFS_SECTOR_B_ADDR;
    uint16_t log_end = sector_start + TFS_SECTOR_SIZE;
    uint16_t needed = sizeof(struct RecordHeader) + len + 2 + 1; /* 19 + len */
    
    /* Check log space bounds */
    if (tfs_ctx.write_ptr + needed > log_end) {
        /* Out of space: trigger compaction/garbage collection */
        int gc_ret = tfs_gc_compact(needed);
        if (gc_ret < 0) {
            return gc_ret;
        }
        /* Re-evaluate boundaries since active sector changed */
        sector_start = (tfs_ctx.active_sector == 0) ? TFS_SECTOR_A_ADDR : TFS_SECTOR_B_ADDR;
        log_end = sector_start + TFS_SECTOR_SIZE;
    }
    
    /* Write record to active sector */
    int record_size = tfs_record_write(tfs_ctx.write_ptr, filename, TFS_RECORD_STATUS_ACTIVE, data, len);
    if (record_size < 0) {
        return record_size;
    }
    
    /* Log runtime write metrics */
    tfs_metrics_log_logical_write(len);
    
    /* Update RAM context */
    if (found == 0) {
        tfs_ctx.file_offsets[idx] = tfs_ctx.write_ptr;
    } else {
        tfs_ctx.file_offsets[tfs_ctx.file_count] = tfs_ctx.write_ptr;
        tfs_ctx.file_count++;
    }
    
    /* Advance write pointer */
    tfs_ctx.write_ptr += record_size;
    
    return 0;
}

int tfs_read(const char *filename, uint8_t *buf, uint16_t len, uint16_t offset_bytes) {
    uint8_t idx = 0;
    int found = tfs_find_active_file(filename, &idx);
    if (found < 0) {
        return TFS_ERR_FILE_NOT_FOUND;
    }
    
    uint16_t file_addr = tfs_ctx.file_offsets[idx];
    struct RecordHeader header;
    int ret = tfs_record_read_header(file_addr, &header);
    if (ret < 0) {
        return ret;
    }
    
    if (offset_bytes >= header.data_len) {
        return 0; /* EOF */
    }
    
    uint16_t to_read = header.data_len - offset_bytes;
    if (to_read > len) {
        to_read = len;
    }
    
    uint16_t data_addr = file_addr + sizeof(struct RecordHeader) + offset_bytes;
    tfs_storage_read_buffer(data_addr, buf, to_read);
    
    return to_read;
}

int tfs_delete(const char *filename) {
    uint8_t idx = 0;
    int found = tfs_find_active_file(filename, &idx);
    if (found < 0) {
        return TFS_ERR_FILE_NOT_FOUND;
    }
    
    uint16_t sector_start = (tfs_ctx.active_sector == 0) ? TFS_SECTOR_A_ADDR : TFS_SECTOR_B_ADDR;
    uint16_t log_end = sector_start + TFS_SECTOR_SIZE;
    uint16_t needed = sizeof(struct RecordHeader) + 0 + 2 + 1; /* 19 bytes */
    
    if (tfs_ctx.write_ptr + needed > log_end) {
        int gc_ret = tfs_gc_compact(needed);
        if (gc_ret < 0) {
            return gc_ret;
        }
        sector_start = (tfs_ctx.active_sector == 0) ? TFS_SECTOR_A_ADDR : TFS_SECTOR_B_ADDR;
        log_end = sector_start + TFS_SECTOR_SIZE;
    }
    
    /* Write delete record */
    int record_size = tfs_record_write(tfs_ctx.write_ptr, filename, TFS_RECORD_STATUS_DELETED, NULL, 0);
    if (record_size < 0) {
        return record_size;
    }
    
    /* Log logical deletion (0 payload bytes written) */
    tfs_metrics_log_logical_write(0);
    
    /* Remove from RAM offset directory */
    tfs_remove_ram_entry(idx);
    
    /* Advance write pointer */
    tfs_ctx.write_ptr += record_size;
    
    return 0;
}

bool tfs_exists(const char *filename) {
    uint8_t idx = 0;
    return (tfs_find_active_file(filename, &idx) == 0);
}

int tfs_stat(const char *filename, struct TFS_Stat *stat_out) {
    uint8_t idx = 0;
    int found = tfs_find_active_file(filename, &idx);
    if (found < 0) {
        return TFS_ERR_FILE_NOT_FOUND;
    }
    
    uint16_t file_addr = tfs_ctx.file_offsets[idx];
    struct RecordHeader header;
    int ret = tfs_record_read_header(file_addr, &header);
    if (ret < 0) {
        return ret;
    }
    
    strncpy(stat_out->filename, header.filename, TFS_MAX_FILENAME);
    stat_out->size = header.data_len;
    stat_out->offset = file_addr;
    stat_out->status = header.status;
    
    return 0;
}

int tfs_statfs(struct TFS_StatFS *statfs_out) {
    uint16_t sector_start = (tfs_ctx.active_sector == 0) ? TFS_SECTOR_A_ADDR : TFS_SECTOR_B_ADDR;
    uint16_t log_end = sector_start + TFS_SECTOR_SIZE;
    
    statfs_out->total_eeprom = TFS_EEPROM_SIZE;
    statfs_out->usable_storage = TFS_SECTOR_LOG_SIZE;
    statfs_out->file_count = tfs_ctx.file_count;
    
    /* Calculate live data and active metadata overhead */
    statfs_out->live_data = 0;
    statfs_out->metadata_overhead = TFS_SUPERBLOCK_SIZE * 2 + TFS_SECTOR_HEADER_SIZE; /* SB1 + SB2 + Active SH */
    
    for (uint8_t i = 0; i < tfs_ctx.file_count; i++) {
        struct RecordHeader header;
        int ret = tfs_record_read_header(tfs_ctx.file_offsets[i], &header);
        if (ret == 0) {
            statfs_out->live_data += header.data_len;
            statfs_out->metadata_overhead += sizeof(struct RecordHeader) + 2 + 1; /* Record Header + CRC-16 + Commit Marker */
        }
    }
    
    /* Available space in the active sector log */
    if (tfs_ctx.write_ptr < log_end) {
        statfs_out->free_space = log_end - tfs_ctx.write_ptr;
    } else {
        statfs_out->free_space = 0;
    }
    
    /* Headroom until next compaction is required */
    statfs_out->compaction_headroom = statfs_out->free_space;
    
    /* Largest file payload that can currently be written */
    if (statfs_out->free_space > (sizeof(struct RecordHeader) + 2 + 1)) {
        statfs_out->largest_free_record = statfs_out->free_space - (sizeof(struct RecordHeader) + 2 + 1);
        if (statfs_out->largest_free_record > TFS_MAX_PAYLOAD) {
            statfs_out->largest_free_record = TFS_MAX_PAYLOAD;
        }
    } else {
        statfs_out->largest_free_record = 0;
    }
    
    return 0;
}

int tfs_check(struct TFS_CheckReport *report) {
    memset(report, 0, sizeof(struct TFS_CheckReport));
    
    /* 1. Validate Superblocks */
    struct Superblock sb1, sb2;
    int r1 = tfs_sb_read(0, &sb1);
    int r2 = tfs_sb_read(1, &sb2);
    
    report->sb1_valid = (r1 == 0) && tfs_sb_validate(&sb1);
    report->sb2_valid = (r2 == 0) && tfs_sb_validate(&sb2);
    
    struct Superblock active_sb;
    uint16_t active_sb_addr = 0;
    int select_ret = tfs_sb_select(&active_sb, &active_sb_addr);
    
    if (select_ret < 0) {
        /* No valid superblock! The filesystem is fully corrupted */
        report->healthy = false;
        return TFS_ERR_NO_VALID_SB;
    }
    
    report->active_sector = active_sb.active_sector;
    report->generation = active_sb.generation;
    report->file_count = tfs_ctx.file_count;
    
    /* 2. Validate Sector Headers */
    struct SectorHeader sh_active, sh_inactive;
    uint8_t active_sector_id = active_sb.active_sector;
    uint8_t inactive_sector_id = (active_sector_id == 0) ? 1 : 0;
    
    int sh_act_ret = tfs_sector_read_header(active_sector_id, &sh_active);
    report->active_sh_valid = (sh_act_ret == 0) && tfs_sector_validate_header(&sh_active, active_sector_id) && (sh_active.state == TFS_SECTOR_STATE_VALID);
    
    int sh_inact_ret = tfs_sector_read_header(inactive_sector_id, &sh_inactive);
    report->inactive_sh_valid = (sh_inact_ret == 0) && tfs_sector_validate_header(&sh_inactive, inactive_sector_id);
    
    /* 3. Scan the log area for record checks */
    uint16_t sector_start = (active_sector_id == 0) ? TFS_SECTOR_A_ADDR : TFS_SECTOR_B_ADDR;
    uint16_t scan_ptr = sector_start + TFS_SECTOR_HEADER_SIZE;
    uint16_t log_end = sector_start + TFS_SECTOR_SIZE;
    
    report->valid_records = 0;
    report->corrupt_records = 0;
    
    while (scan_ptr < log_end) {
        uint8_t status = tfs_storage_read_byte(scan_ptr);
        
        if (status == TFS_RECORD_STATUS_EMPTY) {
            break;
        }
        
        struct RecordHeader record_h;
        int validate_ret = tfs_record_validate(scan_ptr, &record_h);
        if (validate_ret == 0) {
            report->valid_records++;
            scan_ptr += sizeof(struct RecordHeader) + record_h.data_len + 2 + 1;
        } else {
            /* We hit a corruption! */
            report->corrupt_records++;
            break;
        }
    }
    
    report->free_space = log_end - scan_ptr;
    
    /* Health evaluation */
    report->healthy = report->sb1_valid && report->sb2_valid && 
                      report->active_sh_valid && report->inactive_sh_valid && 
                      (sh_inactive.state != TFS_SECTOR_STATE_BUILDING) &&
                      (report->corrupt_records == 0);
                      
    return 0;
}

int tfs_repair(void) {
    struct TFS_CheckReport report;
    int ret = tfs_check(&report);
    if (ret == TFS_ERR_NO_VALID_SB) {
        return TFS_ERR_NO_VALID_SB;
    }
    
    struct Superblock active_sb;
    uint16_t active_sb_addr = 0;
    tfs_sb_select(&active_sb, &active_sb_addr);
    
    /* Repair SB1 if corrupt */
    if (!report.sb1_valid) {
        struct Superblock sb2;
        tfs_sb_read(1, &sb2);
        tfs_sb_write(0, &sb2);
    }
    /* Repair SB2 if corrupt */
    if (!report.sb2_valid) {
        struct Superblock sb1;
        tfs_sb_read(0, &sb1);
        tfs_sb_write(1, &sb1);
    }
    
    /* Repair Sector Headers */
    uint8_t active_sector_id = active_sb.active_sector;
    uint8_t inactive_sector_id = (active_sector_id == 0) ? 1 : 0;
    
    if (!report.active_sh_valid) {
        tfs_sector_write_header(active_sector_id, active_sb.generation, TFS_SECTOR_STATE_VALID);
    }
    
    /* Force inactive sector header from BUILDING to EMPTY if interrupted */
    struct SectorHeader sh_inactive;
    tfs_sector_read_header(inactive_sector_id, &sh_inactive);
    if (sh_inactive.state == TFS_SECTOR_STATE_BUILDING) {
        tfs_sector_write_header(inactive_sector_id, 0, TFS_SECTOR_STATE_EMPTY);
    }
    
    /* Re-mount to load fresh state */
    return tfs_mount();
}

int tfs_rename(const char *old_filename, const char *new_filename) {
    if (tfs_find_active_file(old_filename, NULL) < 0) {
        return -1;
    }
    if (tfs_find_active_file(new_filename, NULL) >= 0) {
        return -1;
    }

    uint8_t buffer[256];
    int bytes_read = tfs_read_file(old_filename, buffer, sizeof(buffer));
    if (bytes_read < 0) return -1;

    int write_res = tfs_write_file(new_filename, buffer, bytes_read);
    if (write_res < 0) return -1;

    int delete_res = tfs_delete_file(old_filename);
    if (delete_res < 0) return -1;

    return 0;
}