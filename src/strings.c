/*
 * Sigma.Core
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
 * File: strings.c
 * Description: Implementation of string utilities in Sigma.Core
 *
 * MEMORY ALLOCATION POLICY:
 * All memory allocations use Allocator interface from sigma.core/alloc.h.
 * This allows sigma.text to work standalone (with minimal allocator) or
 * with full sigma.memory system when available.
 *
 * CRITICAL: Never use stdlib allocation functions directly (malloc, free,
 * strdup, etc.). All allocations MUST go through Allocator interface.
 * See include/internal/stdlib_policy.h for safe replacement functions.
 */

#include <sigma.core/allocator.h>
#include <sigma.core/strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================================================================== */
/* Allocator dispatch state                                                 */
/* ======================================================================== */

/* Per-interface override hooks.  NULL = use global Allocator. */
static sc_alloc_use_t *s_string_use = NULL;
static sc_alloc_use_t *s_sb_use = NULL;

/* Dispatch helpers — fall back to Allocator when hook or field is NULL. */
static void *tx_alloc(sc_alloc_use_t *use, usize size) {
    if (use && use->alloc) return use->alloc(size);
    return Allocator.alloc(size);
}
static void tx_free(sc_alloc_use_t *use, void *ptr) {
    if (use && use->release) {
        use->release(ptr);
        return;
    }
    Allocator.free(ptr);
}
static void *tx_realloc(sc_alloc_use_t *use, void *ptr, usize size) {
    if (use && use->resize) return use->resize(ptr, size);
    return Allocator.realloc(ptr, size);
}

/* ======================================================================== */
/* Opaque Struct Definitions                                                */
/* ======================================================================== */

// Internal buffer structure maintaining farray-compatible memory layout
typedef struct string_buffer_s {
    char *data;      // Buffer data pointer
    usize capacity;  // Total allocated capacity
    usize length;    // Current used length
} string_buffer_s;

// StringBuilder structure
struct string_builder_s {
    string_buffer_s *buffer;  // Internal buffer (farray-compatible layout)
    char *str;                // Direct pointer to string data for efficiency
    usize capacity;           // Current buffer capacity
    usize length;             // Current string length (excluding null terminator)
};

/* ======================================================================== */
/* Forward Declarations                                                     */
/* ======================================================================== */

// API functions
usize string_length(const string str);
string string_copy(const string str);
string string_duplicate(const char *str);
string string_concat(const string str1, const string str2);
int string_compare(const string str1, const string str2);
string string_format(const string format, ...);
void string_dispose(string str);
char *string_to_array(string str);

string_builder stringbuilder_new(usize capacity);
string_builder stringbuilder_from_string(string str);
void stringbuilder_append(string_builder sb, string str);
void stringbuilder_appendf(string_builder sb, string format, ...);
void stringbuilder_appendl(string_builder sb, string str);
void stringbuilder_lappends(string_builder sb, string str);
void stringbuilder_lappendf(string_builder sb, string format, ...);
void stringbuilder_clear(string_builder sb);
string stringbuilder_to_string(string_builder sb);
void stringbuilder_to_stream(string_builder sb, FILE *stream);
usize stringbuilder_length(string_builder sb);
usize stringbuilder_capacity(string_builder sb);
void stringbuilder_set_capacity(string_builder sb, usize new_capacity);
void stringbuilder_dispose(string_builder sb);

// Helper/utility functions
static string string_alloc_copy(const char *src, usize len);
static string_buffer_s *buffer_new(usize capacity);
static int buffer_ensure_capacity(string_buffer_s *buf, usize needed);
static char *buffer_data(string_buffer_s *buf);
static void buffer_dispose(string_buffer_s *buf);

// Allocator-use setters
static void string_set_alloc_use(sc_alloc_use_t *use);
static void stringbuilder_set_alloc_use(sc_alloc_use_t *use);

/* ======================================================================== */
/* String API Functions                                                     */
/* ======================================================================== */

/**
 * @brief Returns the length of a string
 * @param str String to measure
 * @return Length of string in characters, or 0 if NULL
 */
usize string_length(const string str) { return str ? strlen(str) : 0; }

/**
 * @brief Creates a copy of a string
 * @param str String to copy
 * @return Newly allocated copy, or NULL on failure. Caller must dispose.
 */
string string_copy(const string str) { return string_alloc_copy(str, string_length(str)); }

/**
 * @brief Duplicates a C string
 * @param str C string to duplicate
 * @return Newly allocated string, or NULL on failure. Caller must dispose.
 */
string string_duplicate(const char *str) { return string_alloc_copy(str, str ? strlen(str) : 0); }

/**
 * @brief Concatenates two strings
 * @param str1 First string
 * @param str2 Second string
 * @return Newly allocated concatenated string, or NULL on failure. Caller must dispose.
 */
