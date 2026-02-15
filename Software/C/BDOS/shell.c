/*
 * BDOS shell module.
 * Provides a simple interactive command shell using keyboard FIFO events.
 */

#include "BDOS/bdos.h"

char bdos_shell_input[BDOS_SHELL_INPUT_MAX];
int bdos_shell_input_len;
int bdos_shell_input_overflow;
char bdos_shell_prompt[BDOS_SHELL_PROMPT_MAX];
char bdos_shell_cwd[24] = "root";
unsigned int bdos_shell_prompt_x;
unsigned int bdos_shell_prompt_y;
int bdos_shell_last_render_len;
unsigned int bdos_shell_start_micros;

void bdos_shell_build_prompt()
{
  bdos_shell_prompt[0] = '\0';
  strcat(bdos_shell_prompt, "[");
  strcat(bdos_shell_prompt, bdos_shell_cwd);
  strcat(bdos_shell_prompt, "] bdos> ");
}

int bdos_shell_prompt_len()
{
  return strlen(bdos_shell_prompt);
}

void bdos_shell_draw_cursor()
{
  unsigned int x;
  unsigned int y;

  term_get_cursor(&x, &y);
  gpu_write_window_tile(x, y, ' ', PALETTE_BLACK_ON_WHITE);
}

void bdos_shell_clear_cursor()
{
  unsigned int x;
  unsigned int y;

  term_get_cursor(&x, &y);
  gpu_write_window_tile(x, y, ' ', PALETTE_WHITE_ON_BLACK);
}

void bdos_shell_render_line()
{
  int i;

  term_set_palette(PALETTE_WHITE_ON_BLACK);
  term_set_cursor(bdos_shell_prompt_x, bdos_shell_prompt_y);
  for (i = 0; i < bdos_shell_last_render_len; i++)
  {
    term_putchar(' ');
  }

  term_set_cursor(bdos_shell_prompt_x, bdos_shell_prompt_y);
  term_puts(bdos_shell_prompt);
  term_write(bdos_shell_input, bdos_shell_input_len);
  bdos_shell_last_render_len = bdos_shell_prompt_len() + bdos_shell_input_len;
  bdos_shell_draw_cursor();
}

void bdos_shell_start_line()
{
  bdos_shell_build_prompt();
  term_get_cursor(&bdos_shell_prompt_x, &bdos_shell_prompt_y);
  bdos_shell_last_render_len = 0;
  bdos_shell_render_line();
}

void bdos_shell_print_welcome()
{
  term_puts(" ____  ____   ___   ____\n");
  term_puts("| __ )|  _ \\ / _ \\ / ___|\n");
  term_puts("|  _ \\| |_| | |_| |\\___ \\\n");
  term_puts("|____/|____/ \\___/ |____/v2.0-dev1\n\n");
}

int bdos_shell_parse_line(char* line, int* argc_out, char** argv)
{
  int argc = 0;
  char* p = line;

  while (*p != '\0')
  {
    while (*p == ' ' || *p == '\t')
    {
      p++;
    }

    if (*p == '\0')
    {
      break;
    }

    if (argc >= BDOS_SHELL_ARGV_MAX)
    {
      return -1;
    }

    argv[argc] = p;
    argc++;

    while (*p != '\0' && *p != ' ' && *p != '\t')
    {
      p++;
    }

    if (*p == '\0')
    {
      break;
    }

    *p = '\0';
    p++;
  }

  *argc_out = argc;
  return 0;
}

int bdos_shell_cmd_help(int argc, char** argv);
int bdos_shell_cmd_clear(int argc, char** argv);
int bdos_shell_cmd_echo(int argc, char** argv);
int bdos_shell_cmd_version(int argc, char** argv);
int bdos_shell_cmd_uptime(int argc, char** argv);
int bdos_shell_cmd_pwd(int argc, char** argv);
int bdos_shell_cmd_ls(int argc, char** argv);

int bdos_shell_cmd_help(int argc, char** argv)
{
  term_puts("Commands:\n");
  term_puts("  help - List available commands\n");
  term_puts("  clear - Clear the terminal\n");
  term_puts("  echo - Echo arguments\n");
  term_puts("  version - Show BDOS shell version\n");
  term_puts("  uptime - Show shell uptime in ms\n");
  term_puts("  pwd - Print current directory\n");
  term_puts("  ls - List directory (placeholder)\n");

  return 0;
}

int bdos_shell_cmd_clear(int argc, char** argv)
{
  term_clear();
  return 0;
}

int bdos_shell_cmd_echo(int argc, char** argv)
{
  int i;

  for (i = 1; i < argc; i++)
  {
    term_puts(argv[i]);
    if (i < argc - 1)
    {
      term_putchar(' ');
    }
  }
  term_putchar('\n');

  return 0;
}

