#ifndef BDOS_SHELL_H
#define BDOS_SHELL_H

/* ============================================================
 * BDOS shell v2 \u2014 public API.
 *
 * Phase D of shell-terminal-v2:
 *   - tokenizer with quoting/escapes (shell_lex.c)
 *   - variable table with $VAR / ${VAR} expansion (shell_vars.c)
 *   - parser that builds a chain-of-pipelines AST (shell_parse.c)
 *   - executor that walks the AST, handling redirects, pipes,
 *     chains and built-ins (shell_exec.c)
 *   - in-kernel #!/bin/sh interpreter (shell_script.c)
 *
 * The line-edit / prompt / history UI lives in shell.c.
 * Path utilities and the format wizard live in shell_path.c and
 * shell_format.c respectively (unchanged from v1).
 * ============================================================
 */

/* ---- Tunables ---- */

#define BDOS_SHELL_INPUT_MAX   160
#define BDOS_SHELL_ARGV_MAX    8
#define BDOS_SHELL_PIPE_MAX    4    /* commands per pipeline */
#define BDOS_SHELL_CHAIN_MAX   8    /* pipelines per chain   */
#define BDOS_SHELL_TOK_MAX     64   /* tokens per line       */
#define BDOS_SHELL_PROMPT_MAX  192
#define BDOS_SHELL_PATH_MAX    (BRFS_MAX_PATH_LENGTH + 1)
#define BDOS_SHELL_VAR_MAX     32
#define BDOS_SHELL_VAR_NAME    16
#define BDOS_SHELL_VAR_VALUE   96

/* ---- Shell state ---- */

extern char         bdos_shell_cwd[];
extern unsigned int bdos_shell_start_micros;
extern int          bdos_shell_last_exit;   /* $? */

/* Legacy globals \u2014 still mirrored by proc_spawn for syscall compat. */
extern int   bdos_shell_prog_argc;
extern char *bdos_shell_prog_argv[];

/* ---- Lifecycle ---- */

void bdos_shell_init(void);
void bdos_shell_tick(void);
void bdos_shell_start_line(void);
void bdos_shell_reset_and_prompt(void);
void bdos_shell_on_startup(void);

/* Top-level command processor. Returns the chain's exit code. */
int  bdos_shell_execute_line(char *line);

/* Special-mode hook (format wizard etc.). Returns 1 if consumed. */
int  bdos_shell_handle_special_mode_line(char *line);

/* ---- Errors / output helpers ---- */

void bdos_shell_print_fs_error(char *action, int result);
int  bdos_shell_require_fs_ready(void);

/* ---- Path utilities (shell_path.c) ---- */

void bdos_shell_trim_whitespace(char *s);
int  bdos_shell_parse_yes_no(char *value, int *out_yes);
int  bdos_shell_path_is_absolute(char *path);
int  bdos_shell_build_absolute_path(char *input_path, char *out_path);
int  bdos_shell_normalize_path(char *input_path, char *out_path);
int  bdos_shell_resolve_path(char *input_path, char *out_path);

/* Resolve a program name against $PATH. */
int  bdos_shell_resolve_program(char *name, char *out_path);

/* ---- Output utilities (shell_util.c) ---- */

void bdos_shell_print_2digit(unsigned int value);
int  bdos_shell_u32_to_str(unsigned int value, char *out);
void bdos_shell_print_kib(unsigned int bytes);
void bdos_shell_print_hline(unsigned int length);
void bdos_shell_print_field_prefix(char *name, int value_col);
int  bdos_shell_format_byte_size(unsigned int bytes, char *out);
void bdos_shell_sort_names(char names[][BRFS_MAX_FILENAME_LENGTH + 1], int count);
void bdos_shell_sort_files(char names[][BRFS_MAX_FILENAME_LENGTH + 1],
                           unsigned int *sizes, int count);

/* ---- Format wizard (shell_format.c) ---- */
void bdos_shell_start_format_wizard(void);

