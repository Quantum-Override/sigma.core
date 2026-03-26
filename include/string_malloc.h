/*
 * SigmaCore
 * Copyright (c) 2026 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ----------------------------------------------
 * File: string_malloc.h
 * Description: Public API for malloc-based string functions (ABI-compatible drop-in)
 *
 * USAGE:
 * This header provides standalone string utilities using system malloc/free.
 * No sigma.memory dependency. Suitable for sampling tools, embedded environments,
 * and utilities that cannot link sigma.memory.
 *
 * ABI COMPATIBILITY:
 * Function signatures match sigma.core/strings.h exactly. Code can swap between
 * malloc variant (this library) and controller variant (sigma.core.text.o) without changes.
 *
 * All functions returning string allocate memory — caller must dispose.
 *
 * Phase 0 Quick Win — part of ORCHESTRATION-BR-2603-q-or-001
 */
#pragma once

#include <stdio.h>
#include "sigma.core/types.h"

/* ======================================================================== */
/* Type Definitions                                                         */
/* ======================================================================== */

typedef struct string_builder_s *string_builder;

/* ==========================================================================
 * String Free Functions (ABI-compatible with sigma.core/strings.h)
 * ========================================================================== */

/**
 * @brief Returns the length of a string
 * @param s String to measure
 * @return Length of string, or 0 if NULL
 */
usize string_length(string s);

/**
 * @brief Creates a copy of a string
 * @param src String to copy
 * @return Newly allocated copy, or NULL on failure. Caller must dispose.
 */
string string_copy(string src);

/**
 * @brief Duplicates a C string
 * @param cstr C string to duplicate
 * @return Newly allocated string, or NULL on failure. Caller must dispose.
 */
string string_dupe(const char *cstr);

/**
 * @brief Concatenates two strings
 * @param s1 First string
 * @param s2 Second string
 * @return Newly allocated concatenated string, or NULL on failure. Caller must dispose.
 */
string string_concat(string s1, string s2);

/**
 * @brief Creates a formatted string using printf-style format specifiers
 * @param fmt Format string (as string type, not const char*)
 * @param ... Variable arguments for format string
 * @return Newly allocated formatted string, or NULL on failure. Caller must dispose.
 */
string string_format(string fmt, ...);

/**
 * @brief Compares two strings for equality
 * @param s1 First string
 * @param s2 Second string
 * @return 0 if equal, negative if s1 < s2, positive if s1 > s2
 */
int string_compare(string s1, string s2);

/**
 * @brief Returns a char array copy of the string
 * @param s String to convert
 * @return Newly allocated char array, or NULL on failure. Caller must free with free().
 */
char *string_to_array(string s);

/**
 * @brief Disposes a string and frees its memory
 * @param s String to dispose
 */
void string_dispose(string s);

/* ==========================================================================
 * StringBuilder Free Functions (ABI-compatible with sigma.core/strings.h)
 * ========================================================================== */

/**
 * @brief Creates a new string builder with specified initial capacity
 * @param capacity Initial capacity (0 defaults to 16)
 * @return New string builder, or NULL on failure. Caller must dispose.
 */
string_builder stringbuilder_new(usize capacity);

/**
 * @brief Creates a new string builder from an existing string
 * @param str Initial string content (NULL creates empty builder)
 * @return New string builder, or NULL on failure. Caller must dispose.
 */
string_builder stringbuilder_snew(string str);

/**
 * @brief Appends a string to the builder
 * @param sb String builder
 * @param str String to append
 */
void stringbuilder_append(string_builder sb, string str);

/**
 * @brief Appends a formatted string using printf-style format specifiers
 * @param sb String builder
 * @param fmt Format string (as string type, not const char*)
 * @param ... Variable arguments
 */
void stringbuilder_appendf(string_builder sb, string fmt, ...);

/**
 * @brief Appends a string followed by a newline
 * @param sb String builder
 * @param str String to append (NULL appends only newline)
 */
void stringbuilder_appendl(string_builder sb, string str);

