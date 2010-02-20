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
 * The fork() here is based on the ideas used by cygwin
 * -amol
 */

/*
 * The memory allocator herein is part of the tcsh shell distribution.
 * See tc.alloc.c in the tcsh distribution. As of tcsh release 6.12.00,
 * this code is under the 3-clause BSD license.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include <setjmp.h>
#include <errno.h>

#pragma intrinsic("memcpy", "memset","memcmp")

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

typedef unsigned long u_long;
typedef unsigned long memalign_t;
typedef unsigned long caddr_t;
typedef void *ptr_t;
typedef unsigned char U_char;	
typedef unsigned int U_int;
typedef unsigned short U_short;
typedef unsigned long U_long;

#define __P(a) a
#define MEMALIGN(a) (((a) + ROUNDUP) & ~ROUNDUP)

union overhead {
	union overhead *ov_next;	/* when free */
	struct {
		U_char  ovu_magic;	/* magic number */
		U_char  ovu_index;	/* bucket # */
#ifdef RCHECK
		U_short ovu_size;	/* actual block size */
		U_int   ovu_rmagic;	/* range magic number */
#endif
	}       ovu;
#define	ov_magic	ovu.ovu_magic
#define	ov_index	ovu.ovu_index
#define	ov_size		ovu.ovu_size
#define	ov_rmagic	ovu.ovu_rmagic
};

#define	MAGIC		0xfd	/* magic # on accounting info */
#define RMAGIC		0x55555555	/* magic # on range info */
#ifdef RCHECK
#define	RSLOP		sizeof (U_int)
#else
#define	RSLOP		0
#endif


#define ROUNDUP	7

#define	NBUCKETS ((sizeof(long) << 3) - 3)

static	int	findbucket	__P((union overhead *, int));
static	void	morecore	__P((int));

extern void copy_fds(void);
extern void restore_fds(void);
extern void start_sigchild_thread(HANDLE,DWORD);
extern void close_copied_fds(void);

/* 
 * This section marks the beginning of data that will be copied to the 
 * child process. The assumption here is that everything from fork_seg_begin
 * to fork_seg_end is placed contiguously by the linker. This seems logical
 * to me, but I don't really know that much about compilers and linkers.
 * -amol 2/6/97
 */

void stack_probe(void *ptr) ;
void heap_init(void);

//
// This is exported in the user program.
// It must return 0 for no error !!!!
extern int fork_copy_user_mem(HANDLE );

unsigned long __fork_seg_begin = 0;

unsigned long __forked = 0;
unsigned long  *__fork_stack_begin = 0;
HANDLE __hforkparent=0, __hforkchild=0;
jmp_buf __fork_context = {0};
unsigned long __heap_base = 0;
unsigned long __heap_size = 0;
unsigned long __heap_top = 0;

char   *__memtop = NULL;		/* PWP: top of current memory */
char   *__membot = NULL;		/* PWP: bottom of allocatable memory */
union overhead *__nextf[NBUCKETS] = {0};
U_int __nmalloc[NBUCKETS]={0};

int     __realloc_srchlen = 4;	
unsigned long __fork_seg_end = 0;

/* End of shared data. */


u_long _old_exr = 0; // Saved exception registration for longjmp

/*
 * This hack is an attempt at getting to the exception registration
 * in an architecture-independent way. It's critical for longjmp in a
 * code using __try/__except blocks. Microsoft Visual C++ does a global
 * unwind during a longjmp, and that can cause havoc if the exception 
 * registration stored in longjmp is lower(address wise, indicating a jump
 * from below of the stack upward.) in the stack than the current
 * registration (returned by NtCurrentTeb).
 *
 * This works with VC++, because that's all I have. With other compilers, 
 * there might be minimal changes required, depending on where the 
 * exception registration record is stored in the longjmp structure.
 *
 * -amol 2/6/97
 */

#ifndef _M_IX86
#pragma error ("sorry, only X86 support so far")
#endif
NT_TIB * (* myNtCurrentTeb)(void);

#define GETEXCEPTIONREGIST() (((NT_TIB*)get_teb())->ExceptionList)
#define GETSTACKBASE()		 (((NT_TIB*)get_teb())->StackBase)

void *get_teb(void) {

	NT_TIB *the_tib;

	myNtCurrentTeb = (void*)GetProcAddress(LoadLibrary("ntdll.dll"),
			"NtCurrentTeb");
	if (!myNtCurrentTeb)
		return NULL;
	the_tib = myNtCurrentTeb();

	if (the_tib == NULL)
		abort();
	return the_tib;
}

#ifdef NTDBG
#define FORK_TIMEOUT INFINITE
#else
#define FORK_TIMEOUT 50000
#endif
/* 
 * This must be called by the application as the first thing it does.
 *
 * -amol 2/6/97
 */
