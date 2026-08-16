#ifndef TINYFS_CLI_H
#define TINYFS_CLI_H

/*
 * TinyFS Command Line Interface.
 * Implements command parsing and output formatting for both the Host test executable
 * and the Arduino Serial monitor console.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Callback definition for printing console outputs */
typedef void (*TFS_Print_Callback)(const char *str);

/*
 * Initialize the CLI with a printing callback function.
 */
void tfs_cli_init(TFS_Print_Callback print_cb);

/*
 * Parses and executes a single command line (e.g. "ls", "write /f1 data").
 * Returns 0 on success, or a negative error code on failure.
 */
int tfs_cli_execute(const char *cmd_line);

#ifdef __cplusplus
}
#endif

#endif /* TINYFS_CLI_H */