/* ---- Built-in commands (implementations in shell_cmds.c) ---- */
int bi_help    (int argc, char **argv);
int bi_clear   (int argc, char **argv);
int bi_echo    (int argc, char **argv);
int bi_uptime  (int argc, char **argv);
int bi_pwd     (int argc, char **argv);
int bi_cd      (int argc, char **argv);
int bi_ls      (int argc, char **argv);
int bi_mkdir   (int argc, char **argv);
int bi_mkfile  (int argc, char **argv);
int bi_rm      (int argc, char **argv);
int bi_cat     (int argc, char **argv);
int bi_write   (int argc, char **argv);
int bi_cp      (int argc, char **argv);
int bi_mv      (int argc, char **argv);
int bi_sync    (int argc, char **argv);
int bi_df      (int argc, char **argv);
int bi_jobs    (int argc, char **argv);
int bi_kill    (int argc, char **argv);
int bi_fg      (int argc, char **argv);
int bi_export  (int argc, char **argv);
int bi_set     (int argc, char **argv);
int bi_unset   (int argc, char **argv);
int bi_env     (int argc, char **argv);
int bi_exit    (int argc, char **argv);
int bi_true    (int argc, char **argv);
int bi_false   (int argc, char **argv);

/* ---- Variables (shell_vars.c) ---- */

void        bdos_shell_vars_init(void);
const char *bdos_shell_var_get(const char *name);
int         bdos_shell_var_set(const char *name, const char *value);
int         bdos_shell_var_export(const char *name);
int         bdos_shell_var_unset(const char *name);
void        bdos_shell_vars_foreach(int exported_only,
                                    void (*cb)(const char *name, const char *value, int exported));
void        bdos_shell_vars_print(int exported_only);

/*
 * Expand $VAR / ${VAR} / $? / $$ / $0..$9 / $# in src into dst.
 * Single-quoted segments are passed through verbatim; double-quoted
 * segments expand. Returns 0 on success, -1 if dst overflows.
 */
int bdos_shell_expand(const char *src, char *dst, int dst_size,
                      int script_argc, char **script_argv);

/* ---- Lexer (shell_lex.c) ---- */

#define SH_TOK_END          0
#define SH_TOK_WORD         1
#define SH_TOK_REDIR_IN     2   /* < */
#define SH_TOK_REDIR_OUT    3   /* > */
#define SH_TOK_REDIR_APPEND 4   /* >> */
#define SH_TOK_PIPE         5   /* | */
#define SH_TOK_AND          6   /* && */
#define SH_TOK_OR           7   /* || */
#define SH_TOK_SEMI         8   /* ; */

typedef struct {
    int   type;
    char *text;   /* points into store buffer for WORDs, else NULL */
} sh_tok_t;

int bdos_shell_lex(const char *line, sh_tok_t *out_toks, int max_toks,
                   char *store, int store_size);

/* ---- Parser (shell_parse.c) ---- */

typedef struct {
    int   argc;
    char *argv[BDOS_SHELL_ARGV_MAX];
    char *redir_in;     /* NULL if absent */
    char *redir_out;
    char *redir_append;
} sh_cmd_t;

typedef struct {
    int      n_cmds;
    sh_cmd_t cmds[BDOS_SHELL_PIPE_MAX];
} sh_pipeline_t;

#define SH_OP_END   0
#define SH_OP_AND   1
#define SH_OP_OR    2
#define SH_OP_SEMI  3

typedef struct {
    int           n_pipes;
    sh_pipeline_t pipes[BDOS_SHELL_CHAIN_MAX];
    int           ops[BDOS_SHELL_CHAIN_MAX];   /* op AFTER pipes[i] */
} sh_chain_t;

int bdos_shell_parse(sh_tok_t *toks, sh_chain_t *out);

/* ---- Executor (shell_exec.c) ---- */

int bdos_shell_run_cmd(int argc, char **argv);
int bdos_shell_run_chain(sh_chain_t *chain);

/* ---- Script interpreter (shell_script.c) ---- */
int bdos_shell_run_script(const char *path, int script_argc, char **script_argv);

#endif /* BDOS_SHELL_H */
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
void bdos_shell_print_kib(unsigned int bytes);
void bdos_shell_print_hline(unsigned int length);
void bdos_shell_print_field_prefix(char *name, int value_col);
int bdos_shell_format_byte_size(unsigned int bytes, char *out);
void bdos_shell_sort_names(char names[][BRFS_MAX_FILENAME_LENGTH + 1], int count);
void bdos_shell_sort_files(char names[][BRFS_MAX_FILENAME_LENGTH + 1],
                           unsigned int *sizes, int count);

/* Format wizard */
void bdos_shell_start_format_wizard(void);

#endif /* BDOS_SHELL_H */
