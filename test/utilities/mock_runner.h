/*
 * Sigma.Core
 * Copyright (c) 2026 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------
 * File: test/utilities/mock_runner.h
 * Description: Minimal test utility for standalone unit/integration tests that
 *              do not link sigma.test.  Zero external dependencies beyond stdio.
 *
 * Usage:
 *   #include "utilities/mock_runner.h"
 *
 *   static void test_something(void) {
 *       MOCK_ASSERT(1 + 1 == 2, "math should work");
 *   }
 *
 *   int main(void) {
 *       mock_run("something",  test_something);
 *       return mock_summary("my_suite");
 *   }
 */
#pragma once

#ifndef SIGMA_MOCK_RUNNER_H
#define SIGMA_MOCK_RUNNER_H

#include <stdio.h>

static int _mock_pass = 0;
static int _mock_fail = 0;

/**
 * MOCK_ASSERT — requires the macro form to capture __FILE__ and __LINE__.
 * Counts each evaluation as a pass or fail.
 */
#define MOCK_ASSERT(cond, msg)                                                   \
    do {                                                                         \
        if (!(cond)) {                                                           \
            fprintf(stderr, "    FAIL [%s:%d] %s\n", __FILE__, __LINE__, (msg)); \
            _mock_fail++;                                                        \
        } else {                                                                 \
            _mock_pass++;                                                        \
        }                                                                        \
    } while (0)

/**
 * @brief Run a single test function, print PASS/FAIL with the test name.
 *
 * Counts assertion failures that occurred inside fn().
 */
static inline void mock_run(const char *name, void (*fn)(void)) {
    int pre = _mock_fail;
    fn();
    printf("  [%s] %s\n", (_mock_fail == pre) ? "PASS" : "FAIL", name);
}

/**
 * @brief Print a results summary line and return the total failure count.
 *
 * Return value is suitable for use as main()'s exit code.
 */
static inline int mock_summary(const char *suite) {
    int total = _mock_pass + _mock_fail;
    printf("\n%s: %d/%d passed", suite, _mock_pass, total);
    if (_mock_fail > 0) printf(", %d FAILED", _mock_fail);
    printf("\n");
    return _mock_fail;
}

#endif /* SIGMA_MOCK_RUNNER_H */
