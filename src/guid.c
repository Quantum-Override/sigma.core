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
 * File: guid.c
 * Description: Implementation of the GUID interface for Sigma.Core
 */

/* ====================================================================
 * Base types must come first — platform block uses byte, usize, ERR.
 * ==================================================================== */

#include "types.h"

/* ====================================================================
 * Platform random source — no POSIX rand/random used.
 * ==================================================================== */

#if defined(__linux__)

#include <sys/syscall.h>
/* Declare syscall directly — avoids pulling in all of <unistd.h>. */
extern long syscall(long number, ...);

static int sc_random_bytes(byte *buf, usize n) {
    long r = syscall(SYS_getrandom, buf, n, 0);
    return (r == (long)n) ? 0 : ERR;
}

#elif defined(__APPLE__)

/* getentropy available since macOS 10.12 */
#include <sys/random.h>

static int sc_random_bytes(byte *buf, usize n) { return getentropy(buf, n); }

#elif defined(_WIN32)

#include <bcrypt.h>

static int sc_random_bytes(byte *buf, usize n) {
    return BCryptGenRandom(NULL, (PUCHAR)buf, (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0 ? 0
                                                                                              : ERR;
}

#else
#error "Unsupported platform: guid.c requires __linux__, __APPLE__, or _WIN32"
#endif

/* ====================================================================
 * Remaining includes
 * ==================================================================== */

#include <stdio.h>
#include "guid.h"
#include "timing.h"

/* ====================================================================
 * Forward declarations
 * ==================================================================== */

// API functions
static sc_guid_t guid_generate(void);
static string guid_to_string(sc_guid_t guid, string buf, usize size);
static bool guid_equal(sc_guid_t a, sc_guid_t b);

/* ====================================================================
 * API function definitions
 * ==================================================================== */

static sc_guid_t guid_generate(void) {
    sc_guid_t g = {0};
    sc_random_bytes(g.data, 8);

    uint64_t ns = Time.epoch();
    g.data[8] = (byte)(ns >> 56);
    g.data[9] = (byte)(ns >> 48);
    g.data[10] = (byte)(ns >> 40);
    g.data[11] = (byte)(ns >> 32);
    g.data[12] = (byte)(ns >> 24);
    g.data[13] = (byte)(ns >> 16);
    g.data[14] = (byte)(ns >> 8);
    g.data[15] = (byte)(ns);
    return g;
}

static string guid_to_string(sc_guid_t g, string buf, usize size) {
    if (!buf) return NULL;
    snprintf(buf, size, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             g.data[0], g.data[1], g.data[2], g.data[3], g.data[4], g.data[5], g.data[6], g.data[7],
             g.data[8], g.data[9], g.data[10], g.data[11], g.data[12], g.data[13], g.data[14],
             g.data[15]);
    return buf;
}

static bool guid_equal(sc_guid_t a, sc_guid_t b) {
    for (usize i = 0; i < 16; i++) {
        if (a.data[i] != b.data[i]) return false;
    }
    return true;
}

/* ====================================================================
 * Interface definition
 * ==================================================================== */

const sc_guid_i Guid = {
    .generate = guid_generate,
    .to_string = guid_to_string,
    .equal = guid_equal,
    .zero = {.data = {0}},
};
