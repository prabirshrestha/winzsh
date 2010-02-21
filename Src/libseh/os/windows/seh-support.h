
/*******************************************************************************
 *                                                                             *
 * seh-support.h - Macros used to implement SEH-like constructs in GNU C/C++   *
 *                                                                             *
 * LIBSEH - Structured Exception Handling compatibility library.               *
 * Copyright (c) 2008 Tom Bramer < tjb at postpro dot net >                    *
 *                                                                             *
 * Permission is hereby granted, free of charge, to any person                 *
 * obtaining a copy of this software and associated documentation              *
 * files (the "Software"), to deal in the Software without                     *
 * restriction, including without limitation the rights to use,                *
 * copy, modify, merge, publish, distribute, sublicense, and/or sell           *
 * copies of the Software, and to permit persons to whom the                   *
 * Software is furnished to do so, subject to the following                    *
 * conditions:                                                                 *
 *                                                                             *
 * The above copyright notice and this permission notice shall be              *
 * included in all copies or substantial portions of the Software.             *
 *                                                                             *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,             *
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES             *
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND                    *
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT                 *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,                *
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING                *
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR               *
 * OTHER DEALINGS IN THE SOFTWARE.                                             *
 *                                                                             *
 *******************************************************************************/

#ifndef __LIBSEH_WINDOWS_SEHSUPPORT_H__
#define __LIBSEH_WINDOWS_SEHSUPPORT_H__

#ifdef __cplusplus
#define DECLLANG extern "C"
#else
#define DECLLANG
#endif

#define SEH_MAGIC_NUMBER 0xDEADBEEF

#if defined(_MSC_VER) || defined(__DIGITALMARS__)
#define HAVE_COMPILER_SEH_SUPPORT
#endif

#if defined(HAVE_COMPILER_SEH_SUPPORT) || defined(__MINGW32__) || \
    defined(__CYGWIN__) || defined(__GNUWIN32__)
#define HAVE_OS_SEH_SUPPORT
#endif

#if defined(__GNUC__) && defined(HAVE_OS_SEH_SUPPORT)
#define HAVE_SEH_WORKAROUND_SUPPORT
#endif

#if !defined(HAVE_COMPILER_SEH_SUPPORT) && !defined(HAVE_SEH_WORKAROUND_SUPPORT)
#warning This compiler and/or operating system does not support structured exception handling, so SEH will be disabled.
#endif

#if defined(__WIN32__) || defined(_WIN32_WINNT) || \
    defined (_WIN32_IE) || defined(_MSC_VER) || \
    defined(__MINGW32__) || defined(__GNUWIN32__) || \
    defined(__CYGWIN__)

#include <windows.h>
#include <excpt.h>

#endif

#ifdef HAVE_SEH_WORKAROUND_SUPPORT

typedef unsigned long __jmp_ctx[6];

struct ___seh_buf;

typedef int (*_PEXCEPTION_HANDLER)
            (struct _EXCEPTION_RECORD*, struct ___seh_buf*, struct _CONTEXT*, struct _EXCEPTION_RECORD*);



typedef struct ___seh_info {
  EXCEPTION_RECORD record;
  CONTEXT context;
  EXCEPTION_RECORD record2;
  EXCEPTION_POINTERS pointers;
} __seh_info;

typedef struct ___seh_buf {
  struct ___seh_buf* prev;
  _PEXCEPTION_HANDLER handler;
  unsigned int magic;
  __jmp_ctx context;
  unsigned int state;
  unsigned int tlsindex;
  __seh_info* excinfo;

} __seh_buf;

/* External prototypes */
DECLLANG int __stdcall __seh_register(__seh_buf* buf);
DECLLANG int __stdcall __seh_restore_context(__seh_buf* buf, int ret);
DECLLANG void __stdcall __seh_unregister();
DECLLANG __seh_buf* __stdcall __seh_get_registration();
DECLLANG void __stdcall __seh_set_registration(__seh_buf* reg);
DECLLANG __seh_buf* __stdcall __seh_pop_registration();

DECLLANG void __seh_init_buf(__seh_buf* buf, __seh_buf* prev);
DECLLANG void __seh_fini_buf(__seh_buf* buf, __seh_buf* prev);

