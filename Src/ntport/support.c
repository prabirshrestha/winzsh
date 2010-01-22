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
 support.c
 various routines to do exec, etc.
 -amol
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <errno.h>
#include <fcntl.h>
#include "ntport.h"
#include "zsh.h"

extern void init_stdio(void);
extern void nt_term_init(void);
extern void nt_init_signals(void);
extern void init_exceptions(void);

DWORD gdwPlatform = 0;
OSVERSIONINFO gosver;
int gisWin95; // if detection fails

extern unsigned long __forked;

void nt_init(void) {

	int rc;
	char ptr[MAX_PATH];
	char hdrive[MAX_PATH],hpath[MAX_PATH];
	char *s;
	char *temp=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,MAX_PATH);
	char *pathtemp= NULL;
	gosver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	if (!GetVersionEx(&gosver)) {
		MessageBox(NULL,"GetVersionEx failed","zsh",MB_ICONHAND);
		ExitProcess(0xFF);
	}
	if (__forked)
		goto skippy;
	if (GetEnvironmentVariable("HOME",ptr,MAX_PATH) != 0){
		s = ptr;
		while(*s){
			if (*s == '\\') *s = '/';
			s++;
		}
		SetEnvironmentVariable("HOME",ptr);
		goto skippy;
	}
	if( GetEnvironmentVariable("ZDOTDIR",ptr,MAX_PATH)  != 0)
		goto skippy;
	
	if(gosver.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		if (gosver.dwMajorVersion <4) {
			GetEnvironmentVariable("HOMEDRIVE",hdrive,MAX_PATH);
			GetEnvironmentVariable("HOMEPATH",hpath,MAX_PATH);
			wsprintf(temp,"%s%s",hdrive,hpath );
		}
		else if (gosver.dwMajorVersion >= 4) {
			GetEnvironmentVariable("USERPROFILE",temp,MAX_PATH);
		}
	}
	else if (gosver.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
		rc = GetWindowsDirectory(temp,MAX_PATH);
		if (rc > MAX_PATH) {
			MessageBox(NULL,"This should never happen","zsh",MB_ICONHAND);
			ExitProcess(0xFF);
		}
	}
	else {
		MessageBox(NULL,"Unknown platform","zsh",MB_ICONHAND);
	}
	s = temp;
	while(*s) {
		if(*s == '\\') *s ='/';
		*s++;
	}


	SetEnvironmentVariable("HOME",temp);
