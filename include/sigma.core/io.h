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
 * File: sigma.core/io.h
 * Description: File and Directory interfaces for Sigma.Core
 *
 *   File   — full lifecycle: open, read, write, seek, flush, close, plus path
 *             queries (exists, size, remove).  All returned strings are allocated
 *             via Allocator (controller scope set by entry point); the caller
 *             must dispose them with File.dispose().
 *
 *   Directory — query-only: exists, is_dir, list, walk.  No mkdir/rmdir/rename.
 *               list() returns a NULL-terminated string[] allocated via Allocator;
 *               tear down with Directory.dispose_list().
 *               walk() is zero-allocation: fires a callback per entry.
 */
#pragma once

#include <sigma.core/allocator.h>
#include <sigma.core/types.h>
#include <stdio.h>

/* ── Opaque file handle ──────────────────────────────────────────────────── */
typedef struct sc_file_s *sc_file_t;

/* ── Directory walk visitor ──────────────────────────────────────────────── */
/**
 * @brief Callback invoked by Directory.walk() for each entry found.
 *
 * @param path      Absolute or relative path of the entry (not allocated —
 *                  valid only for the duration of this call).
 * @param name      Entry name (filename or dirname, no path prefix).
 * @param is_dir    true if the entry is a directory, false if a regular file.
 * @param ctx       Caller-supplied context pointer passed through unchanged.
 * @return          Return true to continue walking; false to stop early.
 */
typedef bool (*sc_dir_visitor_fn)(const string path, const string name, bool is_dir, void *ctx);

/* ── File interface ──────────────────────────────────────────────────────── */
typedef struct sc_file_i {
    /**
     * @brief Open a file.  Mode follows fopen conventions ("r", "w", "a", etc.).
     * @return Opaque handle, or NULL on failure (check errno).
     */
    sc_file_t (*open)(const string path, const string mode);

    /**
     * @brief Close an open file handle and release its resources.
     */
    void (*close)(sc_file_t f);

    /**
     * @brief Read up to @p limit bytes from @p f.
     *
     * Allocates a buffer via Allocator.  The returned string is NUL-terminated.
     * *out_size receives the number of bytes read (excluding the NUL).
     * Returns NULL on error or empty read.
     * Caller must dispose the result with File.dispose().
     */
    string (*read)(sc_file_t f, usize limit, usize *out_size);

    /**
     * @brief Read one '\n'-terminated line from @p f.
     *
     * Allocates via Allocator.  The trailing newline is stripped.
     * Returns NULL at EOF or on error.
     * Caller must dispose with File.dispose().
     */
    string (*read_line)(sc_file_t f);

    /**
     * @brief Write @p data (length @p size) to @p f.
     * @return Number of bytes written, or -1 on error.
     */
    integer (*write)(sc_file_t f, const string data, usize size);

    /**
     * @brief Append @p data to @p f (seeks to end first).
     * @return Number of bytes written, or -1 on error.
     */
    integer (*append)(sc_file_t f, const string data, usize size);

    /**
     * @brief Seek to @p offset from @p whence (SEEK_SET / SEEK_CUR / SEEK_END).
     * @return 0 on success, -1 on error.
     */
    integer (*seek)(sc_file_t f, integer offset, int whence);

    /**
     * @brief Flush pending writes for @p f.
     * @return 0 on success, -1 on error.
     */
    integer (*flush)(sc_file_t f);

    /**
     * @brief Return the byte size of the file at @p path, or -1 on error.
     */
    integer (*size)(const string path);

    /**
     * @brief Return true if a regular file at @p path exists.
     */
    bool (*exists)(const string path);

    /**
     * @brief Delete the file at @p path.
     * @return 0 on success, -1 on error.
     */
    integer (*remove)(const string path);

    /**
     * @brief Dispose a string returned by File.read() or File.read_line().
     */
    void (*dispose)(string s);
} sc_file_i;

/* ── Directory interface ─────────────────────────────────────────────────── */
typedef struct sc_dir_i {
    /**
     * @brief Return true if @p path exists (file or directory).
     */
    bool (*exists)(const string path);

    /**
     * @brief Return true if @p path exists and is a directory.
     */
    bool (*is_dir)(const string path);

    /**
     * @brief List the entries of the directory at @p path.
     *
     * Returns a NULL-terminated array of allocated strings (entry names only,
     * no path prefix).  The caller must free the result with
     * Directory.dispose_list().  Returns NULL if the path is not a directory
     * or on allocation failure.
     */
    string *(*list)(const string path);

    /**
     * @brief Walk the directory tree rooted at @p path.
     *
     * Calls @p visitor for each entry.  If @p recursive is true, descends into
     * subdirectories.  Walking stops early if the visitor returns false.
     * Zero allocation — path/name strings are stack-composed per entry.
     */
    void (*walk)(const string path, bool recursive, sc_dir_visitor_fn visitor, void *ctx);

    /**
     * @brief Dispose the array returned by Directory.list().
     *
     * Frees every entry string and then the array pointer itself.
     */
    void (*dispose_list)(string *list);
} sc_dir_i;

/* ── Global instances ────────────────────────────────────────────────────── */
extern const sc_file_i File;
extern const sc_dir_i Directory;
