/* 
 * Copyright (c) 1997-2002, 2010, Amol Deshpande and contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *     * Neither the name of the author nor the names of the contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
/*
 * globals.c
 * The mem locations needed in the child are copied here.
 * -amol
 */

#include "ntport.h"

extern unsigned long bookend1,bookend2;
extern char **environ;

#define IMAGE_SIZEOF_NT_OPTIONAL32_HEADER    224
#define IMAGE_SIZEOF_NT_OPTIONAL64_HEADER    240

#ifdef _WIN64
#define IMAGE_SIZEOF_NT_OPTIONAL_HEADER     IMAGE_SIZEOF_NT_OPTIONAL64_HEADER
#else
#define IMAGE_SIZEOF_NT_OPTIONAL_HEADER     IMAGE_SIZEOF_NT_OPTIONAL32_HEADER
#endif


//char ** __saved_environ=0;
#ifdef NTDBG
#undef dprintf
void
dprintf(char *format, ...)
{				/* } */
	va_list vl;
	char putbuf[2048];
	{
		va_start(vl, format);
		vsprintf(putbuf, format, vl);//melkov@cs.muh.ru
		va_end(vl);
		OutputDebugString(putbuf);
	}
}
#endif

int fork_copy_user_mem(HANDLE hproc) {
	
	int bytes,rc;
	int size;

	size =(char*)&bookend2 - (char*)&bookend1;
	//dprintf("hproc 0x%08x, size %u\n",hproc,size);
	rc =WriteProcessMemory(hproc,&bookend1,&bookend1,
					size,
					&bytes);

	if (!rc) {
		__asm { int 3 };
		rc = GetLastError();
		return -1;
	}
	if (size != bytes) {
		dprintf("size %d , wrote %d\n",size,bytes);
	}
	/*
	__saved_environ=environ;
	rc =WriteProcessMemory(hproc,&__saved_environ,&__saved_environ,4, &bytes);

	if (!rc) {
		rc = GetLastError();
		return -1;
	}
	if (4 != bytes) {
		dprintf("size %d , wrote %d\n",size,bytes);
	}
	*/
	return 0;
}
/*
 * Inspired by Microsoft KB article ID: Q90493 
 *
 * returns 0 (false) if app is non-gui, 1 otherwise.
*/
#include <winnt.h>
#include <ntport.h>

__inline BOOL wait_for_io(HANDLE hi, OVERLAPPED *pO) {

        DWORD bytes = 0;
        if(GetLastError() != ERROR_IO_PENDING)
        {
                return FALSE;
        }

        return GetOverlappedResult(hi,pO,&bytes,TRUE);
}
#define CHECK_IO(h,o)  if(!wait_for_io(h,o)) {goto done;}

int is_gui(char *exename) {

        HANDLE hImage;

        DWORD  bytes;
        OVERLAPPED overlap;

        ULONG  ntSignature;

        struct DosHeader{
                IMAGE_DOS_HEADER     doshdr;
                DWORD                extra[16];
        };

        struct DosHeader dh;
        IMAGE_OPTIONAL_HEADER optionalhdr;

        int retCode = 0;

        memset(&overlap,0,sizeof(overlap));


        hImage = CreateFile(exename, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL| FILE_FLAG_OVERLAPPED, NULL);
        if (INVALID_HANDLE_VALUE == hImage) {
                return 0;
        }

        ReadFile(hImage, &dh, sizeof(struct DosHeader), &bytes,&overlap);
        CHECK_IO(hImage,&overlap);


        if (IMAGE_DOS_SIGNATURE != dh.doshdr.e_magic) {
                goto done;
        }

        // read from the coffheaderoffset;
        overlap.Offset = dh.doshdr.e_lfanew;

        ReadFile(hImage, &ntSignature, sizeof(ULONG), &bytes,&overlap);
        CHECK_IO(hImage,&overlap);

        if (IMAGE_NT_SIGNATURE != ntSignature) {
                goto done;
        }
        overlap.Offset = dh.doshdr.e_lfanew + sizeof(ULONG) +
                sizeof(IMAGE_FILE_HEADER);

        ReadFile(hImage, &optionalhdr,IMAGE_SIZEOF_NT_OPTIONAL_HEADER, &bytes,&overlap);
        CHECK_IO(hImage,&overlap);

        if (optionalhdr.Subsystem ==IMAGE_SUBSYSTEM_WINDOWS_GUI)
                retCode =  1;
done:
        CloseHandle(hImage);
        return retCode;
}
int is_9x_gui(char *prog) {
	
	char *progpath;
	DWORD dwret;
	char *pathbuf;
	char *pext;
	
	pathbuf=heap_alloc(MAX_PATH+1);
	if(!pathbuf)
		return 0;

	progpath=heap_alloc((MAX_PATH<<1)+1);
	if(!progpath)
		return 0;

	if (GetEnvironmentVariable("PATH",pathbuf,MAX_PATH) ==0) {
		goto failed;
	}
	
	pathbuf[MAX_PATH]=0;

	dwret = SearchPath(pathbuf,prog,".EXE",MAX_PATH<<1,progpath,&pext);

	if ( (dwret == 0) || (dwret > (MAX_PATH<<1) ) )
		goto failed;
	
	dprintf("progpath is %s\n",progpath);
	dwret = is_gui(progpath);

	heap_free(pathbuf);
	heap_free(progpath);

	return dwret;

failed:
	heap_free(pathbuf);
	heap_free(progpath);
	return 0;


}