skippy:

	rc = GetEnvironmentVariable("PATH",NULL,0);
	pathtemp = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,rc+1);
	rc = GetEnvironmentVariable("PATH",pathtemp,rc+1);

	s = pathtemp;
	while(*s) {
		if(*s == '\\') *s ='/';
		*s++;
	}
	SetEnvironmentVariable("PATH",pathtemp);

	gdwPlatform = gosver.dwPlatformId;

	HeapFree(GetProcessHeap(),0,temp);
	HeapFree(GetProcessHeap(),0,pathtemp);

	init_stdio();
	nt_term_init();
	nt_init_signals();
	init_shell_dll();
	init_plister();

}
void gethostname(char *buf, int len) {
	GetComputerName(buf,&len);
}
void set_default_path(char **path) {
	char temp[128];
	char *ptr=NULL;

	if (gdwPlatform == VER_PLATFORM_WIN32_NT) {
		GetWindowsDirectory(temp,128);
		ptr = temp;
		while(*ptr) {
			if (*ptr == '\\') *ptr = '/';
			ptr++;
		}
		path[0] = ztrdup(temp);
		wsprintf(temp,"%s/system32",path[0]);
		path[1]=ztrdup(temp);
		wsprintf(temp,"%s/system",path[0]);
		path[2]=ztrdup(temp);
		path[3]=ztrdup("c:/bin");
		path[4] = NULL;
	}
	else if(gdwPlatform == VER_PLATFORM_WIN32_WINDOWS) {
		GetWindowsDirectory(temp,128);
		ptr = temp;
		while(*ptr) {
			if (*ptr == '\\') *ptr = '/';
			ptr++;
		}
		path[0] = ztrdup(temp);
		wsprintf(temp,"%s/system",path[0]);
		path[1]=ztrdup(temp);
		wsprintf(temp,"%s/command",path[0]);
		path[2]=ztrdup(temp);
		path[3]=ztrdup("c:/bin");
		path[4] = NULL;
	}
	else {
		MessageBox(NULL,"Unknown platform","zsh",MB_ICONHAND);
	}

}
int gettimeofday(struct timeval *tv, struct timezone *tz) {

	SYSTEMTIME syst;

	GetLocalTime(&syst);

	tv->tv_sec = syst.wSecond;
	tv->tv_usec =syst.wMilliseconds*1000;
	return 0;
}
char *getlogin(void) {
	return "bogus";
}
void make_err_str(unsigned int error,char *buf,int size) {

	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
				  NULL,
				  error, 
				  MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
				  buf,
				  size,
				  NULL);
	return;

}
char * forward_slash_get_cwd(char * path,int size) {

	char *ptemp;
	char * cwd  ;

	GetCurrentDirectory(size,path);
	cwd = path;
	ptemp=cwd;

	while(*ptemp) {
		if (*ptemp == '\\') *ptemp = '/';
		*ptemp++;
	}
	return cwd;
}
static void quoteProtect(char *, char *) ;
void nt_execve_wrapped(char *prog, char**args, char**envir ) {

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	BOOL bRet;
    DWORD type=0;
	char *argv0;
	char **savedargs = args;
	unsigned int cmdsize,cmdlen;
	char *cmdstr ,*cmdend;
	char *p2;
	int rc=0,retries=0;
	int is_winnt;
	int hasdot=0;


	memset(&si,0,sizeof(si));

	/* 
	 * This memory is not freed because we are exec()ed and will
	 * not be alive long.
	 */
	cmdstr= heap_alloc(MAX_PATH<<2);

	is_winnt = (gdwPlatform != VER_PLATFORM_WIN32_WINDOWS);

	cmdsize = MAX_PATH << 2;

	p2 = cmdstr;

	cmdlen = 0;

	cmdlen += copy_quote_and_fix_slashes(prog,cmdstr,&hasdot);

	p2 += cmdlen;

	if (*cmdstr != '"') {// copy_quote leaves a leading space
		*cmdstr = 'A';
		cmdstr++;
	}
	*p2 = 0;
	cmdend = p2;

	if (!is_winnt){
		argv0 = NULL;
		goto win95_directly_here;
	}
	else {
		argv0 = heap_alloc(MAX_PATH);
		wsprintf(argv0,"%s",prog);
	}

retry:

	bRet=GetBinaryType(argv0,&type);

	if (is_winnt && !bRet  ) {
		/* Don't append .EXE if it could be a script file */
		if (GetLastError() == ERROR_BAD_EXE_FORMAT){
			try_shell_ex(args,1);
			errno = ENOEXEC;
			return;
		}
		else if (retries){
			/* If argv[0] got parsed as \\foo (stupid paths with '/' as one
			 * of the components will do it,) and if argv[1] is not the same,
			 * then this is not a real UNC name. 
			 * In other cases, argv[0] and argv[1] here must be the same
			 * anyway. -amol 5/1/6/98
			 */
			if (
				( (*prog == '\\') ||(*prog == '/') ) &&
				( (prog[1] == '\\') ||(prog[1] == '/') ) &&
				((*args[0] == *prog) && (args[0][1] == prog[1])) && 
				(!args[1])
			   )
				try_shell_ex(args,1);
			errno  = ENOENT;
		}
		if (retries > 2){
			return;
		}
		if (retries == 0) {
			wsprintf(argv0,"%s.EXE",prog);
			retries++;
		}
		else if (prog[0] == '\\' && retries == 1) {
			char ptr[80];
			if(GetEnvironmentVariable("ZSHROOT",ptr,80)) {
				wsprintf(argv0,"%s%s",ptr,prog);
			}
			retries++;
		}
		else if (prog[0] == '\\' && retries == 2) {
			char ptr[80];
			if(GetEnvironmentVariable("ZSHROOT",ptr,80)) {
				wsprintf(argv0,"%s%s.EXE",ptr,prog);
			}
			retries++;
		}
		else 
			retries += 2;
		goto retry;
	}
	else if (bRet && retries > 0) { //re-fix argv0
		cmdsize = MAX_PATH << 2;

		cmdstr--; //back to allocated pointer

		p2 = cmdstr;

		cmdlen = 0;

		cmdlen += copy_quote_and_fix_slashes(argv0,cmdstr,&hasdot);

		p2 += cmdlen;

		if (*cmdstr != '"') {// copy_quote leaves a leading space
			*cmdstr = 'A';
			cmdstr++;
		}
		*p2 = 0;
		cmdend = p2;
	}

win95_directly_here:

	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = (HANDLE)_get_osfhandle(0);
	si.hStdOutput = (HANDLE)_get_osfhandle(1);
	si.hStdError = (HANDLE)_get_osfhandle(2);

	*args++; // ignore argv[0];

	concat_args_and_quote(args,&cmdstr,&cmdlen,&cmdend,&cmdsize);
	*cmdend = 0;
	if (gdwPlatform != VER_PLATFORM_WIN32_WINDOWS){
		if (!SetConsoleCtrlHandler(NULL,FALSE)) {
			errno = ENOENT;
		}
	}
	fix_path_for_child();

	if (cmdlen < 1000)
		dprintf("argv0 %s cmdstr %s\n",argv0,cmdstr);
	if (!CreateProcess(argv0,
			cmdstr,
			NULL,
			NULL,
			TRUE, // need this for redirecting std handles
			0,
			NULL,//envcrap,
			NULL,
			&si,
			&pi) ){

		if (GetLastError() == ERROR_BAD_EXE_FORMAT) {
			try_shell_ex(savedargs,1);
			errno  = ENOEXEC;
		}
		else if (GetLastError() == ERROR_INVALID_PARAMETER) {
			/* exceeded command line */
			errno = ENAMETOOLONG;
		}
		else
			errno  = ENOENT;
	}
	else{
		errno= 0;
		//
		{
			DWORD exitcode=0;
			int gui_app = 0;
			if (gdwPlatform != VER_PLATFORM_WIN32_WINDOWS){
				SetConsoleCtrlHandler(NULL,TRUE);
			}
			if (isset(WINNTWAITFORGUIAPPS))
				gui_app = 0;
			else if (is_winnt)
				gui_app = is_gui(argv0);
			else 
				gui_app = is_9x_gui(prog);

			if (!gui_app) {
				WaitForSingleObject(pi.hProcess,INFINITE);
			}
			(void)GetExitCodeProcess(pi.hProcess,&exitcode);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			ExitProcess(exitcode);
		}
	}
}
void nt_execve(char *prog, char**args, char**envir ) {
#ifndef DBG
	__try {
		nt_execve_wrapped(prog, args, envir);
	}__except(1) {
		fprintf(stderr,"Command line overflow. There is a limit, you know\n");
		ExitProcess(0);
	}
#else
		nt_execve_wrapped(prog, args, envir);
#endif DBG
}
/* This function from  Mark Tucker (mtucker@fiji.sidefx.com) */
static void quoteProtect(char *dest, char *src) {
	char	*prev, *curr;
	for (curr = src; *curr; curr++) {

		// Protect " from MS-DOS expansion
		if (*curr == '"') {
			// Now, protect each preceeding backslash
			for (prev = curr-1; prev >= src && *prev == '\\'; prev--)
				*dest++ = '\\';

			*dest++ = '\\';
		}
		*dest++ = *curr;
	}
	*dest = 0;
}
int nt_chdir (char *path) {
	char *tmp = path;
	if (gdwPlatform !=VER_PLATFORM_WIN32_NT) {
		while(*tmp) {
			if (*tmp == '/') *tmp = '\\';
			tmp++;
		}
	}
	return _chdir(path);
}
void caseify_pwd(char *curwd) {
	char *sp, *dp, p,*s;
	WIN32_FIND_DATA fdata;
	HANDLE hFind;

	if (gdwPlatform !=VER_PLATFORM_WIN32_NT) 
		return;
	
	if (*curwd == '/' && (!curwd[1] || curwd[1] == '/'))
		return;
	sp = curwd +3;
	dp = curwd +3;
	do {
		p= *sp;
		if (p && p != '/'){
			sp++;
			continue;
		}
		else {
			*sp = 0;
			hFind = FindFirstFile(curwd,&fdata);
			*sp = p;
			if (hFind != INVALID_HANDLE_VALUE) {
				FindClose(hFind);
				s = fdata.cFileName;	
				while(*s) {
					*dp++ = *s++;
				}
				dp++;
				sp = dp;
			}
			else {
				sp++;
				dp = sp;
			}
		}
		sp++;
	}while(p != 0);

}
unsigned char * def_ext = ".com;.exe;.cmd;.bat;";

