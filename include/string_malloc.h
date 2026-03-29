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
 * Description: VTable-only public API for malloc-based string functions (ABI drop-in)
 *
 * USAGE:
 * This header provides standalone string utilities using system malloc/free.
 * No sigma.memory dependency. Suitable for sampling tools, embedded environments,
 * and utilities that cannot link sigma.memory.
 *
 * PUBLIC API: VTable-only (matches sigma.core pattern)
 * Use String.dupe(), String.concat(), etc. or StringBuilder.new(), etc.
 * Free functions are internal implementation details, not exposed in public header.
 *
 * ABI COMPATIBILITY:
 * VTable signatures match sigma.core/strings.h exactly. Code can swap between
 * malloc variant (this library) and controller variant (sigma.core.text.o) by
 * simply relinking — zero source changes required.
 *
 * Phase 0 Quick Win — part of ORCHESTRATION-BR-2603-q-or-001
 */
#pragma once

#include <stdio.h>
#include "sigma.core/types.h"

/* ==========================================================================
 * Type Definitions
 * ========================================================================== */

typedef struct string_builder_s *string_builder;

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
    string_builder (*new)(usize capacity);  /**< Initializes with a starting capacity. */
    string_builder (*snew)(string);         /**< Initializes a new string builder from string. */
    void (*append)(string_builder, string); /**< Appends a string to the buffer. */
    void (*appendf)(string_builder, string, ...); /**< Appends a formatted string. */
    void (*appendl)(string_builder, string);      /**< Appends a string followed by a newline. */
    void (*lappends)(string_builder, string);     /**< Appends a newline followed by the string. */
    void (*lappendf)(string_builder, string,
                     ...);              /**< Appends a newline followed by a formatted string. */
    void (*clear)(string_builder);      /**< Resets the buffer to empty. */
    string (*toString)(string_builder); /**< Returns a new string with current content. */
    void (*toStream)(string_builder, FILE *);   /**< Writes buffer contents to stream. */
    usize (*length)(string_builder);            /**< Returns current number of characters. */
    usize (*capacity)(string_builder);          /**< Returns total usable characters. */
    void (*setCapacity)(string_builder, usize); /**< Adjusts buffer capacity. */
    void (*dispose)(string_builder);            /**< Disposes the string builder. */
} sc_stringbuilder_i;

/* ==========================================================================
 * Global VTable Instances (ABI drop-in compatible with sigma.core)
 * ========================================================================== */

extern const sc_string_i String;
extern const sc_stringbuilder_i StringBuilder;
