
/*******************************************************************************
 *                                                                             *
 * seh.s - Platform specific SEH functions for i486+ (32-bit)                  *
 *                                                                             *
 * LIBSEH - Structured Exception Handling compatibility library.               *
 * Copyright (c) 2009 Tom Bramer < tjb at postpro dot net >                    *
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

# SEH library functions... very platform specific

.global ___seh_register@4
.global ___seh_restore_context@8
.global ___seh_unregister@0
.global ___seh_get_registration@0
.global ___seh_set_registration@4
.global ___seh_pop_registration@0

/*
 * __seh_register: registers give __seh_buf object, while initializing it at 
 *                 the same time.
 *
 * Arguments: in __seh_buf*: Address of __seh_buf object which must be located
 *                           on the stack.
 *
 * Return value: 0 if returning from this function, or 1 if returning from 
 *               __seh_restore_context.
 *
 * Notes: This function implements a setjmp style non-local jump.  It may even
 *        be possible to use setjmp itself here, though that would not save one 
 *        from platform specific details.
 *
 *        The __seh_buf structure is a superset of the EXCEPTION_REGISTRATION 
 *        type used normally to register an exception handler.  Microsoft C
 *        also uses an extended version of this structure to implement SEH
 *        in the compiler.
 *
 *        %fs:0 is the first double word in the thread information block.  
 *        A linked-list of exception handler registration blocks is formed here,
 *        with %fs:0 pointing to the most recently added handler.
 */

___seh_register@4:
    movl %ebx, -0x4(%esp);
    movl %ebx, %eax;
    popl %edx;   /* Return address */
    popl %ebx;   /* Pointer to SEH buffer */
    subl $0xc, %esp;
    pushl %ecx;
    movl %fs:0, %ecx;
    movl %ecx, 0(%ebx);
    movl %ebx, %fs:0;

    movl %esi, 12(%ebx);
    movl %edi, 16(%ebx);
    movl %eax, 20(%ebx);
    movl %edx, 24(%ebx);
    movl %ebp, 28(%ebx);
    movl %esp, 32(%ebx);

    leal ___seh_exception_handler, %eax;
    movl %eax, 4(%ebx);

    /* Initialize everything else */
    /* For some reason, on GCC 4.3.2, the
       compiler inserts a movl %eax, 8(%ebp) 
       in __seh_init_buf, for reasons I don't 
       quite understand (strange calling convention?).
    */
    pushl %ecx;
    pushl %ebx;
    pushl %ecx;
    pushl %ebx;
    call ___seh_init_buf;
    popl %ebx;
    popl %ecx;
    popl %ebx;
    popl %ecx;
    movl 24(%ebx), %edx;

    popl %ecx;
    popl %ebx;
    addl $0x08, %esp;

    movl $0, %eax;
    jmp *%edx;

/*
 * __seh_restore_context: an over-glorified longjmp-like function.  
 *
 * Arguments: in __seh_buf*: Address of __seh_buf object which must be located
 *                           on the stack.
 *            in int:        Return value to give to the caller of __seh_register
 *
 */
___seh_restore_context@8:
    popl %edx;   /* Return address... we don't need it. */
    popl %ebx;   /* Context buffer */
    popl %eax;   /* Return value */
    movl 12(%ebx), %esi;
    movl 16(%ebx), %edi;
    movl 20(%ebx), %ecx;
    movl 24(%ebx), %edx;
    movl 28(%ebx), %ebp;
    movl 32(%ebx), %esp;
    movl %ecx, %ebx;
    jmp *%edx;

/*
 * __seh_unregister: pops the last registered handler off the handler stack,
 *                   also releasing any resources that it may be holding on to that
 *                   aren't needed anymore.
 *
 * Arguments: none
 *
 */

___seh_unregister@0:
    pushl %ebx;
    movl %fs:0, %ebx;
    movl 0(%ebx), %ecx;
    pushl %ecx;
    pushl %ebx;
    call ___seh_fini_buf;
    addl $8, %esp;

    movl %fs:0, %ebx;
    movl 0(%ebx), %ebx;
    movl %ebx, %fs:0;
    pop %ebx;
    ret;

/*
 * __seh_get_registration: returns the last registered handler registration off the
 *                         handler stack.
 *
 * Return value: __seh_buf*  pointer to the handler block.
 *
 */

___seh_get_registration@0:
    movl %fs:0, %eax;
    ret;

/*
 * __seh_set_registration: sets the registration handler to the given argument.  Linked
 *                         list of exception handlers must be maintained by the caller.
 *
 * Arguments: in __seh_buf*  the new exception handler registration structure.
 *
 */

___seh_set_registration@4:
    movl 4(%esp), %eax;
    movl %eax, %fs:0;
    xorl %eax, %eax;
    ret;


/*
 * __seh_pop_registration: like __seh_unregister, but does not release any resources associated
 *                         with the registration block.
 *
 * Return value: __seh_buf*  pointer to the new top of the handler stack.
 *
 */

___seh_pop_registration@0:
    movl %fs:0, %eax;
    movl 0(%eax), %eax;
    movl %eax, %fs:0;
    ret;
    
