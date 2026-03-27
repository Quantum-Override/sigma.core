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
 * File: io.c
 * Description: Implementation of File and Directory interfaces for Sigma.Core
 *
 * All allocations use Allocator.* directly (controller scope set by entry point).
 *
 * POSIX backing: fopen/fclose/fread/fwrite/fseek/fflush/unlink/stat/
 *                opendir/readdir/closedir
 */

#include <dirent.h>
#include <errno.h>
#include <sigma.core/allocator.h>
#include <sigma.core/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ======================================================================== */
/* Opaque struct definitions                                                */
/* ======================================================================== */

struct sc_file_s {
    FILE *fp;
};

/* ======================================================================== */
/* Forward declarations — File API                                          */
/* ======================================================================== */

static sc_file_t file_open(const string path, const string mode);
static void file_close(sc_file_t f);
static string file_read(sc_file_t f, usize limit, usize *out_size);
static string file_read_line(sc_file_t f);
static integer file_write(sc_file_t f, const string data, usize size);
static integer file_append(sc_file_t f, const string data, usize size);
static integer file_seek(sc_file_t f, integer offset, int whence);
static integer file_flush(sc_file_t f);
static integer file_size(const string path);
static bool file_exists(const string path);
static integer file_remove(const string path);
static void file_dispose(string s);

/* ======================================================================== */
/* Forward declarations — Directory API                                     */
/* ======================================================================== */

static bool dir_exists(const string path);
static bool dir_is_dir(const string path);
static string *dir_list(const string path);
static void dir_walk(const string path, bool recursive, sc_dir_visitor_fn visitor, void *ctx);
static void dir_dispose_list(string *list);

/* ======================================================================== */
/* Forward declarations — helpers                                           */
/* ======================================================================== */

static bool path_stat(const string path, struct stat *st);
static void dir_walk_impl(const string path, bool recursive, sc_dir_visitor_fn visitor, void *ctx,
                          bool *stop);

/* ======================================================================== */
/* File API implementation                                                  */
/* ======================================================================== */

static sc_file_t file_open(const string path, const string mode) {
    if (!path || !mode) return NULL;

    FILE *fp = fopen(path, mode);
    if (!fp) return NULL;

    sc_file_t f = Allocator.alloc(sizeof(struct sc_file_s));
    if (!f) {
        fclose(fp);
        return NULL;
    }
    f->fp = fp;
    return f;
}

static void file_close(sc_file_t f) {
    if (!f) return;
    if (f->fp) fclose(f->fp);
    Allocator.dispose(f);
}

static string file_read(sc_file_t f, usize limit, usize *out_size) {
    if (!f || !f->fp || limit == 0) return NULL;

    string buf = Allocator.alloc(limit + 1);
    if (!buf) return NULL;

    usize got = fread(buf, 1, limit, f->fp);
    if (got == 0) {
        Allocator.dispose(buf);
        return NULL;
    }
    buf[got] = '\0';
    if (out_size) *out_size = got;
    return buf;
}

static string file_read_line(sc_file_t f) {
    if (!f || !f->fp) return NULL;

    usize cap = 128;
    usize len = 0;
    string buf = Allocator.alloc(cap);
    if (!buf) return NULL;

    int c;
    while ((c = fgetc(f->fp)) != EOF) {
        if (len + 1 >= cap) {
            usize new_cap = cap * 2;
            string grown = Allocator.realloc(buf, new_cap);
            if (!grown) {
                Allocator.dispose(buf);
                return NULL;
            }
            buf = grown;
            cap = new_cap;
        }
        if (c == '\n') break;
        buf[len++] = (char)c;
    }

    if (len == 0 && c == EOF) {
        Allocator.dispose(buf);
        return NULL;
    }
    buf[len] = '\0';
    return buf;
}

static integer file_write(sc_file_t f, const string data, usize size) {
    if (!f || !f->fp || !data) return ERR;
    usize written = fwrite(data, 1, size, f->fp);
    return (written == size) ? (integer)written : ERR;
}

static integer file_append(sc_file_t f, const string data, usize size) {
    if (!f || !f->fp || !data) return ERR;
    if (fseek(f->fp, 0, SEEK_END) != 0) return ERR;
    usize written = fwrite(data, 1, size, f->fp);
    return (written == size) ? (integer)written : ERR;
}

static integer file_seek(sc_file_t f, integer offset, int whence) {
    if (!f || !f->fp) return ERR;
    return (fseek(f->fp, (long)offset, whence) == 0) ? OK : ERR;
}

static integer file_flush(sc_file_t f) {
    if (!f || !f->fp) return ERR;
    return (fflush(f->fp) == 0) ? OK : ERR;
}