string string_concat(const string str1, const string str2) {
    string result = NULL;
    usize len1, len2;

    if (!str1 || !str2) goto exit;

    len1 = strlen(str1);
    len2 = strlen(str2);
    result = tx_alloc(s_string_use, len1 + len2 + 1);
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
int string_compare(const string str1, const string str2) {
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
 * @param format Format string
 * @param ... Variable arguments for format string
 * @return Newly allocated formatted string, or NULL on failure. Caller must dispose.
 */
string string_format(const string format, ...) {
    string result = NULL;
    va_list args;
    int len;

    if (!format) goto exit;

    va_start(args, format);
    len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (len < 0) goto exit;

    result = tx_alloc(s_string_use, len + 1);
    if (!result) goto exit;

    va_start(args, format);
    vsnprintf(result, len + 1, format, args);
    va_end(args);

exit:
    return result;
}

/**
 * @brief Disposes a string and frees its memory
 * @param str String to dispose
 */
void string_dispose(string str) {
    if (str) tx_free(s_string_use, str);
}

/**
 * @brief Returns a char array copy of the string
 * @param str String to convert
 * @return Newly allocated char array, or NULL on failure. Caller must dispose.
 */
char *string_to_array(string str) { return string_alloc_copy(str, string_length(str)); }

const sc_string_i String = {
    .length = string_length,
    .copy = string_copy,
    .dupe = string_duplicate,
    .concat = string_concat,
    .compare = string_compare,
    .format = string_format,
    .to_array = string_to_array,
    .dispose = string_dispose,
    .alloc_use = string_set_alloc_use,
};

/* ======================================================================== */
/* StringBuilder API Functions                                              */
/* ======================================================================== */

/**
 * @brief Initializes a new string builder with the given capacity
 * @param capacity Initial capacity (0 defaults to 16)
 * @return New string builder, or NULL on failure. Caller must dispose.
 */
string_builder stringbuilder_new(usize capacity) {
    string_builder sb = NULL;

    if (capacity == 0) capacity = 16;

    sb = tx_alloc(s_sb_use, sizeof(struct string_builder_s));
    if (!sb) goto exit;

    sb->buffer = buffer_new(capacity + 1);  // +1 for null terminator
    if (!sb->buffer) goto cleanup;

    sb->str = buffer_data(sb->buffer);
    sb->capacity = capacity;
    sb->length = 0;
    sb->str[0] = '\0';
    goto exit;

cleanup:
    tx_free(s_sb_use, sb);
    sb = NULL;

exit:
    return sb;
}

/**
 * @brief Initializes a new string builder from a string
 * @param str Initial string content (NULL creates empty builder)
 * @return New string builder, or NULL on failure. Caller must dispose.
 */
string_builder stringbuilder_from_string(string str) {
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
void stringbuilder_append(string_builder sb, string str) {
    usize len, needed_capacity;

    if (!sb || !str) goto exit;

    len = strlen(str);
    needed_capacity = sb->length + len + 1;

    if (needed_capacity > sb->capacity) {
        if (buffer_ensure_capacity(sb->buffer, needed_capacity) != OK) goto exit;

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
 * @param format Format string
 * @param ... Variable arguments for format string
 */
void stringbuilder_appendf(string_builder sb, string format, ...) {
    va_list args, args_copy;
    int len;
    usize required_len, needed_capacity;

    if (!sb || !format) goto exit;

    va_start(args, format);
    va_copy(args_copy, args);
    len = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (len < 0) goto cleanup;

    required_len = (usize)len;
    needed_capacity = sb->length + required_len + 1;

    if (needed_capacity > sb->capacity) {
        if (buffer_ensure_capacity(sb->buffer, needed_capacity) != OK) goto cleanup;

        sb->str = buffer_data(sb->buffer);
        sb->capacity = sb->buffer->capacity - 1;
    }

    vsnprintf(sb->str + sb->length, required_len + 1, format, args);
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
void stringbuilder_appendl(string_builder sb, string str) {
    if (!sb) return;
    if (str) stringbuilder_append(sb, str);
    stringbuilder_append(sb, "\n");
}

/**
 * @brief Appends a newline followed by the string
 * @param sb String builder to append to
 * @param str String to append after newline
 */
void stringbuilder_lappends(string_builder sb, string str) {
    if (!sb) return;
    stringbuilder_append(sb, "\n");
    if (str) stringbuilder_append(sb, str);
}

/**
 * @brief Appends a newline followed by a formatted string
 * @param sb String builder to append to
 * @param format Format string
 * @param ... Variable arguments for format string
 */
void stringbuilder_lappendf(string_builder sb, string format, ...) {
    va_list args, args_copy;
    int len;
    usize required_len, needed_capacity;

    if (!sb || !format) goto exit;

    stringbuilder_append(sb, "\n");

    va_start(args, format);
    va_copy(args_copy, args);
    len = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (len < 0) goto cleanup;

    required_len = (usize)len;
    needed_capacity = sb->length + required_len + 1;

    if (needed_capacity > sb->capacity) {
        if (buffer_ensure_capacity(sb->buffer, needed_capacity) != OK) goto cleanup;

        sb->str = buffer_data(sb->buffer);
        sb->capacity = sb->buffer->capacity - 1;
    }

    vsnprintf(sb->str + sb->length, required_len + 1, format, args);
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
void stringbuilder_clear(string_builder sb) {
    if (!sb) return;
    sb->length = 0;
    sb->str[0] = '\0';
}

/**
 * @brief Returns a new string with the current content
 * @param sb String builder to convert
 * @return Newly allocated string, or NULL on failure. Caller must dispose.
 */
string stringbuilder_to_string(string_builder sb) {
    string result = NULL;
    usize len;

    if (!sb || !sb->str) goto exit;

    len = stringbuilder_length(sb);
    result = tx_alloc(s_sb_use, len + 1);
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
void stringbuilder_to_stream(string_builder sb, FILE *stream) {
    if (!sb || !sb->str || !stream) return;
    usize len = stringbuilder_length(sb);
    if (len > 0) fwrite(sb->str, 1, len, stream);
}

/**
 * @brief Returns the current number of characters in the builder
 * @param sb String builder to query
 * @return Number of characters, or 0 if NULL
 */
usize stringbuilder_length(string_builder sb) { return sb ? sb->length : 0; }

/**
 * @brief Returns the total capacity of the builder
 * @param sb String builder to query
 * @return Total capacity in characters, or 0 if NULL
 */
usize stringbuilder_capacity(string_builder sb) { return sb ? sb->capacity : 0; }

/**
 * @brief Adjusts the buffer capacity
 * @param sb String builder to resize
 * @param new_capacity New capacity (must be larger than current)
 */
void stringbuilder_set_capacity(string_builder sb, usize new_capacity) {
    if (!sb || new_capacity <= sb->capacity) return;

    if (buffer_ensure_capacity(sb->buffer, new_capacity + 1) == OK) {
        sb->str = buffer_data(sb->buffer);
        sb->capacity = sb->buffer->capacity - 1;
    }
}

/**
 * @brief Disposes the string builder and frees all memory
 * @param sb String builder to dispose
 */
void stringbuilder_dispose(string_builder sb) {
    if (!sb) return;
    if (sb->buffer) buffer_dispose(sb->buffer);
    tx_free(s_sb_use, sb);
}

const sc_stringbuilder_i StringBuilder = {
    .new = stringbuilder_new,
    .snew = stringbuilder_from_string,
    .append = stringbuilder_append,
    .appendf = stringbuilder_appendf,
    .appendl = stringbuilder_appendl,
    .lappends = stringbuilder_lappends,
    .lappendf = stringbuilder_lappendf,
    .clear = stringbuilder_clear,
    .toString = stringbuilder_to_string,
    .toStream = stringbuilder_to_stream,
    .length = stringbuilder_length,
    .capacity = stringbuilder_capacity,
    .setCapacity = stringbuilder_set_capacity,
    .dispose = stringbuilder_dispose,
    .alloc_use = stringbuilder_set_alloc_use,
};

/* ======================================================================== */
/* Helper/Utility Function Implementations                                  */
/* ======================================================================== */

/**
 * @brief Allocate and copy a string of given length using Allocator
 * @param src Source string to copy
 * @param len Length to copy (not including null terminator)
 * @return Newly allocated string, or NULL on failure
 *
 * This is the internal safe replacement for strdup/strndup. All string
 * duplication in sigma.text uses this function to ensure Allocator
 * compatibility. NEVER use strdup() directly - it uses malloc().
 */
static string string_alloc_copy(const char *src, usize len) {
    string result = NULL;

    if (!src || len == 0) goto exit;

    result = tx_alloc(s_string_use, len + 1);
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

    buf = tx_alloc(s_sb_use, sizeof(string_buffer_s));
    if (!buf) goto exit;

    buf->data = tx_alloc(s_sb_use, capacity);
    if (!buf->data) goto cleanup;

    buf->capacity = capacity;
    buf->length = 0;
    buf->data[0] = '\0';
    goto exit;

cleanup:
    tx_free(s_sb_use, buf);
    buf = NULL;

exit:
    return buf;
}

/**
 * @brief Ensure buffer has at least the specified capacity
 * @param buf Buffer to resize
 * @param needed Required capacity
 * @return OK on success, ERR on failure
 */
static int buffer_ensure_capacity(string_buffer_s *buf, usize needed) {
    int result = ERR;
    char *new_data;
    usize new_capacity;

    if (!buf || needed <= buf->capacity) {
        result = OK;
        goto exit;
    }

    new_capacity = needed;
    new_data = tx_realloc(s_sb_use, buf->data, new_capacity);
    if (!new_data) goto exit;

    buf->data = new_data;
    buf->capacity = new_capacity;
    result = OK;

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
    if (buf->data) tx_free(s_sb_use, buf->data);
    tx_free(s_sb_use, buf);
}

/* ======================================================================== */
/* Allocator-use setters                                                    */
/* ======================================================================== */

static void string_set_alloc_use(sc_alloc_use_t *use) { s_string_use = use; }

static void stringbuilder_set_alloc_use(sc_alloc_use_t *use) { s_sb_use = use; }
