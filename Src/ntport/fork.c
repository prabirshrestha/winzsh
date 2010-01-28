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
 The fork() here is based on the ideas used by cygwin
 -amol
 */

/*
 The memory allocator herein is part of the tcsh shell distribution.
 See tc.alloc.c in the tcsh distribution. As of tcsh release 6.12.00,
 this code is under the 3-clause BSD license.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include <setjmp.h>
#include <errno.h>

#pragma intrinsic("memcpy", "memset","memcmp")

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

	int rc,stacksize;
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
			(char*)&__fork_seg_end - (char*)&__fork_seg_begin,&rc)) {
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
		int err = GetLastError(); // For debugging purposes
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
			&rc)){
		errno = EINVAL;
		goto error;
	}
	//
	// copy heap itself
	if (!WriteProcessMemory(hProc, (void*)__heap_base,(void*)__heap_base, 
			__heap_top-__heap_base,&rc)){
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
		if (retval = 0)
			abort();
		__heap_top -= delta;
	}

	return (void*) old_top;
}

/* $Header: /u/christos/src/tcsh-6.06/RCS/tc.alloc.c,v 3.29 1995/04/16 19:15:53 christos Exp $ */
/*
 * tc.alloc.c (Caltech) 2/21/82
 * Chris Kingsley, kingsley@cit-20.
 *
 * This is a very fast storage allocator.  It allocates blocks of a small
 * number of different sizes, and keeps free lists of each size.  Blocks that
 * don't exactly fit are passed up to the next larger size.  In this
 * implementation, the available sizes are 2^n-4 (or 2^n-12) bytes long.
 * This is designed for use in a program that uses vast quantities of memory,
 * but bombs when it runs out.
 */
/* Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define CGETS(b,c,d) d
#define xprintf 
#define __P(a) a


#ifndef NULL
#define NULL 0
#endif NULL

/*
   typedef unsigned long memalign_t;
//typedef unsigned long size_t;
typedef unsigned long caddr_t;
typedef void *ptr_t;
 */

void showall(void*,void*);
void *sbrk(int);

//RCSID("$Id: tc.alloc.c,v 3.29 1995/04/16 19:15:53 christos Exp $")

#ifdef NOTNT
char   *memtop = NULL;		/* PWP: top of current memory */
char   *membot = NULL;		/* PWP: bottom of allocatable memory */
#endif NOTNT





/*
 * Lots of os routines are busted and try to free invalid pointers. 
 * Although our free routine is smart enough and it will pick bad 
 * pointers most of the time, in cases where we know we are going to get
 * a bad pointer, we'd rather leak.
 */

#ifdef NOTNT
typedef unsigned char U_char;	/* we don't really have signed chars */
typedef unsigned int U_int;
typedef unsigned short U_short;
typedef unsigned long U_long;

#endif NOTNT

/*
 * The overhead on a block is at least 4 bytes.  When free, this space
 * contains a pointer to the next free block, and the bottom two bits must
 * be zero.  When in use, the first byte is set to MAGIC, and the second
 * byte is the size index.  The remaining bytes are for alignment.
 * If range checking is enabled and the size of the block fits
 * in two bytes, then the top two bytes hold the size of the requested block
 * plus the range checking words, and the header word MINUS ONE.
 */

#if NOTNT
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

/*
 * nextf[i] is the pointer to the next free block of size 2^(i+3).  The
 * smallest allocatable block is 8 bytes.  The overhead information
 * precedes the data area returned to the user.
 */
#define	NBUCKETS ((sizeof(long) << 3) - 3)
union overhead *nextf[NBUCKETS] = {0};

/*
 * nmalloc[i] is the difference between the number of mallocs and frees
 * for a given block size.
 */
U_int nmalloc[NBUCKETS];

#ifndef lint
static	int	findbucket	__P((union overhead *, int));
static	void	morecore	__P((int));
#endif

#endif NOTNT

#ifdef DEBUG
# define CHECK(a, str, p) \
if (a) { \
	xprintf(str, p);	\
		xprintf(" (memtop = %lx __membot = %lx)\n", __memtop, __membot);	\
		abort(); \
}
#else
# define CHECK(a, str, p) \
if (a) { \
	xprintf(str, p);	\
		xprintf(" (__memtop = %lx __membot = %lx)\n", __memtop, __membot);	\
		return; \
}
#endif

