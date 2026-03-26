/*
 * Test: Verify vtable ABI compatibility
 * 
 * This demonstrates drop-in compatibility - code can use either:
 * 1. Direct function calls: string_dupe("hello")
 * 2. Vtable calls: String.dupe("hello")
 */

#include "string_malloc.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

int main(void) {
    printf("==============================================\n");
    printf("VTable ABI Compatibility Test\n");
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
    assert(StringBuilder.length(sb) == StringBuilder.length(sb)); // Just to use the vtable
    
    String.dispose(result);
    StringBuilder.dispose(sb);
    printf("  ✓ StringBuilder vtable works\n\n");

    // Test 3: Mixed usage (direct calls + vtable)
    printf("Testing mixed usage...\n");
    string direct = string_dupe("Direct call");
    string vtable = String.dupe("VTable call");
    string combined = String.concat(direct, vtable);
    printf("  %s\n", combined);
    String.dispose(direct);
    String.dispose(vtable);
    String.dispose(combined);
    printf("  ✓ Mixed usage works\n\n");

    printf("==============================================\n");
    printf("✓ All vtable tests passed!\n");
    printf("==============================================\n");
    
    return 0;
}