static integer file_size(const string path) {
    struct stat st;
    if (!path_stat(path, &st)) return ERR;
    if (!S_ISREG(st.st_mode)) return ERR;
    return (integer)st.st_size;
}

static bool file_exists(const string path) {
    struct stat st;
    if (!path_stat(path, &st)) return false;
    return S_ISREG(st.st_mode);
}

static integer file_remove(const string path) {
    if (!path) return ERR;
    return (unlink(path) == 0) ? OK : ERR;
}

static void file_dispose(string s) { Allocator.dispose(s); }

/* ======================================================================== */
/* Directory API implementation                                             */
/* ======================================================================== */

static bool dir_exists(const string path) {
    struct stat st;
    return path_stat(path, &st);
}

static bool dir_is_dir(const string path) {
    struct stat st;
    if (!path_stat(path, &st)) return false;
    return S_ISDIR(st.st_mode);
}

static string *dir_list(const string path) {
    if (!dir_is_dir(path)) return NULL;

    DIR *dh = opendir(path);
    if (!dh) return NULL;

    usize cap = 16;
    usize count = 0;
    string *entries = Allocator.alloc((cap + 1) * sizeof(string));
    if (!entries) {
        closedir(dh);
        return NULL;
    }

    struct dirent *de;
    while ((de = readdir(dh)) != NULL) {
        /* skip . and .. */
        if (de->d_name[0] == '.' &&
            (de->d_name[1] == '\0' || (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;

        usize nlen = strlen(de->d_name);
        string name = Allocator.alloc(nlen + 1);
        if (!name) {
            dir_dispose_list(entries);
            closedir(dh);
            return NULL;
        }
        memcpy(name, de->d_name, nlen + 1);

        if (count >= cap) {
            usize new_cap = cap * 2;
            string *grown = Allocator.realloc(entries, (new_cap + 1) * sizeof(string));
            if (!grown) {
                Allocator.dispose(name);
                dir_dispose_list(entries);
                closedir(dh);
                return NULL;
            }
            entries = grown;
            cap = new_cap;
        }
        entries[count++] = name;
    }
    closedir(dh);

    entries[count] = NULL; /* NULL sentinel */
    return entries;
}

static void dir_walk(const string path, bool recursive, sc_dir_visitor_fn visitor, void *ctx) {
    if (!path || !visitor) return;
    bool stop = false;
    dir_walk_impl(path, recursive, visitor, ctx, &stop);
}

static void dir_dispose_list(string *list) {
    if (!list) return;
    for (string *ep = list; *ep != NULL; ep++) Allocator.dispose(*ep);
    Allocator.dispose(list);
}

/* ======================================================================== */
/* Helper implementations                                                   */
/* ======================================================================== */

static bool path_stat(const string path, struct stat *st) {
    if (!path) return false;
    return (stat(path, st) == 0);
}

static void dir_walk_impl(const string path, bool recursive, sc_dir_visitor_fn visitor, void *ctx,
                          bool *stop) {
    DIR *dh = opendir(path);
    if (!dh) return;

    struct dirent *de;
    while (!*stop && (de = readdir(dh)) != NULL) {
        if (de->d_name[0] == '.' &&
            (de->d_name[1] == '\0' || (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;

        /* Compose full entry path on the stack — zero allocation. */
        char entry_path[4096];
        size_t plen = strlen(path);
        size_t nlen = strlen(de->d_name);
        if (plen + 1 + nlen >= sizeof(entry_path)) continue;

        memcpy(entry_path, path, plen);
        entry_path[plen] = '/';
        memcpy(entry_path + plen + 1, de->d_name, nlen + 1);

        struct stat st;
        bool is_directory = false;
        if (stat(entry_path, &st) == 0) is_directory = S_ISDIR(st.st_mode);

        bool cont = visitor(entry_path, de->d_name, is_directory, ctx);
        if (!cont) {
            *stop = true;
            break;
        }

        if (recursive && is_directory) dir_walk_impl(entry_path, true, visitor, ctx, stop);
    }
    closedir(dh);
}

/* ======================================================================== */
/* Global interface instances                                               */
/* ======================================================================== */

const sc_file_i File = {
    .open = file_open,
    .close = file_close,
    .read = file_read,
    .read_line = file_read_line,
    .write = file_write,
    .append = file_append,
    .seek = file_seek,
    .flush = file_flush,
    .size = file_size,
    .exists = file_exists,
    .remove = file_remove,
    .dispose = file_dispose,
};

const sc_dir_i Directory = {
    .exists = dir_exists,
    .is_dir = dir_is_dir,
    .list = dir_list,
    .walk = dir_walk,
    .dispose_list = dir_dispose_list,
};
