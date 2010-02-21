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
 * dirent.c
 * directory interface functions. Sort of like dirent functions on unix.
 * Also allow browsing network shares as if they were directories
 * -amol
 */

#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <direct.h>
#include "dirent.h"
#include <winnetwk.h>

/*XXX: Ignored pragma intrinsic
 *
 * Desc: #pragma intrinsic is ignored by gcc. Apparently what this pragma does
 *       is to force those functions to be called inlined. I haven't found
 *       a similar construct in gcc to force this behaviour. Too bad not to
 *       know the reason for calling those functions inline, but my guess is
 *       that undesirable behaviour may arise from not doing so.
 * Fix:  For the time being we will ignore the pragma. In the future those
 *       functions should be reimplemented as inline functions.
 *
 * - Gabriel de Oliveira -
 */
#ifndef MINGW
# pragma intrinsic("memset")
#endif /* !MINGW */

/*XXX: Extra definitions
 *
 * Desc: We will define attribute MAY_ALIAS for variables that may break
 *       strict-aliasing rules when compiling with -O2 level.
 *
 * - Gabriel de Oliveira -
 */

#ifdef MINGW
# ifndef MAY_ALIAS
#  define MAY_ALIAS __attribute__((may_alias))
# endif /* !MAY_ALIAS */
#else
# ifndef MAY_ALIAS
#  define MAY_ALIAS
# endif /* !MAY_ALIAS */
#endif /* MINGW */

#define xmalloc(a) HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(a))
#define xfree(a) HeapFree(GetProcessHeap(),0,(a))

extern DWORD gdwPlatform;
#define IS_WINDOWS_9x() (gdwPlatform != VER_PLATFORM_WIN32_NT)

HANDLE open_enum(char *,WIN32_FIND_DATA*);
void close_enum(DIR*) ;
int enum_next_share(DIR*);
typedef struct _enum_h {
	unsigned char *netres;
	HANDLE henum;
} nethandle_t;

static int inode= 1; // useless piece that some unix programs need
DIR * opendir(char *buf) {

	DIR *dptr;
	WIN32_FIND_DATA fdata;
	char *tmp ;
	int is_net=0;
	

	if (!buf)
		buf = "." ;
	tmp = buf;
	while(*tmp) {
		if (*tmp == '\\')
			*tmp = '/';
		tmp++;
	}
	tmp= (char *)xmalloc(lstrlen(buf) + 4); 
	memset(tmp,0,lstrlen(buf) +4);
	/*
	 * paths like / confuse NT because it looks like a UNC name
	 * when we append "\*" -amol
	 */
	if ( (buf[0] == '/') && (buf[1] != '/') ) {
		wsprintf(tmp,"%c:%s*",'A' + (_getdrive()-1),buf);
	}
	else if ( (buf[0] == '/')  &&  (buf[1] == '/')  ){
		char *p2,*p3;
		p3 = buf + lstrlen(buf);
		while(*p3 != '/')
			p3--;
		p2 = p3;
		if (p3 != &buf[1])
			p3--;
		while(*p3 != '/')
			p3--;
		if (p3 != &buf[1]){
			is_net = 0;
			wsprintf(tmp,"%s/*",buf);
		}
		else {
			*p3 = *p2;
			*p2=0;
			is_net = 1;
//			p3 = buf;
			wsprintf(tmp,"%s",buf);
			*p2 = *p3;
		}
	}
	else { 
		if (IS_WINDOWS_9x())  {
			if (!lstrcmpi(buf,"."))
				wsprintf(tmp,"%s/*",buf);
			else
				wsprintf(tmp,"%s*",buf);
		}
		else
			wsprintf(tmp,"%s/*",buf);
	}
	
	dptr = (DIR *)xmalloc(sizeof(DIR));
	if (!dptr){
		errno = ENOMEM;
		return NULL;
	}
	
	if (!is_net)
		dptr->dd_fd = FindFirstFile(tmp,&fdata);
	else{
		dptr->dd_fd = open_enum(tmp,&fdata);
		dptr->flags = IS_NET;
	}
	if (dptr->dd_fd == INVALID_HANDLE_VALUE){
		if (GetLastError() == ERROR_DIRECTORY)
			errno = ENOTDIR;
		else
			errno = ENFILE;	
		xfree(dptr);
		return NULL;
	}
	memset(dptr->orig_dir_name,0,sizeof(dptr->orig_dir_name));
	memcpy(dptr->orig_dir_name,tmp,lstrlen(tmp));
	xfree(tmp);

	dptr->dd_loc = 0;
	dptr->dd_size = fdata.nFileSizeLow;
	dptr->dd_buf = (struct dirent *)xmalloc(sizeof(struct dirent));
	if (!dptr->dd_buf){
		xfree(dptr);
		errno = ENOMEM;
		return NULL;
	}
	(dptr->dd_buf)->d_ino = inode++;
	(dptr->dd_buf)->d_off = 0;
	(dptr->dd_buf)->d_reclen = 0;
	if (lstrcmpi(fdata.cFileName,".") ){
		//dptr->dd_buf->d_name[0] = '.';
		memcpy((dptr->dd_buf)->d_name,".",2);
		dptr->flags |= IS_ROOT;
	}
	else
		memcpy((dptr->dd_buf)->d_name,fdata.cFileName,MAX_PATH);
	return dptr;
}
int closedir(DIR *dptr){

	if (!dptr)
		return 0;
	if (dptr->flags & IS_NET) {
		close_enum(dptr);
		return 0;
	}
	FindClose(dptr->dd_fd);
	xfree(dptr->dd_buf);
	xfree(dptr);
	return 0;
}
void rewinddir(DIR *dptr) {

	HANDLE hfind;
	WIN32_FIND_DATA fdata;
	char *tmp = dptr->orig_dir_name;

	if (!dptr) return;

	if (dptr->flags & IS_NET) {
		hfind = open_enum(tmp,&fdata);
		close_enum(dptr->dd_fd);
		dptr->dd_fd = hfind;
	}
	else {
		hfind = FindFirstFile(tmp,&fdata);
		assert(hfind != INVALID_HANDLE_VALUE);
		FindClose(dptr->dd_fd);
		dptr->dd_fd = hfind;
	}
	dptr->dd_size = fdata.nFileSizeLow;
	(dptr->dd_buf)->d_ino = inode++;
	(dptr->dd_buf)->d_off = 0;
	(dptr->dd_buf)->d_reclen = 0;
	memcpy((dptr->dd_buf)->d_name,fdata.cFileName,MAX_PATH);
	return;
}
struct dirent *readdir(DIR *dir) {

