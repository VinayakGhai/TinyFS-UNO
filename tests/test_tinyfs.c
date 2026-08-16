#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "tinyfs_config.h"
#include "tinyfs_eeprom.h"
#include "tinyfs_storage.h"
#include "tinyfs_crc.h"
#include "tinyfs.h"
#include "tinyfs_record.h"
#include "tinyfs_metadata.h"
#include "tinyfs_cli.h"

/* Simple test framework helper */
#define RUN_TEST(test_func) \
    do { \
        printf("Running " #test_func "... "); \
        test_func(); \
        printf("PASS\n"); \
    } while (0)

static void test_layout_constants(void) {
    /* 
     * Double check layout values that are checked by compile-time 
     * static assertions. This ensures our test executable agrees 
     * with config constants.
     */
    assert(TFS_EEPROM_SIZE == 1024);
    assert(TFS_SUPERBLOCK_SIZE == 16);
    assert(TFS_SECTOR_SIZE == 496);
    assert(TFS_SECTOR_HEADER_SIZE == 8);
    assert(TFS_SECTOR_LOG_SIZE == 488);
    
    assert(TFS_SB1_ADDR == 0);
    assert(TFS_SB2_ADDR == 16);
    assert(TFS_SECTOR_A_ADDR == 32);
    assert(TFS_SECTOR_B_ADDR == 528);
}

static void test_mock_eeprom_raw(void) {
    tfs_eeprom_init();
    
    /* Ensure default uninitialized state is 0xFF */
    for (uint16_t i = 0; i < TFS_EEPROM_SIZE; i++) {
        assert(tfs_eeprom_read(i) == 0xFF);
        assert(tfs_eeprom_mock_get_write_count(i) == 0);
    }
    
    /* Test writing and reading individual bytes */
    tfs_eeprom_write(100, 0x42);
    assert(tfs_eeprom_read(100) == 0x42);
    assert(tfs_eeprom_mock_get_write_count(100) == 1);
    
    /* Re-write the same byte and check write count increment */
    tfs_eeprom_write(100, 0x42);
    assert(tfs_eeprom_read(100) == 0x42);
    assert(tfs_eeprom_mock_get_write_count(100) == 2);
    
    /* Check read/write out of bounds protection */
    assert(tfs_eeprom_read(TFS_EEPROM_SIZE) == 0xFF);
    tfs_eeprom_write(TFS_EEPROM_SIZE, 0x99);
    assert(tfs_eeprom_mock_get_write_count(TFS_EEPROM_SIZE) == 0);
}

static void test_storage_abstraction(void) {
    tfs_storage_init();
    
    /* Verify initial metrics are zero */
    assert(tfs_metrics.logical_writes == 0);
    assert(tfs_metrics.logical_bytes_written == 0);
    assert(tfs_metrics.physical_bytes_written == 0);
    assert(tfs_metrics_get_write_amplification() == 0.0f);
    
    /* Write buffer and check metrics */
    uint8_t write_buf[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    tfs_storage_write_buffer(200, write_buf, 4);
    
    uint8_t read_buf[4] = {0};
    tfs_storage_read_buffer(200, read_buf, 4);
    assert(memcmp(write_buf, read_buf, 4) == 0);
    
    /* Metrics check */
    assert(tfs_metrics.physical_bytes_written == 4);
    assert(tfs_eeprom_mock_get_write_count(200) == 1);
    assert(tfs_eeprom_mock_get_write_count(203) == 1);
    
    /* Record logical write */
    tfs_metrics_log_logical_write(2); /* Log user payload of 2 bytes */
    assert(tfs_metrics.logical_writes == 1);
    assert(tfs_metrics.logical_bytes_written == 2);
    
    /* Write amplification: 4 physical bytes / 2 logical bytes = 2.0 */
    float wa = tfs_metrics_get_write_amplification();
    assert(wa == 2.0f);
    
    /* Reset metrics */
    tfs_metrics_reset();
    assert(tfs_metrics.physical_bytes_written == 0);
}

static void test_crc_integrity(void) {
    /* Test CRC-8 with standard SMBus vector "123456789" */
    const char *data_8 = "123456789";
    uint8_t res_8 = tfs_crc8(0, (const uint8_t *)data_8, 9);
    assert(res_8 == 0xF4);
    
    /* Test CRC-16 with standard CCITT vector "123456789" */
    const char *data_16 = "123456789";
    uint16_t res_16 = tfs_crc16(0xFFFF, (const uint8_t *)data_16, 9);
    assert(res_16 == 0x29B1);
}

static void test_record_serialization(void) {
    tfs_storage_init();
    
    const char *filename = "/hello";
    const uint8_t payload[] = "Hello World";
    uint16_t payload_len = sizeof(payload); // 12 bytes including null terminator
    
    /* Write record at address 100 */
    int bytes_written = tfs_record_write(100, filename, TFS_RECORD_STATUS_ACTIVE, payload, payload_len);
    
    /* Total record size = 16 (header) + 12 (payload) + 2 (crc16) + 1 (commit) = 31 bytes */
    assert(bytes_written == 31);
    
    /* Read and validate header */
    struct RecordHeader header;
    int ret = tfs_record_read_header(100, &header);
    assert(ret == 0);
    assert(header.status == TFS_RECORD_STATUS_ACTIVE);
    assert(header.data_len == payload_len);
    assert(strcmp(header.filename, filename) == 0);
    
    /* Full record validation */
    ret = tfs_record_validate(100, &header);
    assert(ret == 0);
    
    /* Test cases for corruption detection */
    
    /* 1. Corrupt the header CRC-8 */
    uint8_t original_header_crc = tfs_storage_read_byte(100 + 15);
    tfs_storage_write_byte(100 + 15, original_header_crc ^ 0xFF); /* Corrupt CRC */
    assert(tfs_record_read_header(100, &header) == TFS_ERR_CRC_HEADER);
    assert(tfs_record_validate(100, &header) == TFS_ERR_CRC_HEADER);
    tfs_storage_write_byte(100 + 15, original_header_crc); /* Restore CRC */
    
    /* 2. Corrupt a data payload byte */
    uint8_t original_data_byte = tfs_storage_read_byte(100 + 16 + 2); // corrupt 3rd byte of payload
    tfs_storage_write_byte(100 + 16 + 2, original_data_byte ^ 0xFF);
    assert(tfs_record_validate(100, &header) == TFS_ERR_CRC_DATA);
    tfs_storage_write_byte(100 + 16 + 2, original_data_byte); /* Restore byte */
    
    /* 3. Corrupt the commit marker */
    uint8_t original_commit = tfs_storage_read_byte(100 + 16 + payload_len + 2);
    tfs_storage_write_byte(100 + 16 + payload_len + 2, original_commit ^ 0xFF);
    assert(tfs_record_validate(100, &header) == TFS_ERR_COMMIT_MARKER);
    tfs_storage_write_byte(100 + 16 + payload_len + 2, original_commit); /* Restore commit */
    
    /* 4. Test error limits */
    assert(tfs_record_write(100, "", TFS_RECORD_STATUS_ACTIVE, payload, payload_len) == TFS_ERR_FILENAME_EMPTY);
    assert(tfs_record_write(100, "/toolongnamehere", TFS_RECORD_STATUS_ACTIVE, payload, payload_len) == TFS_ERR_FILENAME_TOOLONG);
    assert(tfs_record_write(TFS_EEPROM_SIZE - 20, filename, TFS_RECORD_STATUS_ACTIVE, payload, payload_len) == TFS_ERR_RECORD_INVALID);
}

static void test_metadata_operations(void) {
    tfs_storage_init();
    
    /* 1. Test Superblock writes and selection */
    struct Superblock sb1;
    sb1.magic[0] = TFS_MAGIC_SB_0;
    sb1.magic[1] = TFS_MAGIC_SB_1;
    sb1.generation = 10;
    sb1.active_sector = 0;
    memset(sb1.padding, 0, 8);
    
    struct Superblock sb2;
    sb2.magic[0] = TFS_MAGIC_SB_0;
    sb2.magic[1] = TFS_MAGIC_SB_1;
    sb2.generation = 12;
    sb2.active_sector = 1;
    memset(sb2.padding, 0, 8);
    
    /* Write both superblocks */
    assert(tfs_sb_write(0, &sb1) == 0);
    assert(tfs_sb_write(1, &sb2) == 0);
    
    /* Read back and validate selection */
    struct Superblock active_sb;
    uint16_t active_addr = 0xFFFF;
    int ret = tfs_sb_select(&active_sb, &active_addr);
    assert(ret == 0);
    assert(active_sb.generation == 12);
    assert(active_sb.active_sector == 1);
    assert(active_addr == TFS_SB2_ADDR);
    
    /* Corrupt SB2 (active one) and check fallback to SB1 */
    tfs_storage_write_byte(TFS_SB2_ADDR + 15, 0x00); // corrupt CRC8 of SB2
    ret = tfs_sb_select(&active_sb, &active_addr);
    assert(ret == 0);
    assert(active_sb.generation == 10);
    assert(active_sb.active_sector == 0);
    assert(active_addr == TFS_SB1_ADDR);
    
    /* Corrupt SB1 too and check selection failure */
    tfs_storage_write_byte(TFS_SB1_ADDR + 15, 0x00); // corrupt CRC8 of SB1
    ret = tfs_sb_select(&active_sb, &active_addr);
    assert(ret == TFS_ERR_NO_VALID_SB);
    
    /* Restore and clean storage */
    tfs_storage_init();
    
    /* 2. Test Sector Headers */
    /* Write Sector A Header */
    assert(tfs_sector_write_header(0, 10, TFS_SECTOR_STATE_VALID) == 0);
    
    struct SectorHeader sh;
    assert(tfs_sector_read_header(0, &sh) == 0);
    assert(tfs_sector_validate_header(&sh, 0) == true);
    assert(sh.generation == 10);
    assert(sh.state == TFS_SECTOR_STATE_VALID);
    
    /* Check validation failures on Sector ID mismatch */
    assert(tfs_sector_validate_header(&sh, 1) == false);
    
    /* Update state to BUILDING and verify it only writes 2 bytes (state & crc8) */
    tfs_eeprom_mock_reset_write_counts();
    assert(tfs_sector_update_state(0, TFS_SECTOR_STATE_BUILDING) == 0);
    
    /* Read back and verify */
    assert(tfs_sector_read_header(0, &sh) == 0);
    assert(tfs_sector_validate_header(&sh, 0) == true);
    assert(sh.state == TFS_SECTOR_STATE_BUILDING);
    
    /* Verify wear optimization: only offset 38 (state) and 39 (crc8) were written */
    for (uint16_t i = 32; i < 40; i++) {
        uint32_t wc = tfs_eeprom_mock_get_write_count(i);
        if (i == 38 || i == 39) {
            assert(wc == 1);
        } else {
            assert(wc == 0);
        }
    }
}

static void test_mount_and_scan(void) {
    /* 1. Format the storage */
    tfs_init();
    assert(tfs_format() == 0);
    
    /* Verify initial context */
    assert(tfs_ctx.active_sector == 0);
    assert(tfs_ctx.write_ptr == TFS_SECTOR_A_ADDR + TFS_SECTOR_HEADER_SIZE);
    assert(tfs_ctx.file_count == 0);
    
    /* Write direct records sequentially to Sector A log */
    uint16_t ptr = tfs_ctx.write_ptr;
    
    /* File 1: /f1, Active */
    int len1 = tfs_record_write(ptr, "/f1", TFS_RECORD_STATUS_ACTIVE, (const uint8_t *)"abc", 3);
    assert(len1 == 22); // 16 header + 3 payload + 2 crc + 1 commit
    ptr += len1;
    
    /* File 2: /f2, Active */
    int len2 = tfs_record_write(ptr, "/f2", TFS_RECORD_STATUS_ACTIVE, (const uint8_t *)"defgh", 5);
    assert(len2 == 24); // 16 + 5 + 2 + 1
    ptr += len2;
    
    /* File 1 Update: /f1, new payload, Active */
    int len3 = tfs_record_write(ptr, "/f1", TFS_RECORD_STATUS_ACTIVE, (const uint8_t *)"hello", 5);
    assert(len3 == 24);
    ptr += len3;
    
    /* File 3: /f3, Active */
    int len4 = tfs_record_write(ptr, "/f3", TFS_RECORD_STATUS_ACTIVE, (const uint8_t *)"x", 1);
    assert(len4 == 20); // 16 + 1 + 2 + 1
    ptr += len4;
    
    /* File 2 Deletion: /f2, Deleted */
    int len5 = tfs_record_write(ptr, "/f2", TFS_RECORD_STATUS_DELETED, NULL, 0);
    assert(len5 == 19); // 16 + 0 + 2 + 1
    ptr += len5;
    
    /* Remount and verify list reconstruction */
    assert(tfs_mount() == 0);
    
    /* Active Files should be:
     * - /f1 (at offset corresponding to the update, len3 write)
     * - /f3 (at offset corresponding to the len4 write)
     * /f2 was deleted, so it shouldn't be active!
     */
    assert(tfs_ctx.file_count == 2);
    
    /* Let's verify files by reading their filenames from EEPROM */
    char name1[TFS_MAX_FILENAME];
    tfs_storage_read_buffer(tfs_ctx.file_offsets[0] + offsetof(struct RecordHeader, filename), (uint8_t *)name1, TFS_MAX_FILENAME);
    assert(strcmp(name1, "/f1") == 0);
    assert(tfs_ctx.file_offsets[0] == TFS_SECTOR_A_ADDR + TFS_SECTOR_HEADER_SIZE + len1 + len2);
    
    char name2[TFS_MAX_FILENAME];
    tfs_storage_read_buffer(tfs_ctx.file_offsets[1] + offsetof(struct RecordHeader, filename), (uint8_t *)name2, TFS_MAX_FILENAME);
    assert(strcmp(name2, "/f3") == 0);
    assert(tfs_ctx.file_offsets[1] == TFS_SECTOR_A_ADDR + TFS_SECTOR_HEADER_SIZE + len1 + len2 + len3);
    
    /* Check write pointer was advanced correctly */
    assert(tfs_ctx.write_ptr == TFS_SECTOR_A_ADDR + TFS_SECTOR_HEADER_SIZE + len1 + len2 + len3 + len4 + len5);
}

static void test_file_apis(void) {
    tfs_init();
    assert(tfs_format() == 0);
    
    /* 1. Write file1 */
    const uint8_t data1[] = "HelloWorld";
    assert(tfs_write("/file1", data1, 10) == 0);
    assert(tfs_exists("/file1") == true);
    
    /* Stat checks */
    struct TFS_Stat st;
    assert(tfs_stat("/file1", &st) == 0);
    assert(st.size == 10);
    assert(strcmp(st.filename, "/file1") == 0);
    assert(st.status == TFS_RECORD_STATUS_ACTIVE);
    assert(st.offset == TFS_SECTOR_A_ADDR + TFS_SECTOR_HEADER_SIZE);
    
    /* Read checks */
    uint8_t read_buf[32] = {0};
    int bytes_read = tfs_read("/file1", read_buf, 32, 0);
    assert(bytes_read == 10);
    assert(memcmp(read_buf, "HelloWorld", 10) == 0);
    
    /* Read offset check */
    memset(read_buf, 0, 32);
    bytes_read = tfs_read("/file1", read_buf, 32, 5);
    assert(bytes_read == 5);
    assert(memcmp(read_buf, "World", 5) == 0);
    
    /* Read EOF check */
    assert(tfs_read("/file1", read_buf, 32, 10) == 0);
    assert(tfs_read("/file1", read_buf, 32, 20) == 0);
    
    /* 2. Update file1 */
    const uint8_t data1_up[] = "TestPayloadUpdated";
    assert(tfs_write("/file1", data1_up, 18) == 0);
    
    assert(tfs_stat("/file1", &st) == 0);
    assert(st.size == 18);
    assert(st.offset == TFS_SECTOR_A_ADDR + TFS_SECTOR_HEADER_SIZE + 29); /* 16 header + 10 payload + 2 crc + 1 commit = 29 */
    
    memset(read_buf, 0, 32);
    assert(tfs_read("/file1", read_buf, 32, 0) == 18);
    assert(memcmp(read_buf, "TestPayloadUpdated", 18) == 0);
    
    /* 3. Write file2 */
    const uint8_t data2[] = "xyz";
    assert(tfs_write("/file2", data2, 3) == 0);
    
    /* StatFS check */
    struct TFS_StatFS sfs;
    assert(tfs_statfs(&sfs) == 0);
    assert(sfs.total_eeprom == 1024);
    assert(sfs.file_count == 2);
    assert(sfs.live_data == 18 + 3);
    assert(sfs.usable_storage == 488);
    assert(sfs.metadata_overhead == 78); /* 32 SBs + 8 SH + 19 * 2 = 78 */
    
    /* 4. Delete file1 */
    assert(tfs_delete("/file1") == 0);
    assert(tfs_exists("/file1") == false);
    assert(tfs_read("/file1", read_buf, 32, 0) == TFS_ERR_FILE_NOT_FOUND);
    assert(tfs_stat("/file1", &st) == TFS_ERR_FILE_NOT_FOUND);
    
    /* StatFS after deletion */
    assert(tfs_statfs(&sfs) == 0);
    assert(sfs.file_count == 1);
    assert(sfs.live_data == 3);
    assert(sfs.metadata_overhead == 59); /* 32 + 8 + 19 = 59 */
    
    /* 5. Max file limit check */
    char name[12];
    for (int i = 0; i < 7; i++) {
        sprintf(name, "/file_x%d", i);
        assert(tfs_write(name, (const uint8_t *)"a", 1) == 0);
    }
    assert(tfs_ctx.file_count == 8);
    
    /* Writing 9th file should fail with TFS_ERR_TOO_MANY_FILES */
    assert(tfs_write("/overflow", (const uint8_t *)"a", 1) == TFS_ERR_TOO_MANY_FILES);
    
    /* 6. Sector overflow check */
    assert(tfs_format() == 0);
    uint8_t large_buf[256];
    memset(large_buf, 0xAA, 256);
    int res = tfs_write("/large1", large_buf, 250);
    printf("tfs_write(/large1) returned: %d\n", res);
    assert(res == 0); /* Takes 19 + 250 = 269 bytes. Write ptr is now 40 + 269 = 309. */
    
    /* Write another file of size 220 bytes. 309 + 19 + 220 = 548, which exceeds sector log boundary (528) */
    assert(tfs_write("/large2", large_buf, 220) == TFS_ERR_SECTOR_FULL);
}

static void test_compaction(void) {
    tfs_init();
    assert(tfs_format() == 0);
    
    /* Create a 100-byte block */
    uint8_t dummy[100];
    memset(dummy, 0x55, 100);
    
    /* 1. Write three files of size 100 bytes each -> fits in Sector A */
    assert(tfs_write("/f1", dummy, 100) == 0);
    assert(tfs_write("/f2", dummy, 100) == 0);
    assert(tfs_write("/f3", dummy, 100) == 0);
    
    /* Write ptr is at 40 + 3 * (19 + 100) = 397 */
    assert(tfs_ctx.write_ptr == 397);
    assert(tfs_ctx.active_sector == 0);
    
    /* 2. Update /f1 with 100 bytes -> still fits in Sector A (397 + 119 = 516 <= 528) */
    assert(tfs_write("/f1", dummy, 100) == 0);
    assert(tfs_ctx.write_ptr == 516);
    
    /* 3. Write /f4 (100 bytes) -> doesn't fit! (516 + 119 = 635 > 528). Should trigger compaction! */
    assert(tfs_write("/f4", dummy, 100) == 0);
    
    /* Compaction should have:
     * - Checked sizes, migrated /f2, /f3, and the latest /f1 to Sector B.
     * - Active files: 4.
     * - Active sector: Sector B (1).
     * - Generation: 2.
     * - Write ptr should be: Sector B log start + 3 migrated files + new file (/f4)
     *   dest_start (536) + 3 * 119 (migrated) + 119 (new /f4) = 536 + 476 = 1012.
     */
    assert(tfs_ctx.active_sector == 1);
    assert(tfs_ctx.file_count == 4);
    assert(tfs_ctx.write_ptr == 1012);
    
    /* Verify files exist and read back correctly */
    assert(tfs_exists("/f1") == true);
    assert(tfs_exists("/f2") == true);
    assert(tfs_exists("/f3") == true);
    assert(tfs_exists("/f4") == true);
    
    uint8_t read_buf[100];
    assert(tfs_read("/f1", read_buf, 100, 0) == 100);
    assert(tfs_read("/f4", read_buf, 100, 0) == 100);
    
    /* Verify sector header states on EEPROM */
    struct SectorHeader shA, shB;
    assert(tfs_sector_read_header(0, &shA) == 0);
    assert(tfs_sector_read_header(1, &shB) == 0);
    
    assert(shA.state == TFS_SECTOR_STATE_EMPTY);
    assert(shB.state == TFS_SECTOR_STATE_VALID);
    assert(shB.generation == 2);
    
    /* Verify superblock generation on EEPROM is 2 */
    struct Superblock active_sb;
    uint16_t sb_addr = 0;
    assert(tfs_sb_select(&active_sb, &sb_addr) == 0);
    assert(active_sb.generation == 2);
    assert(active_sb.active_sector == 1);
    
    /* Verify we alternate superblock slots: SB2 (addr 16) should be active */
    assert(sb_addr == TFS_SB2_ADDR);
}

static void test_check_and_repair(void) {
    tfs_init();
    assert(tfs_format() == 0);
    
    struct TFS_CheckReport report;
    assert(tfs_check(&report) == 0);
    assert(report.healthy == true);
    assert(report.sb1_valid == true);
    assert(report.sb2_valid == true);
    assert(report.active_sh_valid == true);
    assert(report.inactive_sh_valid == true);
    
    /* 1. Corrupt SB1 and verify check detects it */
    tfs_storage_write_byte(TFS_SB1_ADDR + 15, 0x00); /* Corrupt SB1 CRC */
    assert(tfs_check(&report) == 0);
    assert(report.healthy == false);
    assert(report.sb1_valid == false);
    assert(report.sb2_valid == true);
    
    /* Repair SB1 and check health is restored */
    assert(tfs_repair() == 0);
    assert(tfs_check(&report) == 0);
    assert(report.healthy == true);
    assert(report.sb1_valid == true);
    assert(report.sb2_valid == true);
    
    /* 2. Write a file and corrupt inactive sector header state to BUILDING */
    const uint8_t data[] = "test";
    assert(tfs_write("/file", data, 4) == 0);
    
    /* Set Sector B Header state to BUILDING (interrupted compaction simulation) */
    tfs_storage_write_byte(TFS_SECTOR_B_ADDR + 6, TFS_SECTOR_STATE_BUILDING);
    /* Recalculate CRC */
    struct SectorHeader sh_inact;
    assert(tfs_sector_read_header(1, &sh_inact) == 0);
    sh_inact.crc8 = tfs_crc8(0, (const uint8_t *)&sh_inact, sizeof(struct SectorHeader) - 1);
    tfs_storage_write_byte(TFS_SECTOR_B_ADDR + 7, sh_inact.crc8);
    
    /* Check report detects building state of inactive sector */
    assert(tfs_check(&report) == 0);
    assert(report.healthy == false);
    
    /* Repair and verify inactive sector is cleared to EMPTY */
    assert(tfs_repair() == 0);
    assert(tfs_check(&report) == 0);
    assert(report.healthy == true);
    
    /* Verify Sector B is now EMPTY */
    assert(tfs_sector_read_header(1, &sh_inact) == 0);
    assert(sh_inact.state == TFS_SECTOR_STATE_EMPTY);
    
    /* 3. Corrupt active sector header (Sector A) */
    tfs_storage_write_byte(TFS_SECTOR_A_ADDR + 7, 0xFF); /* Corrupt active SH CRC */
    assert(tfs_check(&report) == 0);
    assert(report.healthy == false);
    assert(report.active_sh_valid == false);
    
    /* Repair active header and verify health */
    assert(tfs_repair() == 0);
    assert(tfs_check(&report) == 0);
    assert(report.healthy == true);
    assert(report.active_sh_valid == true);
}

static void test_fault_injection_and_recovery(void) {
    tfs_init();
    assert(tfs_format() == 0);
    
    /* Write an initial stable file */
    const uint8_t data1[] = "stablefile";
    assert(tfs_write("/file1", data1, 10) == 0);
    
    /* Save the EEPROM state prior to writing /file2 */
    uint8_t eeprom_save[TFS_EEPROM_SIZE];
    uint8_t *mock_buf = tfs_eeprom_mock_get_buffer();
    memcpy(eeprom_save, mock_buf, TFS_EEPROM_SIZE);
    
    /* Save the context state too */
    struct TFS_Context ctx_save = tfs_ctx;
    
    const uint8_t data2[] = "hello";
    /* Total record size = 16 (header) + 5 (data) + 2 (crc16) + 1 (commit) = 24 writes */
    
    /* Loop over all possible write limits to test partial writes */
    for (int limit = 0; limit < 24; limit++) {
        /* Restore pre-write state */
        memcpy(mock_buf, eeprom_save, TFS_EEPROM_SIZE);
        tfs_ctx = ctx_save;
        
        /* Set write limit */
        tfs_eeprom_mock_set_write_limit(limit);
        
        /* Attempt write: should fail or run out of writes */
        int res = tfs_write("/file2", data2, 5);
        assert(res < 0 || tfs_eeprom_mock_was_aborted());
        
        /* Remount: recovery engine must handle the interruption */
        assert(tfs_mount() == 0);
        
        /* Invariants check */
        assert(tfs_exists("/file1") == true);
        assert(tfs_exists("/file2") == false); /* /file2 must not exist! */
        
        /* Verify read output of /file1 */
        uint8_t read_buf[32];
        assert(tfs_read("/file1", read_buf, 32, 0) == 10);
        assert(memcmp(read_buf, "stablefile", 10) == 0);
        
        /* The write pointer must have rolled back to the pre-write location */
        assert(tfs_ctx.write_ptr == ctx_save.write_ptr);
    }
    
    /* Restore and write fully without limit */
    memcpy(mock_buf, eeprom_save, TFS_EEPROM_SIZE);
    tfs_ctx = ctx_save;
    tfs_eeprom_mock_set_write_limit(-1); /* Disable limit */
    assert(tfs_write("/file2", data2, 5) == 0);
    
    assert(tfs_mount() == 0);
    assert(tfs_exists("/file1") == true);
    assert(tfs_exists("/file2") == true);
    
    
    /* 2. Test compaction interruption */
    tfs_init();
    assert(tfs_format() == 0);
    
    uint8_t dummy[100];
    memset(dummy, 0xAA, 100);
    assert(tfs_write("/f1", dummy, 100) == 0);
    assert(tfs_write("/f2", dummy, 100) == 0);
    assert(tfs_write("/f3", dummy, 100) == 0);
    assert(tfs_write("/f1", dummy, 100) == 0); /* Write pointer is at 516 */
    
    /* Save stable state before compaction */
    memcpy(eeprom_save, mock_buf, TFS_EEPROM_SIZE);
    ctx_save = tfs_ctx;
    
    /* Set limit to interrupt compaction midway */
    /* Compaction involves writing a sector header (8B) + copying files + writing superblock (16B) */
    /* If we set limit to 15, it will fail during file copying */
    memcpy(mock_buf, eeprom_save, TFS_EEPROM_SIZE);
    tfs_ctx = ctx_save;
    tfs_eeprom_mock_set_write_limit(15);
    
    int res = tfs_write("/f4", dummy, 100);
    assert(res < 0 || tfs_eeprom_mock_was_aborted());
    
    /* Remount after crash: must fall back to original Sector A log and files! */
    assert(tfs_mount() == 0);
    assert(tfs_ctx.active_sector == 0);
    assert(tfs_ctx.file_count == 3);
    assert(tfs_exists("/f1") == true);
    assert(tfs_exists("/f2") == true);
    assert(tfs_exists("/f3") == true);
    assert(tfs_exists("/f4") == false);
    assert(tfs_ctx.write_ptr == ctx_save.write_ptr);
    
    /* Restore write limits */
    tfs_eeprom_mock_set_write_limit(-1);
}

static void dummy_print_cb(const char *str) {
    /* Print to console to inspect CLI output in test logs */
    printf("%s", str);
}

static void test_cli_commands(void) {
    printf("\n--- Start of CLI Command Output Simulation ---\n");
    tfs_cli_init(dummy_print_cb);
    
    tfs_init();
    tfs_format();
    
    /* Test write */
    assert(tfs_cli_execute("write /cli_file \"hello_cli\"") == 0);
    assert(tfs_exists("/cli_file") == true);
    
    /* Test ls */
    assert(tfs_cli_execute("ls") == 0);
    
    /* Test cat */
    assert(tfs_cli_execute("cat /cli_file") == 0);
    
    /* Test stat */
    assert(tfs_cli_execute("stat /cli_file") == 0);
    
    /* Test statfs */
    assert(tfs_cli_execute("statfs") == 0);
    
    /* Test check */
    assert(tfs_cli_execute("check") == 0);
    
    /* Test health */
    assert(tfs_cli_execute("health") == 0);
    
    /* Test benchmark */
    assert(tfs_cli_execute("benchmark") == 0);
    
    /* Test rm */
    assert(tfs_cli_execute("rm /cli_file") == 0);
    assert(tfs_exists("/cli_file") == false);
    
    /* Test unknown command */
    assert(tfs_cli_execute("unknown") == -1);
    
    printf("--- End of CLI Command Output Simulation ---\n\n");
}

int main(void) {
    printf("===================================================\n");
    printf(" TinyFS-UNO: Subsystems 1-9 Test Suite (Host)      \n");
    printf("===================================================\n");
    
    RUN_TEST(test_layout_constants);
    RUN_TEST(test_mock_eeprom_raw);
    RUN_TEST(test_storage_abstraction);
    RUN_TEST(test_crc_integrity);
    RUN_TEST(test_record_serialization);
    RUN_TEST(test_metadata_operations);
    RUN_TEST(test_mount_and_scan);
    RUN_TEST(test_file_apis);
    RUN_TEST(test_compaction);
    RUN_TEST(test_check_and_repair);
    RUN_TEST(test_fault_injection_and_recovery);
    RUN_TEST(test_cli_commands);
    
    printf("\nAll Subsystems 1-9 tests PASSED successfully!\n");
    return 0;
}