int is_pathext(char *extension) {
	unsigned char exts[80];
	int rc;
	unsigned char *begin,*end;

	rc = GetEnvironmentVariable("PATHEXT",exts,80);
	if (rc >80)
		return 0;
	if (rc == 0) {
		begin = def_ext;
		end = def_ext;
	}
	else {
		begin = exts;
		end = exts;
	}
	while(*begin) {
		while(*begin && (*begin != '.'))
			begin++;
		while(*end && (*end != ';'))
			end++;
		if (!*begin)
			break;
		if (!strnicmp(begin+1,extension,end-begin-1))
			return 1;
		begin = end;
		end++;
	}
	return 0;

}
extern void mainCRTStartup(void *);
extern void heap_init(void);
void silly_entry(void *peb) {
	char def_term[]={'v','t','1','0','0','\0'};
	char * path1=NULL;
	int rc;
	heap_init();
	rc = GetEnvironmentVariable("ZSH_ISWIN95",path1,0);
	if (rc == 0)
		gisWin95 = 0;
	else 
		gisWin95 = 1;

	rc = GetEnvironmentVariable("TERM",path1,0);
	if (rc == 0)
		SetEnvironmentVariable("TERM",def_term);
	rc = GetEnvironmentVariable("Path",path1,0);
	if ( rc !=0) {
		path1 =heap_alloc(rc+1);
		GetEnvironmentVariable("Path",path1,rc);
		SetEnvironmentVariable("Path",NULL);
		SetEnvironmentVariable("PATH",NULL);
		SetEnvironmentVariable("PATH",path1);
		heap_free(path1);
	}
	nt_init();
	//fork_init();
	mainCRTStartup(peb);
}
void fix_path_for_child(void){
	char *pathstr;
	char *ptr;
	int len;

	if (isset(WINNTLAMEPATHFIX) ) {
		len = GetEnvironmentVariable("PATH",NULL,0);
		pathstr = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,len+1);

		if (GetEnvironmentVariable("PATH",pathstr,1024)  > 0 ) {
			ptr = pathstr;
			while(*ptr) {
				if (*ptr == '/')
					*ptr = '\\';
				ptr++;
			}
			SetEnvironmentVariable("PATH",pathstr);
		}
	}

}