	WIN32_FIND_DATA fdata;
	HANDLE hfind;
	char *tmp ;

	if (!dir)
		return NULL;

		// special hack for root (which does not have . or ..)
	if (dir->flags & IS_NET) {
		if(enum_next_share(dir)<0)
			return NULL;
	}
	else if (dir->flags & IS_ROOT) {
		tmp= dir->orig_dir_name;
		hfind = FindFirstFile(tmp,&fdata);
		FindClose(dir->dd_fd);
		dir->dd_fd = hfind;
		dir->dd_size = fdata.nFileSizeLow;
		(dir->dd_buf)->d_ino = inode++;
		(dir->dd_buf)->d_off = 0;
		(dir->dd_buf)->d_reclen = 0;
		memcpy((dir->dd_buf)->d_name,fdata.cFileName,MAX_PATH);
		dir->flags &= ~IS_ROOT;
		return dir->dd_buf;

	}
	if(!(dir->flags & IS_NET) && !FindNextFile(dir->dd_fd,&fdata) ){
		return NULL;
	}
	(dir->dd_buf)->d_ino = inode++;
	(dir->dd_buf)->d_off = 0;
	(dir->dd_buf)->d_reclen = 0;
	if (! (dir->flags & IS_NET))
		memcpy((dir->dd_buf)->d_name,fdata.cFileName,MAX_PATH);

	return dir->dd_buf;

}

// Support for treating share names as directories
// -amol 5/28/97
static int ginited = 0;
static HANDLE hmpr;

typedef DWORD (__stdcall *open_fn)(DWORD,DWORD,DWORD,NETRESOURCE *, HANDLE*);
typedef DWORD (__stdcall *close_fn)( HANDLE);
typedef DWORD (__stdcall *enum_fn)( HANDLE,DWORD * ,void *,DWORD*);


static open_fn p_WNetOpenEnum;
static close_fn p_WNetCloseEnum;
static enum_fn  p_WNetEnumResource;

HANDLE open_enum(char *server, WIN32_FIND_DATA *fdata) {

	NETRESOURCE netres;
	HANDLE henum;
	unsigned long ret;

	nethandle_t *hnet;

	if (!ginited) {
		hmpr = LoadLibrary("MPR.DLL");
		if (!hmpr)
			return INVALID_HANDLE_VALUE;

		p_WNetOpenEnum = (open_fn)GetProcAddress(hmpr,"WNetOpenEnumA");
		p_WNetCloseEnum = (close_fn)GetProcAddress(hmpr,"WNetCloseEnum");
		p_WNetEnumResource = (enum_fn)GetProcAddress(hmpr,"WNetEnumResourceA");

		if (!p_WNetOpenEnum || !p_WNetCloseEnum || !p_WNetEnumResource)
			return INVALID_HANDLE_VALUE;
		ginited = 1;
	}
	server[0] = '\\';
	server[1] = '\\';

	memset(fdata,0,sizeof(WIN32_FIND_DATA));
	fdata->cFileName[0] = '.';

	netres.dwScope = RESOURCE_GLOBALNET;
	netres.dwType = RESOURCETYPE_ANY;
	netres.lpRemoteName = server;
	netres.lpProvider = NULL;
	netres.dwUsage = 0;

	ret = p_WNetOpenEnum(RESOURCE_GLOBALNET,RESOURCETYPE_ANY,0,
							&netres,&henum);
	if (ret != NO_ERROR)
		return INVALID_HANDLE_VALUE;
	
	hnet = xmalloc(sizeof(nethandle_t));
	hnet->netres = xmalloc(1024);
	hnet->henum = henum;


	return (HANDLE)hnet;

}
void close_enum(DIR*dptr) {
	nethandle_t *hnet;

	hnet = (nethandle_t*)(dptr->dd_fd);

	xfree(hnet->netres);
	p_WNetCloseEnum(hnet->henum);
	xfree(hnet);
}
int enum_next_share(DIR *dir) {
	nethandle_t *hnet;
	char *tmp,*p1;
	HANDLE henum;
	int MAY_ALIAS count;
        int MAY_ALIAS breq;
        int ret;

	hnet = (nethandle_t*)(dir->dd_fd);
	henum = hnet->henum;
	count =  1;
	breq = 1024;

	ret = p_WNetEnumResource(henum,
                                 (unsigned long *) &count,
                                 hnet->netres,
                                 (unsigned long *) &breq);
	if (ret != NO_ERROR)
		return -1;
	
	tmp = ((NETRESOURCE*)hnet->netres)->lpRemoteName;
	p1 = &tmp[2];
	while(*p1++ != '\\');

	memcpy( (dir->dd_buf)->d_name, p1, lstrlen(p1)+1);

	dir->dd_size = 0;

	return 0;
}
