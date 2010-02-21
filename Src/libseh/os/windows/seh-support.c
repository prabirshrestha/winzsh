
/*******************************************************************************
 *                                                                             *
 * seh-suppoprt.c - Functions used to implement SEH support at runtime.        *
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



#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "seh-support.h"
#include "../../common/stddefs.h"

#ifdef HAVE_SEH_WORKAROUND_SUPPORT

DECLLANG
int __seh_exception_handler(PEXCEPTION_RECORD pRecord,
                            __seh_buf* pReg,
                            PCONTEXT pContext,
                            PEXCEPTION_RECORD pRecord2)
{
  TRACE_START();
  _PEXCEPTION_HANDLER me = pReg->handler;
  __seh_buf* pOrgReg = pReg;
  __seh_info* info = pReg->excinfo;
  unsigned long tlsindex = pReg->tlsindex;

  while (pOrgReg != __seh_get_registration())
    pReg = __seh_pop_registration();

  if (pReg->state == 2)
    pReg = __seh_pop_registration();

  VERIFY(pReg != NULL && pReg->state != 2, 
         "There should never be more than one registration in execution state.");

  TRACE1("Value of pReg = 0x%08x\n", pReg);
  TRACE1("Value of %%fs = 0x%08x\n", __seh_get_registration());
  TRACE1("Previous handler: 0x%08x\n", __seh_get_registration()->prev);
  TRACE1("Handler function: 0x%08x\n", __seh_get_registration()->handler);
  TRACE1("Magic number: 0x%08x\n", __seh_get_registration()->magic);
  TRACE1("Exception code: 0x%08x\n", pRecord->ExceptionCode);
  TRACE1("Exception address: 0x%08x\n", pRecord->ExceptionAddress);


  if (pReg->excinfo == NULL)
    info = pReg->excinfo = (__seh_info*)TlsGetValue(pReg->tlsindex);
  if (pReg->excinfo == NULL) {
    info = pReg->excinfo = (__seh_info*)malloc(sizeof(__seh_info));

    TlsSetValue(pReg->tlsindex, info);
    memcpy(&info->record, pRecord, sizeof(EXCEPTION_RECORD));
    memcpy(&info->record2, pRecord2, sizeof(EXCEPTION_RECORD));
    memcpy(&info->context, pContext, sizeof(CONTEXT));
    info->pointers.ContextRecord = &(info->context);
    info->pointers.ExceptionRecord = &(info->record);
  }

  while (NULL != pReg) {
    if (pReg->handler == me && pReg->magic == SEH_MAGIC_NUMBER) {
      pReg->excinfo = info;
      if (0 == pReg->state) {
        TRACE0("Jumping down to exception handling block.\n");
        pReg->state = 2;
        __seh_restore_context(pReg, 1);
      }
    } else {
      TRACE0("Executing other exception handler.\n");
      free(info);
      TlsFree(tlsindex);
      pReg->handler(pRecord, pReg, pContext, pRecord2);
    }
    TRACE0("Popping registration.\n");
    pReg = __seh_pop_registration();
  }

  TRACE_END();

  return EXCEPTION_CONTINUE_SEARCH;
}

DECLLANG
int GetExceptionCode()
{
  TRACE_START();
  __seh_buf* pReg = __seh_get_registration();

  if (pReg == NULL || pReg->magic != SEH_MAGIC_NUMBER || pReg->excinfo == NULL) {
    return 0;
  }

  TRACE1("Exception code: 0x%08x\n", pReg->excinfo->record.ExceptionCode);

  TRACE_END();
  return pReg->excinfo->record.ExceptionCode;
}

DECLLANG
LPEXCEPTION_POINTERS GetExceptionInformation()
{
  TRACE_START();
  __seh_buf* pReg = __seh_get_registration();
  if (pReg == NULL || pReg->magic != SEH_MAGIC_NUMBER || pReg->excinfo == NULL) {
    return 0;
  }

  TRACE_END();
  return &(pReg->excinfo->pointers);
}

DECLLANG 
void __seh_init_buf(__seh_buf* buf, __seh_buf* prev)
{
  TRACE_START();
  buf->magic = SEH_MAGIC_NUMBER;
  buf->state = 0;
  if (prev == NULL || prev->magic != SEH_MAGIC_NUMBER) {
    buf->tlsindex = TlsAlloc();
    TlsSetValue(buf->tlsindex, NULL);
  } else
    buf->tlsindex = prev->tlsindex;

  buf->excinfo = NULL;
  TRACE_END();
}

DECLLANG 
void __seh_fini_buf(__seh_buf* buf, __seh_buf* prev)
{
  TRACE_START();
  if (buf->excinfo != NULL) {
    free(buf->excinfo);
    buf->excinfo = NULL;
  }

  if (prev == NULL || prev->magic != SEH_MAGIC_NUMBER) {
    TlsFree(buf->tlsindex);
  }


  buf->magic = 0x0;
  TRACE_END();
}

#endif
