#ifndef MYINTL_H
#define MYINTL_H

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(x) (gettext(x))
#else
#define _(x) (x)
#endif

#define N_(x) (x)

#endif /* MYINTL_H */
