/*
 * Sigma.Core — compatibility shim
 * String and StringBuilder types have moved to <sigma.core/strings.h>.
 * This forwarding header ensures code that still includes <sigma.text/strings.h>
 * (e.g. via sigma.test's sigtest.h) resolves to the same definitions and avoids
 * duplicate-type errors.
 */
#pragma once

#include <sigma.core/strings.h>
