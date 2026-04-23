#ifndef BARITE_H
#define BARITE_H

/* install a package from a source */
int barite_install(const char *source, const char *pkg);

/* remove an installed package */
int barite_remove(const char *pkg);

/* list installed packages */
void barite_list(void);

/* show info about a package */
void barite_info(const char *source, const char *pkg);

#endif