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
 * File: string_malloc.c
 * Description: Malloc variant of String functions for zero-dependency usage
 *
 * MEMORY ALLOCATION POLICY:
 * All allocations use system malloc/free/realloc ONLY. No sigma.memory
 * dependency, no controller overhead. Suitable for sampling tools, embedded
 * environments, and utilities that cannot link sigma.memory.
 *
 * Phase 0 Quick Win — extracted from sigma.core/src/strings.c
 * Part of ORCHESTRATION-BR-2603-q-or-001
 */

#include "string_malloc.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================================================================== */
/* Opaque Struct Definitions                                                */
/* ======================================================================== */

// Internal buffer structure
typedef struct string_buffer_s {
    char *data;      // Buffer data pointer
    usize capacity;  // Total allocated capacity
    usize length;    // Current used length
} string_buffer_s;

// StringBuilder structure
struct string_builder_s {
    string_buffer_s *buffer;  // Internal buffer
    char *str;                // Direct pointer to string data
    usize capacity;           // Current buffer capacity
    usize length;             // Current string length (excluding null terminator)
};

/* ======================================================================== */
/* Forward Declarations                                                     */
/* ======================================================================== */

// String API functions (not in public header - vtable-only API)
static usize string_length(string str);
static string string_copy(string src);
static string string_dupe(const char *cstr);
static string string_concat(string s1, string s2);
static string string_format(string fmt, ...);
static int string_compare(string s1, string s2);
static char *string_to_array(string s);
static void string_dispose(string s);

// StringBuilder API functions (not in public header - vtable-only API)
static string_builder stringbuilder_new(usize capacity);
static string_builder stringbuilder_snew(string str);
static void stringbuilder_append(string_builder sb, string str);
static void stringbuilder_appendf(string_builder sb, string fmt, ...);
static void stringbuilder_appendl(string_builder sb, string str);
static void stringbuilder_lappends(string_builder sb, string str);
static void stringbuilder_lappendf(string_builder sb, string fmt, ...);
static void stringbuilder_clear(string_builder sb);
static string stringbuilder_toString(string_builder sb);
static void stringbuilder_toStream(string_builder sb, FILE *stream);
static usize stringbuilder_length(string_builder sb);
static usize stringbuilder_capacity(string_builder sb);
static void stringbuilder_setCapacity(string_builder sb, usize new_capacity);
static void stringbuilder_dispose(string_builder sb);

// String helpers
static string string_alloc_copy(const char *src, usize len);

// Buffer helpers
static string_buffer_s *buffer_new(usize capacity);
static int buffer_ensure_capacity(string_buffer_s *buf, usize needed);
static char *buffer_data(string_buffer_s *buf);
static void buffer_dispose(string_buffer_s *buf);

/* ======================================================================== */
/* API Implementation                                                       */
/* ======================================================================== */

/**
 * @brief Returns the length of a string
 * @param str String to measure
 * @return Length of string, or 0 if NULL
 */
static usize string_length(const string str) { return str ? strlen(str) : 0; }

/**
 * @brief Creates a copy of a string
 * @param str String to copy
 * @return Newly allocated copy, or NULL on failure. Caller must dispose.
 */
static string string_copy(const string str) { return string_alloc_copy(str, string_length(str)); }

/**
 * @brief Creates a string from a C string
 * @param str C string to duplicate
 * @return Newly allocated string, or NULL on failure. Caller must dispose.
 */
static string string_dupe(const char *str) { return string_alloc_copy(str, str ? strlen(str) : 0); }

/**
 * @brief Concatenates two strings
 * @param str1 First string
 * @param str2 Second string
 * @return Newly allocated concatenated string, or NULL on failure. Caller must dispose.
 */
static string string_concat(const string str1, const string str2) {
    string result = NULL;
    usize len1, len2;

    if (!str1 || !str2) goto exit;

    len1 = strlen(str1);
    len2 = strlen(str2);
    result = malloc(len1 + len2 + 1);
    if (!result) goto exit;

    strcpy(result, str1);
    strcpy(result + len1, str2);

exit:
    return result;
}

