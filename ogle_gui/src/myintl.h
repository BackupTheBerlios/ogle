#ifndef MYINTL_H
#define MYINTL_H

#include <libintl.h>
#define _(x) (gettext(x))
#define N_(x) (x)

#endif /* MYINTL_H */
