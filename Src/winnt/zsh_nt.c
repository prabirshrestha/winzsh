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

#include "zsh.h"

char ** semicolonsplit(char *, int );
void semicolon_arrfixenv(char *, char **);

/**/
char *
semicolonarrgetfn(Param pm)
{
    return zjoin(*(char ***)pm->data, ';');
}

/**/
void
semicolonarrsetfn(Param pm, char *x)
{
    char ***dptr = (char ***)pm->data;

    freearray(*dptr);
    *dptr = x ? semicolonsplit(x, pm->flags & PM_UNIQUE) : mkarray(NULL);
    if (pm->ename)
	semicolon_arrfixenv(pm->nam, *dptr);
}
/**/
char **
semicolonsplit(char *s, int uniq)
{
    int ct;
    char *t, **ret, **ptr, **p;

    for (t = s, ct = 0; *t; t++) /* count number of semicolons */
	if (*t == ';')
	    ct++;
    ptr = ret = (char **) zalloc(sizeof(char **) * (ct + 2));

    t = s;
    do {
	s = t;
        /* move t to point at next colon */
	for (; *t && *t != ';'; t++);
	if (uniq)
	    for (p = ret; p < ptr; p++)
		if (strlen(*p) == t - s && ! strncmp(*p, s, t - s))
		    goto cont;
	*ptr = (char *) zalloc((t - s) + 1);
	ztrncpy(*ptr++, s, t - s);
      cont: ;
    }
    while (*t++);
    *ptr = NULL;
    return ret;
}
/**/
void
semicolon_arrfixenv(char *s, char **t)
{
    char **ep, *u;
    int len_s;
    Param pm;

    MUSTUSEHEAP("semicolon_arrfixenv");
    if (t == path)
	cmdnamtab->emptytable(cmdnamtab);
    u = zjoin(t, ';');
    len_s = strlen(s);
    pm = (Param) paramtab->getnode(paramtab, s);
    for (ep = environ; *ep; ep++)
	if (!strncmp(*ep, s, len_s) && (*ep)[len_s] == '=') {
	    pm->env = replenv(*ep, u);
//		(void)SetEnvironmentVariable(*ep,u);
	    return;
	}
    if (isset(ALLEXPORT))
	pm->flags |= PM_EXPORTED;
    if (pm->flags & PM_EXPORTED) {
		pm->env = addenv(s, u);
		(void)SetEnvironmentVariable(s,u);
	}
}