/**
 * @brief Compares two strings for equality
 * @param str1 First string
 * @param str2 Second string
 * @return 0 if equal, negative if str1 < str2, positive if str1 > str2
 */
static int string_compare(const string str1, const string str2) {
    int result;

    if (str1 == str2) {
        result = 0;
        goto exit;
    }

    if (!str1 || !str2) {
        result = str1 ? 1 : -1;
        goto exit;
    }

    result = strcmp(str1, str2);

exit:
    return result;
}

/**
 * @brief Creates a formatted string using printf-style format specifiers
 * @param fmt Format string
 * @param ... Variable arguments for format string
 * @return Newly allocated formatted string, or NULL on failure. Caller must dispose.
 */
static string string_format(string fmt, ...) {
    string result = NULL;
    va_list args;
    int len;

    if (!fmt) goto exit;

    va_start(args, fmt);
    len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (len < 0) goto exit;

    result = malloc(len + 1);
    if (!result) goto exit;

    va_start(args, fmt);
    vsnprintf(result, len + 1, fmt, args);
    va_end(args);

exit:
    return result;
}

/**
 * @brief Disposes a string and frees its memory
 * @param str String to dispose
 */
static void string_dispose(string str) {
    if (str) free(str);
}

/**
 * @brief Returns a char array copy of the string
 * @param str String to convert
 * @return Newly allocated char array, or NULL on failure. Caller must dispose.
 */
static char *string_to_array(string str) { return string_alloc_copy(str, string_length(str)); }

/* ======================================================================== */
/* StringBuilder API Implementation                                         */
/* ======================================================================== */

/**
 * @brief Initializes a new string builder with the given capacity
 * @param capacity Initial capacity (0 defaults to 16)
 * @return New string builder, or NULL on failure. Caller must dispose.
 */
static string_builder stringbuilder_new(usize capacity) {
    string_builder sb = NULL;

    if (capacity == 0) capacity = 16;

    sb = malloc(sizeof(struct string_builder_s));
    if (!sb) goto exit;

    sb->buffer = buffer_new(capacity + 1);  // +1 for null terminator
    if (!sb->buffer) goto cleanup;

    sb->str = buffer_data(sb->buffer);
    sb->capacity = capacity;
    sb->length = 0;
    sb->str[0] = '\0';
    goto exit;

cleanup:
    free(sb);
    sb = NULL;

exit:
    return sb;
}

/**
 * @brief Initializes a new string builder from a string
 * @param str Initial string content (NULL creates empty builder)
 * @return New string builder, or NULL on failure. Caller must dispose.
 */
static string_builder stringbuilder_snew(string str) {
    string_builder sb = NULL;
    usize len;

    if (!str) {
        sb = stringbuilder_new(0);
        goto exit;
    }

    len = strlen(str);
    sb = stringbuilder_new(len + 1);
    if (!sb) goto exit;

    stringbuilder_append(sb, str);

exit:
    return sb;
}

/**
 * @brief Appends a string to the builder, resizing if necessary
 * @param sb String builder to append to
 * @param str String to append
 */
static void stringbuilder_append(string_builder sb, string str) {
    usize len, needed_capacity;

    if (!sb || !str) goto exit;

    len = strlen(str);
    needed_capacity = sb->length + len + 1;

    if (needed_capacity > sb->capacity) {
        if (buffer_ensure_capacity(sb->buffer, needed_capacity) != 0) goto exit;

        sb->str = buffer_data(sb->buffer);
        sb->capacity = sb->buffer->capacity - 1;
    }

    memcpy(sb->str + sb->length, str, len);
    sb->length += len;
    sb->str[sb->length] = '\0';

exit:
    return;
}

/**
 * @brief Appends a formatted string using printf-style format specifiers
 * @param sb String builder to append to
 * @param fmt Format string
 * @param ... Variable arguments for format string
 */