/**
 * @brief Appends a newline followed by a string
 * @param sb String builder
 * @param str String to append after newline
 */
void stringbuilder_lappends(string_builder sb, string str);

/**
 * @brief Appends a newline followed by a formatted string
 * @param sb String builder
 * @param fmt Format string (as string type, not const char*)
 * @param ... Variable arguments
 */
void stringbuilder_lappendf(string_builder sb, string fmt, ...);

/**
 * @brief Clears the builder content (resets to empty)
 * @param sb String builder to clear
 */
void stringbuilder_clear(string_builder sb);

/**
 * @brief Converts builder content to a new string
 * @param sb String builder
 * @return Newly allocated string, or NULL on failure. Caller must dispose.
 */
string stringbuilder_toString(string_builder sb);

/**
 * @brief Writes builder content to a stream
 * @param sb String builder
 * @param stream Output stream (e.g., stdout, stderr, or file)
 */
void stringbuilder_toStream(string_builder sb, FILE *stream);

/**
 * @brief Returns the current length of the builder content
 * @param sb String builder
 * @return Number of characters, or 0 if NULL
 */
usize stringbuilder_length(string_builder sb);

/**
 * @brief Returns the current capacity of the builder
 * @param sb String builder
 * @return Total capacity in characters, or 0 if NULL
 */
usize stringbuilder_capacity(string_builder sb);

/**
 * @brief Sets builder capacity
 * @param sb String builder
 * @param new_capacity New capacity
 */
void stringbuilder_setCapacity(string_builder sb, usize new_capacity);

/**
 * @brief Disposes the string builder and frees all memory
 * @param sb String builder to dispose
 */
void stringbuilder_dispose(string_builder sb);

/* ==========================================================================
 * VTable Interfaces (ABI-compatible with sigma.core/strings.h)
 * ========================================================================== */

/**
 * @brief String interface vtable
 * Matches sc_string_i from sigma.core/strings.h exactly (minus alloc_use hook)
 */
typedef struct sc_string_i {
    usize (*length)(string);          /**< Returns the length of a string. */
    string (*copy)(string);           /**< Creates a copy of a string. */
    string (*dupe)(const char *);     /**< Duplicates a string. */
    string (*concat)(string, string); /**< Returns a concatenated string. */
    string (*format)(string, ...);    /**< Returns a formatted string. */
    int (*compare)(string, string);   /**< Compares two strings for equality. */
    char *(*to_array)(string);        /**< Returns a char array copy of the string. */
    void (*dispose)(string);          /**< Disposes the string allocation. */
} sc_string_i;

/**
 * @brief StringBuilder interface vtable
 * Matches sc_stringbuilder_i from sigma.core/strings.h exactly (minus alloc_use hook)
 */
typedef struct sc_stringbuilder_i {
    string_builder (*new)(usize capacity); /**< Initializes with a starting capacity. */
    string_builder (*snew)(string); /**< Initializes a new string builder from string. */
    void (*append)(string_builder, string); /**< Appends a string to the buffer. */
    void (*appendf)(string_builder, string, ...); /**< Appends a formatted string. */
    void (*appendl)(string_builder, string); /**< Appends a string followed by a newline. */
    void (*lappends)(string_builder, string); /**< Appends a newline followed by the string. */
    void (*lappendf)(string_builder, string, ...); /**< Appends a newline followed by a formatted string. */
    void (*clear)(string_builder); /**< Resets the buffer to empty. */
    string (*toString)(string_builder); /**< Returns a new string with current content. */
    void (*toStream)(string_builder, FILE *); /**< Writes buffer contents to stream. */
    usize (*length)(string_builder); /**< Returns current number of characters. */
    usize (*capacity)(string_builder); /**< Returns total usable characters. */
    void (*setCapacity)(string_builder, usize); /**< Adjusts buffer capacity. */
    void (*dispose)(string_builder); /**< Disposes the string builder. */
} sc_stringbuilder_i;

/* Global vtable instances (ABI drop-in compatible with sigma.core) */
extern const sc_string_i String;
extern const sc_stringbuilder_i StringBuilder;
