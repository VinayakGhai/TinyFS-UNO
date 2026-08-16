/**
 * @file tinyfs_cli.h
 * @brief TinyFS Command Line Interface.
 *
 * Implements command parsing and output formatting for both the Host test executable
 * and the Arduino Serial monitor console.
 */

#ifndef TINYFS_CLI_H
#define TINYFS_CLI_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback function type for printing console outputs.
 *
 * @param[in] null-terminated string to print.
 */
typedef void (*TFS_Print_Callback)(const char *str);

/**
 * @brief Initialize the CLI with a printing callback function.
 *
 * Must be called before tfs_cli_execute().
 *
 * @param[in] print_cb Callback function for output (e.g., Serial.print wrapper).
 */
void tfs_cli_init(TFS_Print_Callback print_cb);

/**
 * @brief Parse and execute a single command line.
 *
 * Supported commands include:
 * - `ls` - List files
 * - `write <filename> <data>` - Write data to file
 * - `read <filename>` - Read file contents
 * - `delete <filename>` - Delete a file
 * - `format` - Format filesystem
 * - `check` - Check filesystem integrity
 * - `stat` - Show filesystem statistics
 *
 * @param[in] cmd_line Null-terminated command string to parse and execute.
 * @return 0 on success, negative error code on failure.
 */
int tfs_cli_execute(const char *cmd_line);

#ifdef __cplusplus
}
#endif

#endif /* TINYFS_CLI_H */