static void stringbuilder_appendf(string_builder sb, string fmt, ...) {
    va_list args, args_copy;
    int len;
    usize required_len, needed_capacity;

    if (!sb || !fmt) goto exit;

    va_start(args, fmt);
    va_copy(args_copy, args);
    len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (len < 0) goto cleanup;

    required_len = (usize)len;
    needed_capacity = sb->length + required_len + 1;

    if (needed_capacity > sb->capacity) {
        if (buffer_ensure_capacity(sb->buffer, needed_capacity) != 0) goto cleanup;

        sb->str = buffer_data(sb->buffer);
        sb->capacity = sb->buffer->capacity - 1;
    }

    vsnprintf(sb->str + sb->length, required_len + 1, fmt, args);
    sb->length += required_len;

cleanup:
    va_end(args);

exit:
    return;
}

/**
 * @brief Appends a string followed by a newline
 * @param sb String builder to append to
 * @param str String to append (NULL appends only newline)
 */
static void stringbuilder_appendl(string_builder sb, string str) {
    if (!sb) return;
    if (str) stringbuilder_append(sb, str);
    stringbuilder_append(sb, "\n");
}

/**
 * @brief Appends a newline followed by the string
 * @param sb String builder to append to
 * @param str String to append after newline
 */
static void stringbuilder_lappends(string_builder sb, string str) {
    if (!sb) return;
    stringbuilder_append(sb, "\n");
    if (str) stringbuilder_append(sb, str);
}

/**
 * @brief Appends a newline followed by a formatted string
 * @param sb String builder to append to
 * @param fmt Format string
 * @param ... Variable arguments for format string
 */
static void stringbuilder_lappendf(string_builder sb, string fmt, ...) {
    va_list args, args_copy;
    int len;
    usize required_len, needed_capacity;

    if (!sb || !fmt) goto exit;

    stringbuilder_append(sb, "\n");

    va_start(args, fmt);
    va_copy(args_copy, args);
    len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (len < 0) goto cleanup;

    required_len = (usize)len;
    needed_capacity = sb->length + required_len + 1;

    if (needed_capacity > sb->capacity) {
        if (buffer_ensure_capacity(sb->buffer, needed_capacity) != 0) goto cleanup;

        sb->str = buffer_data(sb->buffer);
        sb->capacity = sb->buffer->capacity - 1;
    }

    vsnprintf(sb->str + sb->length, required_len + 1, fmt, args);
    sb->length += required_len;

cleanup:
    va_end(args);

exit:
    return;
}

/**
 * @brief Resets the buffer to empty
 * @param sb String builder to clear
 */
static void stringbuilder_clear(string_builder sb) {
    if (!sb) return;
    sb->length = 0;
    sb->str[0] = '\0';
}

/**
 * @brief Returns a new string with the current content
 * @param sb String builder to convert
 * @return Newly allocated string, or NULL on failure. Caller must dispose.
 */
static string stringbuilder_toString(string_builder sb) {
    string result = NULL;
    usize len;

    if (!sb || !sb->str) goto exit;

    len = stringbuilder_length(sb);
    result = malloc(len + 1);
    if (!result) goto exit;

    strcpy(result, sb->str);

exit:
    return result;
}

/**
 * @brief Writes the buffer contents to the given stream
 * @param sb String builder to write
 * @param stream Output stream
 */
static void stringbuilder_toStream(string_builder sb, FILE *stream) {
    if (!sb || !sb->str || !stream) return;
    usize len = stringbuilder_length(sb);
    if (len > 0) fwrite(sb->str, 1, len, stream);
}

/**
 * @brief Returns the current number of characters in the builder
 * @param sb String builder to query
 * @return Number of characters, or 0 if NULL
 */
static usize stringbuilder_length(string_builder sb) { return sb ? sb->length : 0; }

/**
 * @brief Returns the total capacity of the builder
 * @param sb String builder to query
 * @return Total capacity in characters, or 0 if NULL
 */
static usize stringbuilder_capacity(string_builder sb) { return sb ? sb->capacity : 0; }

/**
 * @brief Adjusts the buffer capacity
 * @param sb String builder to resize
 * @param new_capacity New capacity (must be larger than current)
 */
