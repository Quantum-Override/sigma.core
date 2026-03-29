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
 * File: test_dir.c
 * Description: Test suite for Directory interface
 */
#include <sigma.core/io.h>
#include <sigma.test/sigtest.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TMP_DIR "/tmp/sigma_test_dir"
#define TMP_FILE "/tmp/sigma_test_dir/file_a.txt"

/* Context for walk visitor tests */
typedef struct {
    int count;
    bool found_file_a;
} walk_ctx_t;

static bool walk_counter(const string path, const string name, bool is_dir, void *ctx) {
    (void)path;
    (void)is_dir;
    walk_ctx_t *c = (walk_ctx_t *)ctx;
    c->count++;
    if (strcmp(name, "file_a.txt") == 0) c->found_file_a = true;
    return true; /* keep walking */
}

static bool walk_stopper(const string path, const string name, bool is_dir, void *ctx) {
    (void)path;
    (void)name;
    (void)is_dir;
    int *count = (int *)ctx;
    (*count)++;
    return false; /* stop after first entry */
}

void set_config(FILE **log_stream) { *log_stream = fopen("logs/test_dir.log", "w"); }

void set_teardown(void) {
    unlink(TMP_FILE);
    rmdir(TMP_DIR);
}

/* Helper: create the scratch directory + one file inside it */
static void create_tmp_tree(void) {
    mkdir(TMP_DIR, 0700);
    FILE *fp = fopen(TMP_FILE, "w");
    if (fp) {
        fputs("x", fp);
        fclose(fp);
    }
}

/* exists() returns false for nonexistent path */
void test_exists_false(void) {
    Assert.isFalse(Directory.exists("/tmp/__sigma_no_such_dir_xyz__"),
                   "Directory.exists should return false for nonexistent path");
}

/* exists() returns true for a real directory */
void test_exists_true(void) {
    create_tmp_tree();
    Assert.isTrue(Directory.exists(TMP_DIR),
                  "Directory.exists should return true for an existing directory");
    set_teardown();
}

/* is_dir() returns true for a directory */
void test_is_dir_true(void) {
    create_tmp_tree();
    Assert.isTrue(Directory.is_dir(TMP_DIR), "Directory.is_dir should return true for a directory");
    set_teardown();
}

/* is_dir() returns false for a regular file */
void test_is_dir_false_for_file(void) {
    create_tmp_tree();
    Assert.isFalse(Directory.is_dir(TMP_FILE),
                   "Directory.is_dir should return false for a regular file");
    set_teardown();
}

/* is_dir() returns false for nonexistent path */
void test_is_dir_false_nonexistent(void) {
    Assert.isFalse(Directory.is_dir("/tmp/__sigma_no_such_dir_xyz__"),
                   "Directory.is_dir should return false for nonexistent path");
}

/* list() returns NULL for nonexistent path */
void test_list_nonexistent(void) {
    string *entries = Directory.list("/tmp/__sigma_no_such_dir_xyz__");
    Assert.isNull(entries, "Directory.list should return NULL for nonexistent path");
}

/* list() returns a NULL-terminated array containing the entry */
void test_list_entries(void) {
    create_tmp_tree();

    string *entries = Directory.list(TMP_DIR);
    Assert.isNotNull(entries, "Directory.list should return non-NULL for existing dir");

    /* Walk until NULL sentinel, look for file_a.txt */
    bool found = false;
    int count = 0;
    for (string *ep = entries; *ep != NULL; ep++) {
        count++;
        if (strcmp(*ep, "file_a.txt") == 0) found = true;
    }
    Assert.isTrue(count > 0, "Directory.list should have at least one entry");
    Assert.isTrue(found, "Directory.list should include file_a.txt");

    Directory.dispose_list(entries);
    set_teardown();
}

/* dispose_list() handles NULL gracefully */
void test_dispose_list_null(void) {
    /* Must not crash */
    Directory.dispose_list(NULL);
}

/* walk() fires visitor for each entry in flat dir */
void test_walk_flat(void) {
    create_tmp_tree();

    walk_ctx_t ctx = {0, false};
    Directory.walk(TMP_DIR, false, walk_counter, &ctx);

    Assert.isTrue(ctx.count > 0, "walk should find at least one entry");
    Assert.isTrue(ctx.found_file_a, "walk should find file_a.txt");

    set_teardown();
}

/* walk() respects early-stop (visitor returns false) */
void test_walk_early_stop(void) {
    create_tmp_tree();

    int count = 0;
    Directory.walk(TMP_DIR, false, walk_stopper, &count);

    Assert.areEqual(&(int){1}, &count, INT,
                    "walk should invoke visitor exactly once when it returns false");

    set_teardown();
}

/* walk() with NULL visitor must not crash */
void test_walk_null_visitor(void) {
    create_tmp_tree();
    Directory.walk(TMP_DIR, false, NULL, NULL);
    set_teardown();
}

static void register_dir_tests(void) {
    testset("io_dir_set", set_config, set_teardown);

    testcase("Directory.exists false", test_exists_false);
    testcase("Directory.exists true", test_exists_true);
    testcase("Directory.is_dir true", test_is_dir_true);
    testcase("Directory.is_dir false for file", test_is_dir_false_for_file);
    testcase("Directory.is_dir false nonexistent", test_is_dir_false_nonexistent);
    testcase("Directory.list nonexistent", test_list_nonexistent);
    testcase("Directory.list entries", test_list_entries);
    testcase("Directory.dispose_list NULL", test_dispose_list_null);
    testcase("Directory.walk flat", test_walk_flat);
    testcase("Directory.walk early stop", test_walk_early_stop);
    testcase("Directory.walk NULL visitor", test_walk_null_visitor);
}

__attribute__((constructor)) static void enqueue_dir_tests(void) {
    Tests.enqueue(register_dir_tests);
}
