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
 * signal.h
 * signal emulation things
 * -amol
 */

#ifndef SIGNAL_H
#define SIGNAL_H


#define NSIG 23     

// These must be CTRL_xxx_EVENT+1 (in wincon.h)
//
#define SIGINT		1 
#define SIGBREAK 	2
#define SIGHUP		3 //CTRL_CLOSE_EVENT
// 3 and 4 are reserved. hence we can't use 4 and 5
#define	SIGTERM		6 // ctrl_logoff
#define SIGKILL		7 // ctrl_shutdown

#define SIGILL		8 
#define SIGFPE		9	
#define SIGALRM		10
#define SIGWINCH	11
#define SIGSEGV 	12	
#define SIGSTOP 	13
#define SIGPIPE 	14
#define SIGCHLD 	15
#define SIGCONT		16 
#define SIGTSTP 	18
#define SIGTTOU 	19
#define SIGTTIN 	20
#define SIGABRT 	22	

#define SIGQUIT SIGBREAK

/* signal action codes */

#define SIG_DFL (void (*)(int))0	   /* default signal action */
#define SIG_IGN (void (*)(int))1	   /* ignore signal */
#define SIG_SGE (void (*)(int))3	   /* signal gets error */
#define SIG_ACK (void (*)(int))4	   /* acknowledge */


/* signal error value (returned by signal call on error) */

#define SIG_ERR (void (*)(int))-1	   /* signal error value */


#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#undef signal
#define signal _nt_signal

/*XXX: (Conflictive) sigset_t redefined
 *
 * Desc: sigset_t was originally defined at types.h:95. Furthermore, the
 *       original definition of sigset_t is of type int (and here it is of
 *       type unsigned long). I considered two courses of action regarding
 *       this:
 *
 *       a) Defining _NO_OLDNAMES when compiling the source, thus avoiding the
 *          original definition of sigset_t in types.h. This is not viable
 *          because the source also relies on other OLDNAMES definitions from
 *          this file.
 *       b) Disregarding this definition when compiling with gcc and 'hoping'
 *          no errors will arise from it.
 * Fix:  Option b.
 *
 * - Gabriel de Oliveira -
 */
#ifndef MINGW
typedef unsigned long sigset_t;
#endif /* !MINGW */

typedef void Sigfunc (int);

struct sigaction {
	Sigfunc *sa_handler;
	sigset_t sa_mask;
	int sa_flags;
};


#define sigemptyset(ptr) (*(ptr) = 0)
#define sigfillset(ptr)  ( *(ptr) = ~(sigset_t)0,0)

/* Function prototypes */

void (* _nt_signal(int, void (*)(int)))(int);

int sigaddset(sigset_t*, int);
int sigdelset(sigset_t*,int);
unsigned int alarm(unsigned int);

int sigismember(const sigset_t *set, int);
int sigprocmask(int ,const sigset_t*,sigset_t*);
int sigaction(int, const struct sigaction *, struct sigaction*);
int sigsuspend(const sigset_t *sigmask);

#define WNOHANG 0
#define WUNTRACED 1

#define WIFEXITED(a) 1
#define WEXITSTATUS(a) (a)
//#define WIFSIGNALED(a) (( ( (((unsigned long)(a)) >>24) & 0xC0) !=0) )
//
// this needs to be fixed ??
#define WIFSIGNALED(a) ( ((((unsigned long)(a)) & 0xC0000000 ) != 0))
#define WTERMSIG(a) ( ((unsigned long)(a))==0xC000013AL?SIGINT:SIGSEGV)
#define WCOREDUMP(a) 0
#define WIFSTOPPED(a) 0
#define WSTOPSIG(a) 0

int waitpid(pid_t, int*,int);
int times(struct tms*);
  
#endif /* SIGNAL_H */