/* Utility functions */
DECLLANG int GetExceptionCode();
DECLLANG LPEXCEPTION_POINTERS GetExceptionInformation();


#define __seh_handler_install(exclabel)       \
  __label__ exclabel ## _filter;              \
  __label__ exclabel ## _cleanup;             \
  __seh_buf exclabel ## _rg;                  \
  int exclabel ## _result = 0;                \
  if(__seh_register(&(exclabel ## _rg)))      \
    goto exclabel ## _filter;                 \


#define __seh_handler_uninstall(exclabel)     \
    __seh_unregister();                       \



#define __seh_handler_filter(exclabel, expr)      \
exclabel ## _filter:;                             \
  if(EXCEPTION_EXECUTE_HANDLER == (expr))         \
    exclabel ## _result = 2;                      \
  else                                            \
    exclabel ## _result = 3;                      \
  goto exclabel ## _finally;                      \



#define __seh_handler_cleanup(exclabel)           \
exclabel ## _cleanup:                             \
  __seh_handler_uninstall(exclabel)               \



#define __seh_handler_begin_except(exclabel, expr)                             \
{                                                                              \
  __label__ exclabel ## _finally;                                              \
  __label__ exclabel ## _except;                                               \
  goto exclabel ## _finally;                                                   \
  __seh_handler_filter(exclabel, expr)                                         \
exclabel ## _finally:;                                                         \
  if(exclabel ## _result == 3) {                                               \
    exclabel ## _rg.state = 1;                                                 \
    exclabel ## _rg.handler(&(exclabel ## _rg.excinfo->record),                \
                            &(exclabel ## _rg),                                \
                            &(exclabel ## _rg.excinfo->context),               \
                            &(exclabel ## _rg.excinfo->record2));              \
  }                                                                            \
  else if(exclabel ## _result == 2) goto exclabel ## _except;                  \
  goto exclabel ## _cleanup;                                                   \
                                                                               \
exclabel ## _except:                                                           \


#define __seh_handler_end_except(exclabel)    \
  __seh_handler_cleanup(exclabel)             \
}

#define __seh_handler_begin_finally(exclabel)                  \
{                                                              \
  __label__ exclabel ## _finally;                              \
  __label__ exclabel ## _except;                               \
  goto exclabel ## _finally;                                   \
  __seh_handler_filter(exclabel, EXCEPTION_CONTINUE_SEARCH)    \
exclabel ## _finally:;                                         \


#define __seh_handler_end_finally(exclabel)                                    \
  if(exclabel ## _result == 3) {                                               \
    exclabel ## _rg.state = 1;                                                 \
    exclabel ## _rg.handler(&(exclabel ## _rg.excinfo->record),                \
                            &(exclabel ## _rg),                                \
                            &(exclabel ## _rg.excinfo->context),               \
                            &(exclabel ## _rg.excinfo->record2));              \
  }                                                                            \
  else if(exclabel ## _result == 2) goto exclabel ## _except;                  \
  goto exclabel ## _cleanup;                                                   \
exclabel ## _except:;                                                          \
  __seh_handler_cleanup(exclabel)                                              \
}

#define __seh_handler_begin_try(exclabel)             \
  __seh_handler_install(exclabel);                    \


#define __seh_handler_end_try(exclabel)               \




#define __try                                         \
do {                                                  \
   __seh_handler_begin_try(__eh_frame)

#define __finally                                     \
   __seh_handler_end_try(__eh_frame)                  \
   __seh_handler_begin_finally(__eh_frame) 

#define __end_finally                                 \
   __seh_handler_end_finally(__eh_frame)              \
} while(0);                                           \

#define __except(filterexpr)                          \
   __seh_handler_end_try(__eh_frame)                  \
   __seh_handler_begin_except(__eh_frame, filterexpr) \

#define __end_except                                  \
   __seh_handler_end_except(__eh_frame)               \
} while(0);                                           \

#else

#define __end_finally
#define __end_except

#ifndef HAVE_COMPILER_SEH_SUPPORT

#define __try if(1)
#define __finally if(1)
#define __except(filterexpr) if(0)

#endif /* HAVE_COMPILER_SEH_SUPPORT */

#endif /* HAVE_SEH_WORKAROUND_SUPPORT */

#endif /* __SEH_H__ */

