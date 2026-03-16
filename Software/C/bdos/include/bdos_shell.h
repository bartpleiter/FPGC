/*
 * bdos_shell.h — BDOS interactive shell module.
 *
 * Command line editor, history, built-in commands, and format wizard.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BDOS_SHELL_H
#define BDOS_SHELL_H

/* Shell configuration */
#define BDOS_SHELL_INPUT_MAX   160
#define BDOS_SHELL_ARGV_MAX    8
#define BDOS_SHELL_PROMPT_MAX  192
#define BDOS_SHELL_PATH_MAX    (BRFS_MAX_PATH_LENGTH + 1)

/* Shell state (defined in shell.c / shell_cmds.c) */
extern char bdos_shell_cwd[];
extern unsigned int bdos_shell_start_micros;

/* Program argc/argv (set before bdos_exec_program, readable via syscall) */
extern int bdos_shell_prog_argc;
extern char *bdos_shell_prog_argv[];

/* Shell lifecycle */
void bdos_shell_init(void);
void bdos_shell_tick(void);
void bdos_shell_start_line(void);
void bdos_shell_reset_and_prompt(void);
void bdos_shell_execute_line(char *line);
int bdos_shell_handle_special_mode_line(char *line);
void bdos_shell_on_startup(void);

/* Shell utilities used by other modules */
void bdos_shell_print_fs_error(char *action, int result);

/* Path utilities */
void bdos_shell_trim_whitespace(char *s);
int bdos_shell_parse_line(char *line, int *argc_out, char **argv);
int bdos_shell_parse_yes_no(char *value, int *out_yes);
int bdos_shell_path_is_absolute(char *path);
int bdos_shell_build_absolute_path(char *input_path, char *out_path);
int bdos_shell_normalize_path(char *input_path, char *out_path);
int bdos_shell_resolve_path(char *input_path, char *out_path);
int bdos_shell_resolve_program(char *name, char *out_path);

/* Output utilities */
int bdos_shell_require_fs_ready(void);
void bdos_shell_print_2digit(unsigned int value);
int bdos_shell_u32_to_str(unsigned int value, char *out);
void bdos_shell_print_kib(unsigned int words);
void bdos_shell_print_hline(unsigned int length);
void bdos_shell_print_field_prefix(char *name, int value_col);
int bdos_shell_format_word_size(unsigned int words, char *out);
void bdos_shell_sort_names(char names[][BRFS_MAX_FILENAME_LENGTH + 1], int count);
void bdos_shell_sort_files(char names[][BRFS_MAX_FILENAME_LENGTH + 1],
                           unsigned int *sizes, int count);

/* Format wizard */
void bdos_shell_start_format_wizard(void);

#endif /* BDOS_SHELL_H */
