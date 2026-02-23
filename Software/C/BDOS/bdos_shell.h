#ifndef BDOS_SHELL_H
#define BDOS_SHELL_H

// Shell configuration
#define BDOS_SHELL_INPUT_MAX   160
#define BDOS_SHELL_ARGV_MAX    8
#define BDOS_SHELL_PROMPT_MAX  192
#define BDOS_SHELL_PATH_MAX    (BRFS_MAX_PATH_LENGTH + 1)

// Shell state (defined in shell.c / shell_cmds.c)
extern char bdos_shell_cwd[BDOS_SHELL_PATH_MAX];
extern unsigned int bdos_shell_start_micros;

// Shell lifecycle
void bdos_shell_init();
void bdos_shell_tick();
void bdos_shell_start_line();
void bdos_shell_reset_and_prompt();
void bdos_shell_execute_line(char* line);
int bdos_shell_handle_special_mode_line(char* line);
void bdos_shell_on_startup();

// Shell utilities used by other modules
void bdos_shell_print_fs_error(char* action, int result);

#endif // BDOS_SHELL_H
