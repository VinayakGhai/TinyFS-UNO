#include <tinyfs.h>

void setup() {
    Serial.begin(9600);
    while (!Serial) {
        ; /* Wait for native USB serial port connection if applicable */
    }

    Serial.println(F("\n============================================="));
    Serial.println(F("      TinyFS-UNO Hardware Smoke Test         "));
    Serial.println(F("============================================="));
    
    tfs_init();
    int res = tfs_mount();
    if (res < 0) {
        Serial.println(F("Mount failed. Formatting storage..."));
        tfs_format();
        res = tfs_mount();
    }

    /* Check if smoke test file already exists from a previous boot */
    if (tfs_exists("/smoketest")) {
        Serial.println(F("PASS: Persistent file /smoketest survived reboot!"));
        uint8_t read_buf[32];
        int read_bytes = tfs_read("/smoketest", read_buf, sizeof(read_buf) - 1, 0);
        if (read_bytes > 0) {
            read_buf[read_bytes] = '\0';
            Serial.print(F("Payload content: "));
            Serial.println((char *)read_buf);
        }
    } else {
        Serial.println(F("Creating new file /smoketest..."));
        const char *payload = "TinyFS_OK_2026";
        int w_res = tfs_write("/smoketest", (const uint8_t *)payload, strlen(payload));
        if (w_res == 0) {
            Serial.println(F("File created successfully."));
            Serial.println(F("--> Press the physical Reset button on your Uno to verify persistence across boot!"));
        } else {
            Serial.print(F("ERROR: Write failed with code "));
            Serial.println(w_res);
        }
    }
}

void loop() {
    /* Standby */
}
