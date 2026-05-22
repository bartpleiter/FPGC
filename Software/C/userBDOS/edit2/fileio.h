#ifndef FILEIO_H
#define FILEIO_H

#include "editor.h"

int file_load(editor_t *ed, const char *path);
int file_save(editor_t *ed, const char *path);

#endif
