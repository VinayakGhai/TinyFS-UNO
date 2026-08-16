#include "tinyfs_gc.h"
#include "tinyfs_storage.h"
#include "tinyfs_record.h"
#include "tinyfs_metadata.h"
#include <string.h>

int tfs_gc_compact(uint16_t extra_needed) {
    /* 1. Retrieve the active superblock to get the current generation */
    struct Superblock active_sb;
    uint16_t active_sb_addr = 0;
    int ret = tfs_sb_select(&active_sb, &active_sb_addr);
    if (ret < 0) {
        return ret;
    }
    
    uint8_t current_sector = tfs_ctx.active_sector;
    uint8_t inactive_sector = (current_sector == 0) ? 1 : 0;
    uint32_t new_generation = active_sb.generation + 1;
    
    /* 2. Calculate the total size required for active files */
    uint16_t active_files_size = 0;
    struct RecordHeader headers[TFS_MAX_FILES];
    
    for (uint8_t i = 0; i < tfs_ctx.file_count; i++) {
        ret = tfs_record_read_header(tfs_ctx.file_offsets[i], &headers[i]);
        if (ret < 0) {
            return ret; /* Error reading active record */
        }
        active_files_size += sizeof(struct RecordHeader) + headers[i].data_len + 2 + 1;
    }
    
    /* Check if compaction + new data can fit into the destination log */
    if (active_files_size + extra_needed > TFS_SECTOR_LOG_SIZE) {
        return TFS_ERR_SECTOR_FULL; /* Filesystem is genuinely out of space */
    }
    
    /* 3. Write Sector Header of inactive sector as BUILDING */
    ret = tfs_sector_write_header(inactive_sector, new_generation, TFS_SECTOR_STATE_BUILDING);
    if (ret < 0) {
        return ret;
    }
    
    /* 4. Copy active files sequentially to the destination log */
    uint16_t dest_start = (inactive_sector == 0) ? TFS_SECTOR_A_ADDR : TFS_SECTOR_B_ADDR;
    uint16_t dest_ptr = dest_start + TFS_SECTOR_HEADER_SIZE;
    uint16_t new_offsets[TFS_MAX_FILES];
    
    uint8_t copy_buf[TFS_MAX_PAYLOAD];
    
    for (uint8_t i = 0; i < tfs_ctx.file_count; i++) {
        uint16_t src_offset = tfs_ctx.file_offsets[i];
        
        /* Read payload from source */
        if (headers[i].data_len > 0) {
            tfs_storage_read_buffer(src_offset + sizeof(struct RecordHeader), copy_buf, headers[i].data_len);
        }
        
        /* Write to destination */
        int written = tfs_record_write(dest_ptr, headers[i].filename, TFS_RECORD_STATUS_ACTIVE, copy_buf, headers[i].data_len);
        if (written < 0) {
            return written;
        }
        
        new_offsets[i] = dest_ptr;
        dest_ptr += written;
    }
    
    /* 5. Write the end-of-log marker (0xFF) at dest_ptr */
    tfs_storage_write_byte(dest_ptr, TFS_RECORD_STATUS_EMPTY);
    
    /* 6. Update destination sector header to VALID */
    ret = tfs_sector_update_state(inactive_sector, TFS_SECTOR_STATE_VALID);
    if (ret < 0) {
        return ret;
    }
    
    /* 7. Write the new superblock to the alternate slot */
    struct Superblock new_sb;
    new_sb.magic[0] = TFS_MAGIC_SB_0;
    new_sb.magic[1] = TFS_MAGIC_SB_1;
    new_sb.generation = new_generation;
    new_sb.active_sector = inactive_sector;
    memset(new_sb.padding, 0, sizeof(new_sb.padding));
    
    /* Alternate between superblock slots */
    uint8_t new_sb_idx = (active_sb_addr == TFS_SB1_ADDR) ? 1 : 0;
    ret = tfs_sb_write(new_sb_idx, &new_sb);
    if (ret < 0) {
        return ret;
    }
    
    /* 8. Mark the old sector header as EMPTY */
    ret = tfs_sector_write_header(current_sector, 0, TFS_SECTOR_STATE_EMPTY);
    if (ret < 0) {
        return ret;
    }
    
    /* 9. Update the runtime context */
    tfs_ctx.active_sector = inactive_sector;
    tfs_ctx.write_ptr = dest_ptr;
    memcpy(tfs_ctx.file_offsets, new_offsets, sizeof(new_offsets));
    
    return 0;
}
