///////////////////////////////////////////////////////////////////////////////
// \author (c) Marco Paland (info@paland.com)
//             2014-2019, PALANDesign Hannover, Germany
//
// \license The MIT License (MIT)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// \brief Tiny printf, sprintf and snprintf implementation, optimized for speed on
//        embedded systems with a very limited resources.
//        Use this instead of bloated standard/newlib printf.
//        These routines are thread safe and reentrant.
//
//
// ANOS NOTE: DO NOT USE THIS IN INTERRUPT CONTEXTS, IT WILL DEADLOCK.
//
///////////////////////////////////////////////////////////////////////////////

// clang-format Language: C

#ifndef __ANOS_KERNEL_KPRINTF_H
#define __ANOS_KERNEL_KPRINTF_H

#include <stdarg.h>
#include <stddef.h>

#define PRINTF_DISABLE_SUPPORT_FLOAT // Disabled until we support SSE

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Output a character to a custom device like UART, used by the printf() function
 * This function is declared here only. You have to write your custom implementation somewhere
 * \param character Character to output
 */
void _putchar(char character);

/**
 * Tiny printf implementation
 * You have to implement _putchar if you use printf()
 * To avoid conflicts with the regular printf() API it is overridden by macro defines
 * and internal underscore-appended functions like printf_() are used
 * \param format A string that specifies the format of the output
 * \return The number of characters that are written into the array, not counting the terminating null character
 */
#ifdef UNIT_TESTS
#ifdef printf
#define kprintf printf
#else
#define kprintf(...)
#endif
#else
#define kprintf printf_
#ifdef __GNUC__
__attribute__ ((format (__printf__, 1, 2)))
#endif
int printf_(const char* format, ...);
#endif

/**
 * Tiny sprintf implementation
 * Due to security reasons (buffer overflow) YOU SHOULD CONSIDER USING (V)SNPRINTF INSTEAD!
 * \param buffer A pointer to the buffer where to store the formatted string. MUST be big enough to store the output!
 * \param format A string that specifies the format of the output
 * \return The number of characters that are WRITTEN into the buffer, not counting the terminating null character
 */
#define sprintf sprintf_
#ifdef __GNUC__
__attribute__ ((format (__printf__, 2, 3)))
#endif
int sprintf_(char* buffer, const char* format, ...);

/**
 * Tiny snprintf/vsnprintf implementation
 * \param buffer A pointer to the buffer where to store the formatted string
 * \param count The maximum number of characters to store in the buffer, including a terminating null character
 * \param format A string that specifies the format of the output
 * \param va A value identifying a variable arguments list
 * \return The number of characters that COULD have been written into the buffer, not counting the terminating
 *         null character. A value equal or larger than count indicates truncation. Only when the returned value
 *         is non-negative and less than count, the string has been completely written.
 */
#define snprintf snprintf_
#define vsnprintf vsnprintf_
#ifdef __GNUC__
__attribute__ ((format (__printf__, 3, 4)))
#endif
int  snprintf_(char* buffer, size_t count, const char* format, ...);

#ifdef __GNUC__
__attribute__ ((format (__printf__, 3, 0)))
#endif
int vsnprintf_(char* buffer, size_t count, const char* format, va_list va);

/**
 * Tiny vprintf implementation
 * \param format A string that specifies the format of the output
 * \param va A value identifying a variable arguments list
 * \return The number of characters that are WRITTEN into the buffer, not counting the terminating null character
 */
#define vprintf vprintf_
#ifdef __GNUC__
__attribute__ ((format (__printf__, 1, 0)))
#endif
int vprintf_(const char* format, va_list va);

/**
 * printf with output function
 * You may use this as dynamic alternative to printf() with its fixed _putchar() output
 * \param out An output function which takes one character and an argument pointer
 * \param arg An argument pointer for user data passed to output function
 * \param format A string that specifies the format of the output
 * \return The number of characters that are sent to the output function, not counting the terminating null character
 */
#ifdef __GNUC__
__attribute__ ((format (__printf__, 3, 4)))
#endif
int fctprintf(void (*out)(char character, void* arg), void* arg, const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif // __ANOS_KERNEL_KPRINTF_H
