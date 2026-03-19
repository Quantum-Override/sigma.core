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
 * File: timing.h
 * Description: Time interface for Sigma.Core
 */

#pragma once

#include "types.h"

/**
 * @brief Structured calendar time value (UTC by default).
 */
typedef struct {
    int32_t year;           /**< Full year, e.g. 2026           */
    uint8_t month;          /**< 1–12                            */
    uint8_t day;            /**< 1–31                            */
    uint8_t hour;           /**< 0–23                            */
    uint8_t minute;         /**< 0–59                            */
    uint8_t second;         /**< 0–59                            */
    uint32_t nanosecond;    /**< 0–999999999                     */
    int16_t utc_offset_min; /**< 0 = UTC; reserved for TZ support */
} sc_time_t;

/**
 * @brief Time interface.
 */
typedef struct sc_time_i {
    /**
     * @brief Return local time (v1: always UTC; TZ support deferred).
     * @return sc_time_t value — no allocation, no dispose needed.
     */
    sc_time_t (*current)(void);

    /**
     * @brief Return current UTC time.
     * @return sc_time_t value — no allocation, no dispose needed.
     */
    sc_time_t (*current_utc)(void);

    /**
     * @brief Format current UTC time as an ISO 8601 timestamp.
     *
     * Format: "YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ" (30 chars + NUL = 31 bytes).
     * @param buf   Caller-supplied buffer of at least 31 bytes.
     * @param size  Size of buf in bytes; pass 31 or larger.
     * @return buf on success, NULL if buf is NULL.
     */
    string (*timestamp)(string buf, usize size);

    /**
     * @brief Return nanoseconds elapsed since the Unix epoch (1970-01-01T00:00:00Z).
     * @return uint64_t nanosecond count.
     */
    uint64_t (*epoch)(void);
} sc_time_i;

extern const sc_time_i Time;
