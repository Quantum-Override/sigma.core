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
 * File: test_file.c
 * Description: Test suite for File interface
 */
#include <sigma.core/io.h>
#include <sigma.memory/internal/memory.h>
#include <sigma.test/sigtest.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TMP_FILE "/tmp/sigma_test_file.txt"
#define TMP_CONTENT "Hello, Sigma!\n"
#define TMP_LINES "line one\nline two\nline three\n"

void set_config(FILE **log_stream) { *log_stream = fopen("logs/test_file.log", "w"); }

void set_teardown(void) { unlink(TMP_FILE); }

/* exists() returns false for nonexistent path */
void test_exists_false(void) {
    Assert.isFalse(File.exists("/tmp/__sigma_no_such_file_xyz__.txt"),
                   "File.exists should return false for nonexistent path");
}

/* open/close round-trip */
void test_open_close(void) {
    sc_file_t f = File.open(TMP_FILE, "w");
    Assert.isNotNull(f, "File.open(\"w\") should return non-NULL");
    File.close(f);
    Assert.isTrue(File.exists(TMP_FILE), "file should exist after open+close");
    unlink(TMP_FILE);
}

/* open returns NULL for bad mode/path */
void test_open_null_path(void) {
    sc_file_t f = File.open(NULL, "r");
    Assert.isNull(f, "File.open(NULL path) should return NULL");
}

/* write then read round-trip */
void test_write_read(void) {
    sc_file_t f = File.open(TMP_FILE, "w");
    Assert.isNotNull(f, "open for write failed");

    integer written = File.write(f, TMP_CONTENT, strlen(TMP_CONTENT));
    Assert.areEqual(&(integer){(integer)strlen(TMP_CONTENT)}, &written, LONG,
                    "write byte count mismatch");
    File.close(f);

    f = File.open(TMP_FILE, "r");
    Assert.isNotNull(f, "open for read failed");

    usize got = 0;
    string data = File.read(f, 512, &got);
    Assert.isNotNull(data, "File.read returned NULL");
    Assert.areEqual(&(usize){strlen(TMP_CONTENT)}, &got, LONG, "read size mismatch");
    Assert.areEqual(&(int){0}, &(int){strcmp(data, TMP_CONTENT)}, INT, "read content mismatch");

    File.dispose(data);
    File.close(f);
    unlink(TMP_FILE);
}

/* size() returns correct byte count */
void test_file_size(void) {
    sc_file_t f = File.open(TMP_FILE, "w");
    Assert.isNotNull(f, "open for write failed");
    File.write(f, TMP_CONTENT, strlen(TMP_CONTENT));
    File.close(f);

    integer sz = File.size(TMP_FILE);
    Assert.areEqual(&(integer){(integer)strlen(TMP_CONTENT)}, &sz, LONG, "File.size mismatch");
    unlink(TMP_FILE);
}

/* read_line reads one line at a time (newline stripped) */
void test_read_line(void) {
    sc_file_t f = File.open(TMP_FILE, "w");
    Assert.isNotNull(f, "open for write failed");
    File.write(f, TMP_LINES, strlen(TMP_LINES));
    File.close(f);

    f = File.open(TMP_FILE, "r");
    Assert.isNotNull(f, "open for read failed");

    string line = File.read_line(f);
    Assert.isNotNull(line, "read_line returned NULL for first line");
    Assert.areEqual(&(int){0}, &(int){strcmp(line, "line one")}, INT, "first line mismatch");
    File.dispose(line);

    line = File.read_line(f);
    Assert.isNotNull(line, "read_line returned NULL for second line");
    Assert.areEqual(&(int){0}, &(int){strcmp(line, "line two")}, INT, "second line mismatch");
    File.dispose(line);

    line = File.read_line(f);
    Assert.isNotNull(line, "read_line returned NULL for third line");
    Assert.areEqual(&(int){0}, &(int){strcmp(line, "line three")}, INT, "third line mismatch");
    File.dispose(line);

    /* EOF check */
    line = File.read_line(f);
    Assert.isNull(line, "read_line should return NULL at EOF");

    File.close(f);
    unlink(TMP_FILE);
}

/* append adds content after existing data */
void test_append(void) {
    sc_file_t f = File.open(TMP_FILE, "w");
    Assert.isNotNull(f, "open for write failed");
    File.write(f, "part1", 5);
    File.close(f);

    f = File.open(TMP_FILE, "a");
    Assert.isNotNull(f, "open for append failed");
    File.append(f, "part2", 5);
    File.close(f);

    f = File.open(TMP_FILE, "r");
    Assert.isNotNull(f, "open for read failed");
    usize got = 0;
    string data = File.read(f, 64, &got);
    Assert.isNotNull(data, "read after append returned NULL");
    Assert.areEqual(&(int){0}, &(int){strcmp(data, "part1part2")}, INT,
                    "appended content mismatch");
    File.dispose(data);
    File.close(f);
    unlink(TMP_FILE);
}

/* seek positions the read cursor */
void test_seek(void) {
    sc_file_t f = File.open(TMP_FILE, "w");
    Assert.isNotNull(f, "open for write failed");
    File.write(f, "ABCDE", 5);
    File.close(f);

    f = File.open(TMP_FILE, "r");
    Assert.isNotNull(f, "open for read failed");

    integer rc = File.seek(f, 2, SEEK_SET);
    Assert.areEqual(&(integer){0}, &rc, LONG, "File.seek should return 0");

    usize got = 0;
    string data = File.read(f, 3, &got);
    Assert.isNotNull(data, "read after seek returned NULL");
    Assert.areEqual(&(int){0}, &(int){strcmp(data, "CDE")}, INT, "post-seek read content mismatch");
    File.dispose(data);
    File.close(f);
    unlink(TMP_FILE);
}

/* remove deletes the file */
void test_remove(void) {
    sc_file_t f = File.open(TMP_FILE, "w");
    Assert.isNotNull(f, "open for write failed");
    File.close(f);
    Assert.isTrue(File.exists(TMP_FILE), "file should exist before remove");

    integer rc = File.remove(TMP_FILE);
    Assert.areEqual(&(integer){0}, &rc, LONG, "File.remove should return 0");
    Assert.isFalse(File.exists(TMP_FILE), "file should not exist after remove");
}

/* flush returns 0 on open handle */
void test_flush(void) {
    sc_file_t f = File.open(TMP_FILE, "w");
    Assert.isNotNull(f, "open for write failed");
    File.write(f, "x", 1);
    integer rc = File.flush(f);
    Assert.areEqual(&(integer){0}, &rc, LONG, "File.flush should return 0");
    File.close(f);
    unlink(TMP_FILE);
}

__attribute__((constructor)) void init_file_tests(void) {
    init_memory_system();
    testset("io_file_set", set_config, set_teardown);

    testcase("File.exists false", test_exists_false);
    testcase("File open/close", test_open_close);
    testcase("File open NULL path", test_open_null_path);
    testcase("File write and read", test_write_read);
    testcase("File size", test_file_size);
    testcase("File read_line", test_read_line);
    testcase("File append", test_append);
    testcase("File seek", test_seek);
    testcase("File remove", test_remove);
    testcase("File flush", test_flush);
}