memalign_t
fmalloc(nbytes)
	register size_t nbytes;
{
	register union overhead *p;
	register int bucket = 0;
	register unsigned shiftr;

	/*
	 * Convert amount of memory requested into closest block size stored in
	 * hash buckets which satisfies request.  Account for space used per block
	 * for accounting.
	 */
#ifdef SUNOS4
	/*
	 * SunOS localtime() overwrites the 9th byte on an 8 byte malloc()....
	 * so we get one more...
	 * From Michael Schroeder: This is not true. It depends on the 
	 * timezone string. In Europe it can overwrite the 13th byte on a
	 * 12 byte malloc.
	 * So we punt and we always allocate an extra byte.
	 */
	nbytes++;
#endif

	nbytes = MEMALIGN(MEMALIGN(sizeof(union overhead)) + nbytes + RSLOP);
	shiftr = (nbytes - 1) >> 2;

	/* apart from this loop, this is O(1) */
	while ((shiftr >>= 1) != 0)
		bucket++;
	/*
	 * If nothing in hash bucket right now, request more memory from the
	 * system.
	 */
	if (__nextf[bucket] == NULL)
		morecore(bucket);
	if ((p = (union overhead *) __nextf[bucket]) == NULL) {
		showall(NULL, NULL);
		xprintf(CGETS(19, 1, "nbytes=%d: Out of memory\n"), nbytes);
		abort();
	}
	/* remove from linked list */
	__nextf[bucket] = __nextf[bucket]->ov_next;
	p->ov_magic = MAGIC;
	p->ov_index = bucket;
	__nmalloc[bucket]++;
#ifdef RCHECK
	/*
	 * Record allocated size of block and bound space with magic numbers.
	 */
	p->ov_size = (p->ov_index <= 13) ? nbytes - 1 : 0;
	p->ov_rmagic = RMAGIC;
	*((U_int *) (((caddr_t) p) + nbytes - RSLOP)) = RMAGIC;
#endif
	return ((memalign_t) (((caddr_t) p) + MEMALIGN(sizeof(union overhead))));
}

#ifndef lint
/*
 * Allocate more memory to the indicated bucket.
 */
	static void
morecore(bucket)
	register int bucket;
{
	register union overhead *op;
	register int rnu;		/* 2^rnu bytes will be requested */
	register int nblks;		/* become nblks blocks of the desired size */
	register int siz;

	if (__nextf[bucket])
		return;
	/*
	 * Insure memory is allocated on a page boundary.  Should make getpageize
	 * call?
	 */
	op = (union overhead *) sbrk(0);
	__memtop = (char *) op;
	if (__membot == NULL)
		__membot = __memtop;
	if ((long) op & 0x3ff) {
		__memtop = (char *) sbrk(1024 - ((long) op & 0x3ff));
		__memtop += (long) (1024 - ((long) op & 0x3ff));
	}

	/* take 2k unless the block is bigger than that */
	rnu = (bucket <= 8) ? 11 : bucket + 3;
	nblks = 1 << (rnu - (bucket + 3));	/* how many blocks to get */
	__memtop = (char *) sbrk(1 << rnu);	/* PWP */
	op = (union overhead *) __memtop;
	/* no more room! */
	if ((long) op == -1)
		return;
	__memtop += (long) (1 << rnu);
	/*
	 * Round up to minimum allocation size boundary and deduct from block count
	 * to reflect.
	 */
	if (((U_long) op) & ROUNDUP) {
		op = (union overhead *) (((U_long) op + (ROUNDUP + 1)) & ~ROUNDUP);
		nblks--;
	}
	/*
	 * Add new memory allocated to that on free list for this hash bucket.
	 */
	__nextf[bucket] = op;
	siz = 1 << (bucket + 3);
	while (--nblks > 0) {
		op->ov_next = (union overhead *) (((caddr_t) op) + siz);
		op = (union overhead *) (((caddr_t) op) + siz);
	}
	op->ov_next = NULL;
}

#endif

	void
ffree(cp)
	ptr_t   cp;
{
	register int size;
	register union overhead *op;

	/*
	 * the don't free flag is there so that we avoid os bugs in routines
	 * that free invalid pointers!
	 */
	if (cp == 0 )
		return;
	CHECK(!__memtop || !__membot,
			CGETS(19, 2, "free(%lx) called before any allocations."), cp);
	CHECK(cp > (ptr_t) __memtop,
			CGETS(19, 3, "free(%lx) above top of memory."), cp);
	CHECK(cp < (ptr_t) __membot,
			CGETS(19, 4, "free(%lx) below bottom of memory."), cp);
	op = (union overhead *) (((caddr_t) cp) - MEMALIGN(sizeof(union overhead)));
	CHECK(op->ov_magic != MAGIC,
			CGETS(19, 5, "free(%lx) bad block."), cp);

#ifdef RCHECK
	if (op->ov_index <= 13)
		CHECK(*(U_int *) ((caddr_t) op + op->ov_size + 1 - RSLOP) != RMAGIC,
				CGETS(19, 6, "free(%lx) bad range check."), cp);
#endif
	CHECK(op->ov_index >= NBUCKETS,
			CGETS(19, 7, "free(%lx) bad block index."), cp);
	size = op->ov_index;
	op->ov_next = __nextf[size];
	__nextf[size] = op;

	__nmalloc[size]--;

}

	memalign_t
fcalloc(i, j)
	size_t  i, j;
{
	register char *cp, *scp;

	i *= j;
	scp = cp = (char *) fmalloc((size_t) i);
	/*
	   if (i != 0)
	   do
	 *cp++ = 0;
	 while (--i);
	 */

	return ((memalign_t) scp);
}

