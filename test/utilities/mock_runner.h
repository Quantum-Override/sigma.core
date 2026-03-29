/*
 * Sigma.Core
 * Copyright (c) 2026 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------
 * File: test/utilities/mock_runner.h
 * Description: Lightweight test runner for standalone unit tests (no sigma.test linkage)
 */

#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Test state
 */
static int mock_tests_run = 0;
static int mock_tests_passed = 0;
static int mock_tests_failed = 0;
static const char *mock_current_test = NULL;
static bool mock_test_failed = false;

/*
 * MOCK_ASSERT(condition, message)
 *
 * Assert a condition is true. If false, print message and mark test as failed.
 */
#define MOCK_ASSERT(cond, msg)                                                  \
    do {                                                                        \
        if (!(cond)) {                                                          \
            fprintf(stderr, "  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, (msg)); \
            mock_test_failed = true;                                            \
        }                                                                       \
    } while (0)

/*
 * mock_run(name, test_fn)
 *
 * Run a single test function. Print pass/fail status.
 */
static inline void mock_run(const char *name, void (*test_fn)(void)) {
    mock_current_test = name;
    mock_test_failed = false;
    mock_tests_run++;

    /* Run the test */
    test_fn();

    /* Report result */
    if (mock_test_failed) {
        fprintf(stderr, "FAIL: %s\n", name);
        mock_tests_failed++;
    } else {
        printf("PASS: %s\n", name);
        mock_tests_passed++;
    }
}

/*
 * mock_summary(suite_name)
 *
 * Print test summary and return exit code (0 if all passed, 1 if any failed).
 */
static inline int mock_summary(const char *suite_name) {
    printf("\n========================================\n");
    printf("%s Test Summary\n", suite_name);
    printf("========================================\n");
    printf("Total:  %d\n", mock_tests_run);
    printf("Passed: %d\n", mock_tests_passed);
    printf("Failed: %d\n", mock_tests_failed);
    printf("========================================\n");

    return (mock_tests_failed == 0) ? 0 : 1;
}
