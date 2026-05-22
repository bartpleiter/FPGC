#ifndef RENDER_H
#define RENDER_H

#include "editor.h"

void render_init(void);
void render_all(editor_t *ed);
void render_refresh(void);
void render_status_dirty(void);
void render_show_save_status(int success);
int  render_confirm(char *prompt);

#endif