/*
 * When a program attempts "storage compaction" as mentioned in the
 * old malloc man page, it realloc's an already freed block.  Usually
 * this is the last block it freed; occasionally it might be farther
 * back.  We have to search all the free lists for the block in order
 * to determine its bucket: 1st we make one pass thru the lists
 * checking only the first block in each; if that fails we search
 * ``realloc_srchlen'' blocks in each list for a match (the variable
 * is extern so the caller can modify it).  If that fails we just copy
 * however many bytes was given to realloc() and hope it's not huge.
 */
#ifdef NOTNT
#ifndef lint
/* 4 should be plenty, -1 =>'s whole list */
static int     realloc_srchlen = 4;	
#endif /* lint */
#endif NOTNT

//#undef realloc
//extern void* realloc(void*,size_t);

	memalign_t
frealloc(cp, nbytes)
	ptr_t   cp;
	size_t  nbytes;
{
	register U_int onb;
	union overhead *op;
	ptr_t res;
	register int i;
	int     was_alloced = 0;

	if (cp == 0)
		return (fmalloc(nbytes));
	if ( (unsigned long)cp < __heap_base || (unsigned long)cp > __heap_top ){
		return (unsigned long)realloc(cp,nbytes);
	}
	op = (union overhead *) (((caddr_t) cp) - MEMALIGN(sizeof(union overhead)));
	if (op->ov_magic == MAGIC) {
		was_alloced++;
		i = op->ov_index;
	}
	else
		/*
		 * Already free, doing "compaction".
		 * 
		 * Search for the old block of memory on the free list.  First, check the
		 * most common case (last element free'd), then (this failing) the last
		 * ``realloc_srchlen'' items free'd. If all lookups fail, then assume
		 * the size of the memory block being realloc'd is the smallest
		 * possible.
		 */
		if ((i = findbucket(op, 1)) < 0 &&
				(i = findbucket(op, __realloc_srchlen)) < 0)
			i = 0;

	onb = MEMALIGN(nbytes + MEMALIGN(sizeof(union overhead)) + RSLOP);

	/* avoid the copy if same size block */
	if (was_alloced && (onb <= (U_int) (1 << (i + 3))) && 
			(onb > (U_int) (1 << (i + 2)))) {
#ifdef RCHECK
		/* JMR: formerly this wasn't updated ! */
		nbytes = MEMALIGN(MEMALIGN(sizeof(union overhead))+nbytes+RSLOP);
		*((U_int *) (((caddr_t) op) + nbytes - RSLOP)) = RMAGIC;
		op->ov_rmagic = RMAGIC;
		op->ov_size = (op->ov_index <= 13) ? nbytes - 1 : 0;
#endif
		return ((memalign_t) cp);
	}
	if (((unsigned long)res = fmalloc(nbytes)) == 0)
		return ((memalign_t) NULL);
	if (cp != res) {		/* common optimization */
		/* 
		 * christos: this used to copy nbytes! It should copy the 
		 * smaller of the old and new size
		 */
		onb = (1 << (i + 3)) - MEMALIGN(sizeof(union overhead)) - RSLOP;
		(void) memmove((ptr_t) res, (ptr_t) cp, 
					   (size_t) (onb < nbytes ? onb : nbytes));
	}
	if (was_alloced)
		ffree(cp);
	return ((memalign_t) res);
}



#ifndef lint
/*
 * Search ``srchlen'' elements of each free list for a block whose
 * header starts at ``freep''.  If srchlen is -1 search the whole list.
 * Return bucket number, or -1 if not found.
 */
	static int
findbucket(freep, srchlen)
	union overhead *freep;
	int     srchlen;
{
	register union overhead *p;
	register int i, j;

	for (i = 0; i < NBUCKETS; i++) {
		j = 0;
		for (p = __nextf[i]; p && j != srchlen; p = p->ov_next) {
			if (p == freep)
				return (i);
			j++;
		}
	}
	return (-1);
}

#endif

/*
 * mstats - print out statistics about malloc
 *
 * Prints two lines of numbers, one showing the length of the free list
 * for each size category, the second showing the number of mallocs -
 * frees for each size category.
 */
/*ARGSUSED*/
	void
showall(v, c)
	void *v;
	void *c;
{
	register int i, j;
	register union overhead *p;
	int     totfree = 0, totused = 0;

	xprintf(CGETS(19, 8, "current memory allocation:\nfree:\t"));
	for (i = 0; i < NBUCKETS; i++) {
		for (j = 0, p = __nextf[i]; p; p = p->ov_next, j++)
			continue;
		xprintf(" %4d", j);
		totfree += j * (1 << (i + 3));
	}
	xprintf(CGETS(19, 9, "\nused:\t"));
	for (i = 0; i < NBUCKETS; i++) {
		xprintf(" %4u", __nmalloc[i]);
		totused += __nmalloc[i] * (1 << (i + 3));
	}
	xprintf(CGETS(19, 10, "\n\tTotal in use: %d, total free: %d\n"),
			totused, totfree);
	xprintf(CGETS(19, 11,
			"\tAllocated memory from 0x%lx to 0x%lx.  Real top at 0x%lx\n"),
			(unsigned long) __membot, (unsigned long) __memtop,
			(unsigned long) sbrk(0));
}