int fork_init(void) {

	unsigned long stackbase;

	stackbase = (unsigned long)GETSTACKBASE();

	__fork_stack_begin =(ULONG*)stackbase;

	//	heap_init();

	if (__forked) {

		// stack_probe probes out a decent-sized stack for the child,
		// since initially it has a very small stack.
		//
		stack_probe((char *)__fork_stack_begin - 0x2000);

		//
		// Save the old Exception registration record and jump
		// off the cliff.
		//
		_old_exr = __fork_context[6];
		__fork_context[6] =(int)GETEXCEPTIONREGIST();//tmp;
		//
		// Whee !
		longjmp(__fork_context,1);
	}

	return 0;
}
int fork(void) {

	int MAY_ALIAS rc;
        int stacksize;
	char modname[512];
	HANDLE  hProc,hThread, hArray[2];
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	SECURITY_ATTRIBUTES sa;
	DWORD object_active;

	unsigned long fork_stack_end;

	//
	// Create two inheritable events
	//
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor =0;
	sa.bInheritHandle = TRUE;
	if (!__hforkchild)
		__hforkchild = CreateEvent(&sa,TRUE,FALSE,NULL);
	if (!__hforkparent)
		__hforkparent = CreateEvent(&sa,TRUE,FALSE,NULL);

	rc = setjmp(__fork_context);

	if (rc) { // child
		//
		// Restore old registration
		// -amol 2/2/97
		GETEXCEPTIONREGIST() = (struct _EXCEPTION_REGISTRATION_RECORD*)_old_exr;

		SetEvent(__hforkchild);

		if(WaitForSingleObject(__hforkparent,FORK_TIMEOUT) != WAIT_OBJECT_0)
			ExitProcess(0xFFFF);

		CloseHandle(__hforkchild);
		CloseHandle(__hforkparent);

		__hforkchild = __hforkparent=0;

		restore_fds();
		return 0;
	}

	copy_fds();

	memset(&si,0,sizeof(si));
	si.cb= sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;

	si.hStdInput= GetStdHandle(STD_INPUT_HANDLE);
	si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

	if (GetModuleFileName(GetModuleHandle(NULL),modname,512) > 512) {
		errno = ENOENT;
		rc = GetLastError();
		return -1;
	}
	rc = CreateProcess(modname,
			NULL,
			NULL,
			NULL,
			TRUE,
			CREATE_SUSPENDED,
			NULL,
			NULL,
			&si,
			&pi);
	if (!rc) {
		errno =EPERM;
		return -1;
	}

	ResetEvent(__hforkchild);
	ResetEvent(__hforkparent);

	hProc = pi.hProcess;
	hThread = pi.hThread;


	__forked=1;
	//
	// Copy all the shared data
	//
	if (!WriteProcessMemory(hProc,&__fork_seg_begin,&__fork_seg_begin,
			(char*)&__fork_seg_end - (char*)&__fork_seg_begin,
                        (unsigned long*) &rc)) {
		goto error;
	}
	__forked=0;


	rc = ResumeThread(hThread);

	//
	// Wait for the child to start and init itself.
	// The timeout is so that we don't wait too long
	//
	hArray[0] = __hforkchild;
	hArray[1] = hProc;

	object_active = WaitForMultipleObjects(2,hArray,FALSE,FORK_TIMEOUT);
	if (object_active != WAIT_OBJECT_0) {
//		int err = GetLastError(); // For debugging purposes // unused variable
		goto error;
	}

	// Stop the child again and copy the stack and heap
	//
	SuspendThread(hThread);

	// stack
	stacksize = __fork_stack_begin - &fork_stack_end;
	stacksize *= sizeof(unsigned long);
	if (!WriteProcessMemory(hProc,(char *)&fork_stack_end,
			(char *)&fork_stack_end,
			stacksize,
			(unsigned long *)&rc)){
		errno = EINVAL;
		goto error;
	}
	//
	// copy heap itself
	if (!WriteProcessMemory(hProc, (void*)__heap_base,(void*)__heap_base, 
			__heap_top-__heap_base,(unsigned long *)&rc)){
		goto error;
	}

	rc = fork_copy_user_mem(hProc);

	if(rc) {
		errno = ESRCH;
		goto error;
	}

	// Release the child.
	SetEvent(__hforkparent);
	rc = ResumeThread(hThread);

	start_sigchild_thread(hProc,pi.dwProcessId);
	close_copied_fds();
	CloseHandle(hThread);
	//
	// return process id to parent.
	return pi.dwProcessId;

error:
	CloseHandle(hProc);
	CloseHandle(hThread);
	return -1;
}
#pragma optimize("",off)
void stack_probe (void *ptr) {
	char buf[1000];
	int x;

	if (&x > (int *)ptr)
		stack_probe(ptr);
	(void)buf;
}
#pragma optimize("",on)
//
// This function basically reserves some heap space.
// In the child it also commits the size committed in the parent.
void heap_init(void) {

	char * temp;
	int rc;
	if (__forked) {
		temp = (char *)VirtualAlloc((void*)__heap_base,__heap_size, MEM_RESERVE,
				PAGE_READWRITE);
		rc = GetLastError();
		if (temp != (char*)__heap_base) 
			abort();
		if (!VirtualAlloc((void*)__heap_base,__heap_top - __heap_base, 
				MEM_COMMIT,PAGE_READWRITE))
			abort();
		temp = (char*)__heap_base;
	}
	else {
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		__heap_size = ((sysinfo.dwPageSize << 10) <<4);
		__heap_base = (unsigned long)VirtualAlloc(0 , __heap_size,MEM_RESERVE,
				PAGE_READWRITE);

		if (__heap_base == 0) {
			abort();
		}

		__heap_top = __heap_base;
	}

}
//
// Implementation of sbrk() for the fmalloc family
//
void * sbrk(int delta) {

	u_long retval,old_top=__heap_top;

	if (delta == 0)
		return (void*) __heap_top;
	if (delta > 0) {
		retval = (u_long)VirtualAlloc((void*)__heap_top, delta,MEM_COMMIT,
				PAGE_READWRITE);
		if (retval == 0 )
			abort();
		__heap_top += delta;
	}
	else {
		retval = (u_long)VirtualAlloc((void*)((char*)__heap_top-(char*)delta), 
				delta,MEM_DECOMMIT, PAGE_READWRITE);
		if (retval == 0)
			abort();
		__heap_top -= delta;
	}

        return (void*) old_top;
}

#include "tc.alloc.c"
