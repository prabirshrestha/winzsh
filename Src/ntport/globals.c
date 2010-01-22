/* 
Copyright (c) 1997-2002, 2010, Amol Deshpande and contributors
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of the contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
/*
 globals.c
 The mem locations needed in the child are copied here.
 -amol
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

extern unsigned long bookend1,bookend2;
extern char **environ;

//char ** __saved_environ=0;
#undef dprintf
void
dprintf(char *format, ...)
{				/* } */
	va_list vl;
	char putbuf[1024];
	{
		va_start(vl, format);
		vsprintf(putbuf, format, vl);//melkov@cs.muh.ru
		va_end(vl);
		OutputDebugString(putbuf);
	}
}
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
How To Determine Whether an Application is Console or GUI     [win32sdk]
ID: Q90493     CREATED: 15-OCT-1992   MODIFIED: 16-DEC-1996
*/
#include <winnt.h>
#define xmalloc(s) HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(s))
#define xfree(p) HeapFree(GetProcessHeap(),0,(p))
#define XFER_BUFFER_SIZE 2048

int is_gui(char *exename) {

	HANDLE hImage;

	DWORD  bytes;
	DWORD  SectionOffset;
	DWORD  CoffHeaderOffset;
	DWORD  MoreDosHeader[16];

	ULONG  ntSignature;

	IMAGE_DOS_HEADER      image_dos_header;
	IMAGE_FILE_HEADER     image_file_header;
	IMAGE_OPTIONAL_HEADER image_optional_header;


	hImage = CreateFile(exename, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (INVALID_HANDLE_VALUE == hImage) {
		return 0;
	}

	/*
	 *  Read the MS-DOS image header.
	 */
	if (!ReadFile(hImage, &image_dos_header, sizeof(IMAGE_DOS_HEADER),
			&bytes,NULL)){
		CloseHandle(hImage);
		return 0;
	}

	if (IMAGE_DOS_SIGNATURE != image_dos_header.e_magic) {
		CloseHandle(hImage);
		return 0;
	}

	/*
	 *  Read more MS-DOS header.       */
	if (!ReadFile(hImage, MoreDosHeader, sizeof(MoreDosHeader),
			&bytes,NULL)){
		CloseHandle(hImage);
		return 0;
	}

	/*
	 *  Get actual COFF header.
	 */
	CoffHeaderOffset = SetFilePointer(hImage, image_dos_header.e_lfanew,
			NULL,FILE_BEGIN);

	if (CoffHeaderOffset == (DWORD) -1){
		CloseHandle(hImage);
		return 0;
	}

	CoffHeaderOffset += sizeof(ULONG);

	if (!ReadFile (hImage, &ntSignature, sizeof(ULONG),
			&bytes,NULL)){
		CloseHandle(hImage);
		return 0;
	}

	if (IMAGE_NT_SIGNATURE != ntSignature) {
		CloseHandle(hImage);
		return 0;
	}

	SectionOffset = CoffHeaderOffset + IMAGE_SIZEOF_FILE_HEADER +
		IMAGE_SIZEOF_NT_OPTIONAL_HEADER;

	if (!ReadFile(hImage, &image_file_header, IMAGE_SIZEOF_FILE_HEADER,
			&bytes, NULL)){
		CloseHandle(hImage);
		return 0;
	}

	/*
	 *  Read optional header.
	 */
	if (!ReadFile(hImage, &image_optional_header, 
			IMAGE_SIZEOF_NT_OPTIONAL_HEADER,&bytes,NULL)) {
		CloseHandle(hImage);
		return 0;
	}

	CloseHandle(hImage);

	if (image_optional_header.Subsystem ==IMAGE_SUBSYSTEM_WINDOWS_GUI)
		return 1;
	return 0;
}
int is_9x_gui(char *prog) {
	
	char *progpath;
	DWORD dwret;
	char *pathbuf;
	char *pext;
	
	pathbuf=xmalloc(MAX_PATH);

	progpath=xmalloc(MAX_PATH<<1);

	if (GetEnvironmentVariable("PATH",pathbuf,MAX_PATH) ==0) {
		goto failed;
	}
	
	pathbuf[MAX_PATH]=0;

	dwret = SearchPath(pathbuf,prog,".EXE",MAX_PATH<<1,progpath,&pext);

	if ( (dwret == 0) || (dwret > (MAX_PATH<<1) ) )
		goto failed;
	
	dprintf("progpath is %s\n",progpath);
	dwret = is_gui(progpath);

	xfree(pathbuf);
	xfree(progpath);

	return dwret;

failed:
	xfree(pathbuf);
	xfree(progpath);
	return 0;


}
