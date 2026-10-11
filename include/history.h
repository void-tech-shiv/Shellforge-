#ifndef SHELLFORGE_HISTORY_H
#define SHELLFORGE_HISTORY_H

void history_init(void);
void history_add_line(const char *line);
int history_print(void);

#endif
