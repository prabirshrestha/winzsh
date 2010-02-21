
/*******************************************************************************
 *                                                                             *
 * sehpp.h - C++ SEH interface.                                                *
 *                                                                             *
 * LIBSEH - Structured Exception Handling compatibility library.               *
 * Copyright (c) 2008 Tom Bramer < tjb at postpro dot net >                    *
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

/** @file sehpp.h
 *  Interface for LibSEH C++ bindings.
 */

#ifndef __LIBSEH_SEHPP_H__
#define __LIBSEH_SEHPP_H__

#ifdef __cplusplus

#include <stdexcept>
#include <stdint.h>

namespace seh
{
  /**
   * Initializes sehpp.  This should not be called directly.  Instead,
   * use the sehpp_initialize macro.
   *
   * @param buf   A pointer to a ___seh_buf that is
   *              located on the stack.
   */
  void __initialize(__seh_buf* buf);

  /**
   * @class exception
   * @brief The base of all sehpp exceptions.
   * @author tjbramer
   * @since 9-6-2008
   * @version 1.0
   */
  class exception : public std::exception
  {
    public:
      /**
       * Constructor
       *
       * @param code     The exception code.
       * @param address  The address in which the exception occurred.
       */
      explicit
      exception(uint32_t code, void* address);

      virtual
      ~exception() throw() { }

      /**
       * Gets a description of the exception.
       *
       * @return The description of the exception.
       */
      virtual const char*
      what();

    protected:
      /**
       * Set the exception message.
       * 
       * @param msg    The exception message.
       */
      virtual void set_msg(const std::string& msg);

      /**
       * Empty constructor.  Used for sub classes only.
       */
      explicit 
      exception() { }

    private:
      std::string msg_;    ///<  The internal message buffer.
  };

  /**
   * @class access_violation
   * @brief An exception that results from accessing memory
   *        that is unmapped or is protected from access by the
   *        current process.
   * @author tjbramer
   * @since 9-6-2008
   * @version 1.0
   */
  class access_violation : public exception
  {
    public:
      /**
       * Constructor
       *
       * @param address     The address in which the exception occurred.
       * @param accessaddr  The address that was accessed.
       */
      explicit
      access_violation(void* address, void* accessaddr);

      virtual
      ~access_violation() throw() { }
  };

}

// Macro for initializing sehpp
#define sehpp_initialize()              \
    ___seh_buf __sehpp_handler;         \
    seh::__initialize(&__sehpp_handler);  \


#endif /* __cplusplus */

#endif /* __LIBSEH_SEHPP_H__ */