/* 
 * Copy source into target, quote if it has space, also converting '/' to '\'. 
 *
 * hasdot is set to 1 if source ends in a file extension
 * return value is the  length of the string copied.
 */
int copy_quote_and_fix_slashes(char *source,char *target, int *hasdot ) {

	int len ;
	int hasspace;
	char *save;
	char *ptr;

	save = target; /* leave space for quote */
	len = 1;

	target++;

	hasspace = 0;
	while(*source) {
		if (*source == '/')
			*source = '\\';
		else if (*source == ' ')
			hasspace = 1;

		*target++ = *source;

		source++;
		len++;
	}
	ptr  = target;//source;
	while( (ptr > save ) && (*ptr != '/')) {
		if (*ptr == '.')
			*hasdot = 1;
		ptr--;
	}

	if (hasspace) {
		*save = '"';
		*target = '"';
		len++;
	}
	return len;
}
/*
 * This routine is a replacement for the old, horrible strcat() loop
 * that was used to turn the argv[] array into a string for CreateProcess().
 * It's about a zillion times faster. 
 * -amol 2/4/99
 */
void concat_args_and_quote(char **args, char **cstr, unsigned int *clen,
		char **cend, unsigned int *cmdsize) {

	unsigned int argcount, arglen, cmdlen;
	char *tempptr, *cmdend ,*cmdstr;
	short quotespace = 0;
	short quotequote = 0;
	char tempquotedbuf[256];

	/* 
		quotespace hack needed since execv() would have separated args, but
		createproces doesnt
		-amol 9/14/96
	*/
	cmdend= *cend;
	cmdstr = *cstr;
	cmdlen = *clen;

	argcount = 0;
	while (*args && (cmdlen < 65500) ) {

		*cmdend++ = ' ';
		cmdlen++;

		tempptr = *args;

		arglen = 0;
		argcount++;

		if (!*tempptr) {
			*cmdend++ = '"';
			*cmdend++ = '"';
		}
		while(*tempptr) {
			if (*tempptr == ' ' || *tempptr == '\t') 
				quotespace = 1;
			else if (*tempptr == '"')
				quotequote = 1;
			tempptr++;
			arglen++;
		}
        if (arglen + cmdlen +4 > *cmdsize) { // +4 is if we have to quote
            tempptr = cmdstr-1;
			dprintf("Heap realloc before 0x%08x\n",cmdstr-1);
            cmdstr = heap_realloc(cmdstr-1,*cmdsize<<1);
            if (tempptr != cmdstr) {
                cmdend = cmdstr + (cmdend-tempptr);
            }
            cmdstr++;
			dprintf("Heap realloc after 0x%08x\n",cmdstr-1);
            *cmdsize <<=1;
        }
		if (quotespace)
			*cmdend++ = '"';

        if (!isset(WINNTNOQUOTEPROTECT) && quotequote){
            tempquotedbuf[0]=0;

            tempptr = &tempquotedbuf[0];
            quoteProtect(tempquotedbuf,*args);
            while (*tempptr) {
                *cmdend = *tempptr;
                cmdend++;
                tempptr++;
            }
            cmdlen +=2;
        }
        else {
            tempptr = *args;
            while(*tempptr) {
                *cmdend = *tempptr;
                cmdend++;
                tempptr++;
            }
        }

        if (quotespace) {
            *cmdend++ = '"';
            cmdlen +=2;
        }
        cmdlen += arglen;

        args++;
	}
	*clen = cmdlen;
	*cend = cmdend;
	*cstr = cmdstr;


}
/* Takes pwd as argument. extracts drive letter or server name 
 * and puts parentheses around it before returning the extracted
 * string
 */

static char xtractbuf[256];
char * fmt_pwd_for_prompt(char *dir) {

	char *ptr;

	if (!dir)
		return NULL;

	if (!(*dir) || !(*(dir+1)) )
		return NULL;

	if (*dir == '/' && *(dir+1) != '/')
		return NULL;

	xtractbuf[0] = '(';

	ptr = &xtractbuf[1];

	if (*(dir+1) == ':') { /* path with mapped or local drive*/
		xtractbuf[1] = *dir;	
		xtractbuf[2] = *(dir+1);	
		xtractbuf[3] = ')';
		xtractbuf[4] = 0;
	}
	else if (*dir == '/' && *(dir+1) == '/') {
		*ptr++ = *dir++;
		*ptr++ = *dir++;
		while(*dir && (*dir != '/')) {
			*ptr = *dir;

			dir++;
			ptr++;
		}
		*ptr++ = ')';
		*ptr = 0;
	}
	return &xtractbuf[0];
}
