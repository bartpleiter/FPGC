#include "bdos.h"

void bdos_shell_trim_whitespace(char *s)
{
  int len;
  int start;
  int i;

  len = strlen(s);
  start = 0;

  while (s[start] == ' ' || s[start] == '\t')
  {
    start++;
  }

  while (len > start && (s[len - 1] == ' ' || s[len - 1] == '\t'))
  {
    len--;
  }

  if (start > 0)
  {
    for (i = 0; i < (len - start); i++)
    {
      s[i] = s[start + i];
    }
  }

  s[len - start] = '\0';
}

int bdos_shell_parse_line(char *line, int *argc_out, char **argv)
{
  int argc;
  char *p;

  argc = 0;
  p = line;

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

int bdos_shell_parse_yes_no(char *value, int *out_yes)
{
  if (strcmp(value, "y") == 0 || strcmp(value, "yes") == 0 || strcmp(value, "1") == 0)
  {
    *out_yes = 1;
    return 0;
  }

  if (strcmp(value, "n") == 0 || strcmp(value, "no") == 0 || strcmp(value, "0") == 0)
  {
    *out_yes = 0;
    return 0;
  }

  return -1;
}

int bdos_shell_path_is_absolute(char *path)
{
  return (path[0] == '/');
}

int bdos_shell_build_absolute_path(char *input_path, char *out_path)
{
  int in_len;
  int cwd_len;

  if (input_path == NULL || out_path == NULL)
  {
    return BRFS_ERR_INVALID_PARAM;
  }

  if (bdos_shell_path_is_absolute(input_path))
  {
    in_len = strlen(input_path);
    if (in_len >= BDOS_SHELL_PATH_MAX)
    {
      return BRFS_ERR_PATH_TOO_LONG;
    }
    strcpy(out_path, input_path);
    return BRFS_OK;
  }

  cwd_len = strlen(bdos_shell_cwd);
  in_len = strlen(input_path);

  if (cwd_len == 1 && bdos_shell_cwd[0] == '/')
  {
    if ((1 + in_len) >= BDOS_SHELL_PATH_MAX)
    {
      return BRFS_ERR_PATH_TOO_LONG;
    }

    out_path[0] = '/';
    out_path[1] = '\0';
    strcat(out_path, input_path);
    return BRFS_OK;
  }

  if ((cwd_len + 1 + in_len) >= BDOS_SHELL_PATH_MAX)
  {
    return BRFS_ERR_PATH_TOO_LONG;
  }

  strcpy(out_path, bdos_shell_cwd);
  strcat(out_path, "/");
  strcat(out_path, input_path);

  return BRFS_OK;
}

int bdos_shell_normalize_path(char *input_path, char *out_path)
{
  char token[BRFS_MAX_FILENAME_LENGTH + 1];
  char components[32][BRFS_MAX_FILENAME_LENGTH + 1];
  int comp_count;
  int in_i;
  int token_len;
  int out_i;
  int j;
  int k;

  if (input_path == NULL || out_path == NULL)
  {
    return BRFS_ERR_INVALID_PARAM;
  }

  comp_count = 0;
  in_i = 0;

  if (input_path[0] == '/')
  {
    in_i = 1;
  }

  while (1)
  {
    while (input_path[in_i] == '/')
    {
      in_i++;
    }

    if (input_path[in_i] == '\0')
    {
      break;
    }

    token_len = 0;
    while (input_path[in_i] != '\0' && input_path[in_i] != '/')
    {
      if (token_len >= BRFS_MAX_FILENAME_LENGTH)
      {
        return BRFS_ERR_NAME_TOO_LONG;
      }
      token[token_len] = input_path[in_i];
      token_len++;
      in_i++;
    }
    token[token_len] = '\0';

    if (strcmp(token, ".") == 0)
    {
      continue;
    }

    if (strcmp(token, "..") == 0)
    {
      if (comp_count > 0)
      {
        comp_count--;
      }
      continue;
    }

    if (comp_count >= 32)
    {
      return BRFS_ERR_PATH_TOO_LONG;
    }

    strcpy(components[comp_count], token);
    comp_count++;
  }

  if (comp_count == 0)
  {
    out_path[0] = '/';
    out_path[1] = '\0';
    return BRFS_OK;
  }

  out_i = 0;
  out_path[out_i++] = '/';

  for (j = 0; j < comp_count; j++)
  {
    if (j > 0)
    {
      out_path[out_i++] = '/';
    }

    for (k = 0; components[j][k] != '\0'; k++)
    {
      if (out_i >= (BDOS_SHELL_PATH_MAX - 1))
      {
        return BRFS_ERR_PATH_TOO_LONG;
      }
      out_path[out_i++] = components[j][k];
    }
  }

  out_path[out_i] = '\0';
  return BRFS_OK;
}

int bdos_shell_resolve_path(char *input_path, char *out_path)
{
  int result;
  char abs_path[BDOS_SHELL_PATH_MAX];

  result = bdos_shell_build_absolute_path(input_path, abs_path);
  if (result != BRFS_OK)
  {
    return result;
  }

  return bdos_shell_normalize_path(abs_path, out_path);
}