static void stringbuilder_setCapacity(string_builder sb, usize new_capacity) {
    if (!sb || new_capacity <= sb->capacity) return;

    if (buffer_ensure_capacity(sb->buffer, new_capacity + 1) == 0) {
        sb->str = buffer_data(sb->buffer);
        sb->capacity = sb->buffer->capacity - 1;
    }
}

/**
 * @brief Disposes the string builder and frees all memory
 * @param sb String builder to dispose
 */
static void stringbuilder_dispose(string_builder sb) {
    if (!sb) return;
    if (sb->buffer) buffer_dispose(sb->buffer);
    free(sb);
}

/* ======================================================================== */
/* Helper Functions                                                         */
/* ======================================================================== */

/**
 * @brief Allocate and copy string data
 * @param src Source string
 * @param len Length to copy (not including null terminator)
 * @return Newly allocated string, or NULL on failure
 */
static string string_alloc_copy(const char *src, usize len) {
    string result = NULL;

    if (!src || len == 0) goto exit;

    result = malloc(len + 1);
    if (!result) goto exit;

    memcpy(result, src, len);
    result[len] = '\0';

exit:
    return result;
}

/**
 * @brief Create new buffer with specified capacity
 * @param capacity Initial capacity (minimum 16 bytes)
 * @return Pointer to new buffer, or NULL on failure
 */
static string_buffer_s *buffer_new(usize capacity) {
    string_buffer_s *buf = NULL;

    if (capacity == 0) capacity = 16;

    buf = malloc(sizeof(string_buffer_s));
    if (!buf) goto exit;

    buf->data = malloc(capacity);
    if (!buf->data) goto cleanup;

    buf->capacity = capacity;
    buf->length = 0;
    buf->data[0] = '\0';
    goto exit;

cleanup:
    free(buf);
    buf = NULL;

exit:
    return buf;
}

/**
 * @brief Ensure buffer has at least the specified capacity
 * @param buf Buffer to resize
 * @param needed Required capacity
 * @return 0 on success, -1 on failure
 */
static int buffer_ensure_capacity(string_buffer_s *buf, usize needed) {
    int result = -1;
    char *new_data;
    usize new_capacity;

    if (!buf || needed <= buf->capacity) {
        result = 0;
        goto exit;
    }

    new_capacity = needed;
    new_data = realloc(buf->data, new_capacity);
    if (!new_data) goto exit;

    buf->data = new_data;
    buf->capacity = new_capacity;
    result = 0;

exit:
    return result;
}

/**
 * @brief Get direct pointer to buffer data
 * @param buf Buffer to query
 * @return Pointer to buffer data, or NULL if buffer is NULL
 */
static char *buffer_data(string_buffer_s *buf) { return buf ? buf->data : NULL; }

/**
 * @brief Dispose buffer and its data
 * @param buf Buffer to dispose
 */
static void buffer_dispose(string_buffer_s *buf) {
    if (!buf) return;
    if (buf->data) free(buf->data);
    free(buf);
}

/* ======================================================================== */
/* Global vtable instances (ABI-compatible with sigma.core)                */
/* ======================================================================== */

const sc_string_i String = {.length = string_length,
                            .copy = string_copy,
                            .dupe = string_dupe,
                            .concat = string_concat,
                            .format = string_format,
                            .compare = string_compare,
                            .to_array = string_to_array,
                            .dispose = string_dispose};

const sc_stringbuilder_i StringBuilder = {.new = stringbuilder_new,
                                          .snew = stringbuilder_snew,
                                          .append = stringbuilder_append,
                                          .appendf = stringbuilder_appendf,
                                          .appendl = stringbuilder_appendl,
                                          .lappends = stringbuilder_lappends,
                                          .lappendf = stringbuilder_lappendf,
                                          .clear = stringbuilder_clear,
                                          .toString = stringbuilder_toString,
                                          .toStream = stringbuilder_toStream,
                                          .length = stringbuilder_length,
                                          .capacity = stringbuilder_capacity,
                                          .setCapacity = stringbuilder_setCapacity,
                                          .dispose = stringbuilder_dispose};
