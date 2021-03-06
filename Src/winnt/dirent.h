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
 * dirent.h
 * directory interface functions. Sort of like dirent functions on unix.
 * -amol
 */

#ifndef DIRENT_H
#define DIRENT_H

//#define _WINSOCKAPI_ // conflicts with timeval
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define NAME_MAX MAX_PATH

#define IS_ROOT 0x01
#define IS_NET  0x02

#define d_fileno d_ino
struct dirent {
	long            d_ino;
	int             d_off;
	unsigned short  d_reclen;
	char            d_name[NAME_MAX+1];
};

typedef struct {
	HANDLE dd_fd;
	int dd_loc;
	int dd_size;
	int flags;
	char orig_dir_name[NAME_MAX +1];
	struct dirent *dd_buf;
}DIR;

DIR *opendir(char*);
struct dirent *readdir(DIR*);
int closedir(DIR*);
void rewinddir(DIR*);
#endif /* DIRENT_H */