int bdos_shell_cmd_version(int argc, char** argv)
{
  term_puts("BDOS v2.0-dev1\n");
  return 0;
}

int bdos_shell_cmd_uptime(int argc, char** argv)
{
  unsigned int elapsed_us;
  unsigned int elapsed_ms;

  elapsed_us = get_micros() - bdos_shell_start_micros;
  elapsed_ms = elapsed_us / 1000;

  term_puts("Uptime: ");
  term_putint((int)elapsed_ms);
  term_puts(" ms\n");
  return 0;
}

int bdos_shell_cmd_pwd(int argc, char** argv)
{
  term_puts("/");
  term_puts(bdos_shell_cwd);
  term_putchar('\n');
  return 0;
}

int bdos_shell_cmd_ls(int argc, char** argv)
{
  term_puts("error: filesystem not implemented yet\n");
  return 0;
}

void bdos_shell_execute_line(char* line)
{
  int argc;
  char* argv[BDOS_SHELL_ARGV_MAX];

  if (bdos_shell_parse_line(line, &argc, argv) != 0)
  {
    term_puts("error: too many arguments\n");
    return;
  }

  if (argc == 0)
  {
    return;
  }

  if (strcmp(argv[0], "help") == 0)
  {
    bdos_shell_cmd_help(argc, argv);
    return;
  }

  if (strcmp(argv[0], "clear") == 0)
  {
    bdos_shell_cmd_clear(argc, argv);
    return;
  }

  if (strcmp(argv[0], "echo") == 0)
  {
    bdos_shell_cmd_echo(argc, argv);
    return;
  }

  if (strcmp(argv[0], "version") == 0)
  {
    bdos_shell_cmd_version(argc, argv);
    return;
  }

  if (strcmp(argv[0], "uptime") == 0)
  {
    bdos_shell_cmd_uptime(argc, argv);
    return;
  }

  if (strcmp(argv[0], "pwd") == 0)
  {
    bdos_shell_cmd_pwd(argc, argv);
    return;
  }

  if (strcmp(argv[0], "ls") == 0)
  {
    bdos_shell_cmd_ls(argc, argv);
    return;
  }

  term_puts("error: unknown command: ");
  term_puts(argv[0]);
  term_puts("\nType 'help' to list commands.\n");
}

void bdos_shell_submit_input()
{
  bdos_shell_clear_cursor();
  term_putchar('\n');

  if (bdos_shell_input_overflow)
  {
    term_puts("error: input too long\n");
  }
  else
  {
    bdos_shell_input[bdos_shell_input_len] = '\0';
    bdos_shell_execute_line(bdos_shell_input);
  }

  bdos_shell_input_len = 0;
  bdos_shell_input[0] = '\0';
  bdos_shell_input_overflow = 0;
  bdos_shell_start_line();
}

void bdos_shell_handle_key_event(int key_event)
{
  if (key_event == '\n' || key_event == '\r')
  {
    bdos_shell_submit_input();
    return;
  }

  if (key_event == '\b' || key_event == 127)
  {
    if (bdos_shell_input_len > 0)
    {
      bdos_shell_input_len--;
      bdos_shell_input[bdos_shell_input_len] = '\0';
      bdos_shell_input_overflow = 0;
      bdos_shell_render_line();
    }
    return;
  }

  if (key_event == 3)
  {
    bdos_shell_input_len = 0;
    bdos_shell_input[0] = '\0';
    bdos_shell_input_overflow = 0;
    bdos_shell_render_line();
    return;
  }

  if (key_event == 12)
  {
    term_clear();
    bdos_shell_input_len = 0;
    bdos_shell_input[0] = '\0';
    bdos_shell_input_overflow = 0;
    bdos_shell_start_line();
    return;
  }

  if (key_event >= 32 && key_event <= 126)
  {
    if (bdos_shell_input_len < (BDOS_SHELL_INPUT_MAX - 1))
    {
      bdos_shell_input[bdos_shell_input_len] = (char)key_event;
      bdos_shell_input_len++;
      bdos_shell_input[bdos_shell_input_len] = '\0';
      bdos_shell_render_line();
    }
    else
    {
      bdos_shell_input_overflow = 1;
    }
    return;
  }
}

void bdos_shell_init()
{
  term_set_palette(PALETTE_WHITE_ON_BLACK);
  term_clear();

  bdos_shell_input_len = 0;
  bdos_shell_input[0] = '\0';
  bdos_shell_input_overflow = 0;
  bdos_shell_last_render_len = 0;

  bdos_shell_start_micros = get_micros();

  bdos_shell_print_welcome();
  bdos_shell_start_line();
}

void bdos_shell_tick()
{
  while (bdos_keyboard_event_available())
  {
    int key_event = bdos_keyboard_event_read();
    if (key_event != -1)
    {
      bdos_shell_handle_key_event(key_event);
    }
  }
}
