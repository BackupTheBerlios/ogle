dnl AC_C_ALWAYS_INLINE
dnl Define inline to something appropriate, including the new always_inline
dnl attribute from gcc 3.1
AC_DEFUN([AC_C_ALWAYS_INLINE],
    [AC_C_INLINE
    if test x"$GCC" = x"yes" -a x"$ac_cv_c_inline" = x"inline"; then
        AC_MSG_CHECKING([for always_inline])
        SAVE_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS -Wall -Werror"
        AC_TRY_COMPILE([],[__attribute__ ((__always_inline__)) void f (void);],
            [ac_cv_always_inline=yes],[ac_cv_always_inline=no])
        CFLAGS="$SAVE_CFLAGS"
        AC_MSG_RESULT([$ac_cv_always_inline])
        if test x"$ac_cv_always_inline" = x"yes"; then
            AC_DEFINE_UNQUOTED([inline],[__attribute__ ((__always_inline__))])
        fi
    fi])

