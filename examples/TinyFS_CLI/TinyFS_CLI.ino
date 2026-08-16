#include <tinyfs.h>
#include <tinyfs_cli.h>

/* Command line buffer */
static char cmd_buffer[64];
static uint8_t cmd_len = 0;

/* Callback function to print CLI outputs directly to the hardware Serial port */
void serial_print_cb(const char *str) {
    Serial.print(str);
}

void setup() {
    /* Initialize Hardware Serial at 9600 baud rate */
    Serial.begin(9600);
    while (!Serial) {
        ; /* Wait for serial port connection (needed for native USB boards) */
    }

    Serial.println(F("\n============================================="));
    Serial.println(F("           TinyFS-UNO Console v0.1            "));
    Serial.println(F("============================================="));
    Serial.print(F("Mounting filesystem... "));

    /* Initialize filesystem structures and mount */
    tfs_init();
    int ret = tfs_mount();
    
    if (ret < 0) {
        Serial.println(F("FAILED (Unformatted)"));
        Serial.print(F("Formatting EEPROM... "));
        ret = tfs_format();
        if (ret == 0) {
            Serial.println(F("OK"));
        } else {
            Serial.print(F("ERROR: Format failed ("));
            Serial.print(ret);
            Serial.println(F(")"));
        }
    } else {
        Serial.println(F("OK"));
    }

    /* Print filesystem health overview */
    struct TFS_StatFS sfs;
    if (tfs_statfs(&sfs) == 0) {
        Serial.print(F("Sector:      "));
        Serial.println(tfs_ctx.active_sector == 0 ? 'A' : 'B');
        Serial.print(F("Active Files:"));
        Serial.println(sfs.file_count);
        Serial.print(F("Free Space:  "));
        Serial.print(sfs.free_space);
        Serial.println(F(" B"));
    }

    /* Initialize CLI console callback */
    tfs_cli_init(serial_print_cb);
    
    /* Print prompt */
    Serial.print(F("\ntinyfs> "));
}

void loop() {
    /* Read incoming characters from Serial Monitor */
    while (Serial.available() > 0) {
        char c = Serial.read();

        if (c == '\r' || c == '\n') {
            /* Execute command on newline reception */
            if (cmd_len > 0) {
                cmd_buffer[cmd_len] = '\0';
                Serial.println(); /* Print newline first */
                
                /* Execute the parsed command */
                tfs_cli_execute(cmd_buffer);
                
                /* Reset input buffer */
                cmd_len = 0;
            } else {
                Serial.println();
            }
            
            /* Print fresh prompt */
            Serial.print(F("tinyfs> "));
        }
        else if (c == '\b' || c == 127) {
            /* Handle backspace/delete visually */
            if (cmd_len > 0) {
                cmd_len--;
                Serial.print(F("\b \b")); /* Move back, write space, move back again */
            }
        }
        else {
            /* Store character if buffer is not full */
            if (cmd_len < sizeof(cmd_buffer) - 1) {
                /* Echo the character back to the console */
                Serial.print(c);
                cmd_buffer[cmd_len++] = c;
            }
        }
    }
}
