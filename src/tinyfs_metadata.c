#include "tinyfs_metadata.h"
#include "tinyfs_storage.h"
#include "tinyfs_crc.h"
#include <string.h>

int tfs_sb_read(uint8_t sb_idx, struct Superblock *sb) {
    if (sb_idx > 1) {
        return TFS_ERR_NO_VALID_SB;
    }
    uint16_t addr = (sb_idx == 0) ? TFS_SB1_ADDR : TFS_SB2_ADDR;
    tfs_storage_read_buffer(addr, (uint8_t *)sb, sizeof(struct Superblock));
    return 0;
}

bool tfs_sb_validate(const struct Superblock *sb) {
    if (sb->magic[0] != TFS_MAGIC_SB_0 || sb->magic[1] != TFS_MAGIC_SB_1) {
        return false;
    }
    
    /* Calculate CRC-8 over the first 15 bytes */
    uint8_t calculated_crc = tfs_crc8(0, (const uint8_t *)sb, sizeof(struct Superblock) - 1);
    return calculated_crc == sb->crc8;
}

int tfs_sb_write(uint8_t sb_idx, const struct Superblock *sb) {
    if (sb_idx > 1) {
        return TFS_ERR_SB_WRITE_FAIL;
    }
    
    uint16_t addr = (sb_idx == 0) ? TFS_SB1_ADDR : TFS_SB2_ADDR;
    
    /* Create a local copy to populate the CRC */
    struct Superblock local_sb = *sb;
    local_sb.crc8 = tfs_crc8(0, (const uint8_t *)&local_sb, sizeof(struct Superblock) - 1);
    
    tfs_storage_write_buffer(addr, (const uint8_t *)&local_sb, sizeof(struct Superblock));
    return 0;
}

int tfs_sb_select(struct Superblock *active_sb, uint16_t *active_sb_addr) {
    struct Superblock sb1, sb2;
    int r1 = tfs_sb_read(0, &sb1);
    int r2 = tfs_sb_read(1, &sb2);
    
    bool v1 = (r1 == 0) && tfs_sb_validate(&sb1);
    bool v2 = (r2 == 0) && tfs_sb_validate(&sb2);
    
    if (v1 && v2) {
        /* Both superblocks are valid: choose the newest generation */
        if (sb1.generation >= sb2.generation) {
            *active_sb = sb1;
            if (active_sb_addr) *active_sb_addr = TFS_SB1_ADDR;
        } else {
            *active_sb = sb2;
            if (active_sb_addr) *active_sb_addr = TFS_SB2_ADDR;
        }
        return 0;
    } else if (v1) {
        /* Only Superblock 1 is valid */
        *active_sb = sb1;
        if (active_sb_addr) *active_sb_addr = TFS_SB1_ADDR;
        return 0;
    } else if (v2) {
        /* Only Superblock 2 is valid */
        *active_sb = sb2;
        if (active_sb_addr) *active_sb_addr = TFS_SB2_ADDR;
        return 0;
    }
    
    /* Neither superblock is valid: filesystem is unmounted/corrupt */
    return TFS_ERR_NO_VALID_SB;
}

int tfs_sector_read_header(uint8_t sector_id, struct SectorHeader *sh) {
    if (sector_id > 1) {
        return TFS_ERR_INVALID_SECTOR;
    }
    uint16_t addr = (sector_id == 0) ? TFS_SECTOR_A_ADDR : TFS_SECTOR_B_ADDR;
    tfs_storage_read_buffer(addr, (uint8_t *)sh, sizeof(struct SectorHeader));
    return 0;
}

bool tfs_sector_validate_header(const struct SectorHeader *sh, uint8_t expected_sector_id) {
    if (sh->magic != TFS_MAGIC_SECTOR) {
        return false;
    }
    if (sh->sector_id != expected_sector_id) {
        return false;
    }
    
    /* Calculate CRC-8 over the first 7 bytes of the sector header */
    uint8_t calculated_crc = tfs_crc8(0, (const uint8_t *)sh, sizeof(struct SectorHeader) - 1);
    return calculated_crc == sh->crc8;
}

int tfs_sector_write_header(uint8_t sector_id, uint32_t generation, uint8_t state) {
    if (sector_id > 1) {
        return TFS_ERR_SECTOR_WRITE_FAIL;
    }
    
    uint16_t addr = (sector_id == 0) ? TFS_SECTOR_A_ADDR : TFS_SECTOR_B_ADDR;
    
    struct SectorHeader sh;
    sh.magic = TFS_MAGIC_SECTOR;
    sh.sector_id = sector_id;
    sh.generation = generation;
    sh.state = state;
    sh.crc8 = tfs_crc8(0, (const uint8_t *)&sh, sizeof(struct SectorHeader) - 1);
    
    tfs_storage_write_buffer(addr, (const uint8_t *)&sh, sizeof(struct SectorHeader));
    return 0;
}

int tfs_sector_update_state(uint8_t sector_id, uint8_t new_state) {
    if (sector_id > 1) {
        return TFS_ERR_SECTOR_WRITE_FAIL;
    }
    
    uint16_t base_addr = (sector_id == 0) ? TFS_SECTOR_A_ADDR : TFS_SECTOR_B_ADDR;
    
    /* Read existing header */
    struct SectorHeader sh;
    int ret = tfs_sector_read_header(sector_id, &sh);
    if (ret < 0) {
        return ret;
    }
    
    /* 
     * Perform update. We modify the state byte and recalculate the CRC-8.
     * We only perform write operations on the state (offset 6) and CRC (offset 7) bytes
     * to prevent wearing out the other cells of the header sector.
     */
    sh.state = new_state;
    sh.crc8 = tfs_crc8(0, (const uint8_t *)&sh, sizeof(struct SectorHeader) - 1);
    
    tfs_storage_write_byte(base_addr + 6, sh.state);
    tfs_storage_write_byte(base_addr + 7, sh.crc8);
    
    return 0;
}
