#ifndef SHELLFORGE_ALIAS_H
#define SHELLFORGE_ALIAS_H

const char *alias_resolve(const char *name);
const char *builtin_canonical_name(const char *name);
void print_aliases(void);

#endif
