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
 * File: guid.h
 * Description: GUID interface for Sigma.Core
 */

#pragma once

#include "types.h"

/**
 * @brief 16-byte globally unique identifier.
 *
 * Layout:
 *   bytes[ 0.. 7] — 64 random bits (OS CSPRNG)
 *   bytes[ 8..15] — 64-bit nanosecond epoch, big-endian
 *
 * String form: "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" (36 chars + NUL)
 */
typedef struct {
    byte data[16];
} sc_guid_t;

/**
 * @brief GUID interface.
 */
typedef struct sc_guid_i {
    /**
     * @brief Generate a new GUID.
     * @return sc_guid_t value — no allocation, no dispose needed.
     */
    sc_guid_t (*generate)(void);

    /**
     * @brief Format a GUID as a canonical hex string.
     *
     * Format: "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" (36 chars + NUL = 37 bytes).
     * @param guid  The GUID to format.
     * @param buf   Caller-supplied buffer of at least 37 bytes.
     * @param size  Size of buf in bytes; pass 37 or larger.
     * @return buf on success, NULL if buf is NULL.
     */
    string (*to_string)(sc_guid_t guid, string buf, usize size);

    /**
     * @brief Compare two GUIDs for equality.
     * @return true if all 16 bytes are identical.
     */
    bool (*equal)(sc_guid_t a, sc_guid_t b);

    /**
     * @brief All-zero sentinel GUID.
     */
    sc_guid_t zero;
} sc_guid_i;

extern const sc_guid_i Guid;
