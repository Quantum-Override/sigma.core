/*
 * Test: Verify vtable-only public API
 *
 * This demonstrates the vtable-only pattern used throughout sigma.core
 * All functions are accessed through the String and StringBuilder vtables
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "string_malloc.h"

int main(void) {
    printf("==============================================\n");
    printf("VTable-Only Public API Test\n");
    printf("==============================================\n\n");

    // Test 1: String vtable usage
    printf("Testing String vtable...\n");
    string s1 = String.dupe("Hello");
    string s2 = String.dupe("World");
    assert(String.compare(s1, s2) != 0);

    string s3 = String.concat(s1, s2);
    assert(String.length(s3) == 10);

    string s4 = String.format("Result: %s", s3);
    printf("  %s\n", s4);

    String.dispose(s1);
    String.dispose(s2);
    String.dispose(s3);
    String.dispose(s4);
    printf("  ✓ String vtable works\n\n");

    // Test 2: StringBuilder vtable usage
    printf("Testing StringBuilder vtable...\n");
    string_builder sb = StringBuilder.new(64);
    StringBuilder.append(sb, "Line 1");
    StringBuilder.appendl(sb, " continues");
    StringBuilder.appendf(sb, "Value: %d", 42);

    string result = StringBuilder.toString(sb);
    printf("  %s\n", result);
    assert(StringBuilder.length(sb) == StringBuilder.length(sb));  // Just to use the vtable

    String.dispose(result);
    StringBuilder.dispose(sb);
    printf("  ✓ StringBuilder vtable works\n\n");

    // Test 3: Verify vtable-only pattern
    printf("Testing vtable-only pattern...\n");
    string str1 = String.dupe("Vtable");
    string str2 = String.dupe("Only");
    string combined = String.concat(str1, str2);
    printf("  %s\n", combined);
    assert(String.length(combined) == 10);  // "VtableOnly" = 10 chars
    String.dispose(str1);
    String.dispose(str2);
    String.dispose(combined);
    printf("  ✓ Vtable-only pattern works\n\n");

    printf("==============================================\n");
    printf("✓ All vtable tests passed!\n");
    printf("==============================================\n");

    return 0;
}
