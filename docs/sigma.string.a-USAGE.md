# sigma.string.a Usage Guide

**Zero-dependency string utilities for C projects**

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [String API](#string-api)
3. [StringBuilder API](#stringbuilder-api)
4. [Common Patterns](#common-patterns)
5. [Memory Management](#memory-management)
6. [Build & Link](#build--link)
7. [Error Handling](#error-handling)

---

## Quick Start

### Include the Header

```c
#include <sigma/string_malloc.h>
```

### Simple Example

```c
#include <sigma/string_malloc.h>
#include <stdio.h>

int main(void) {
    // Create strings
    string greeting = string_dupe("Hello");
    string name = string_dupe("World");
    
    // Combine them
    string fmt = string_dupe("%s, %s!");
    string message = string_format(fmt, greeting, name);
    printf("%s\n", message);  // "Hello, World!"
    
    // Clean up
    string_dispose(greeting);
    string_dispose(name);
    string_dispose(message);
    string_dispose(fmt);
    
    return 0;
}
```

**Compile:**
```bash
gcc myapp.c -o myapp -lsigma_string
./myapp
```

---

## String API

### string_dupe

Create a new string from a C string literal.

```c
string str = string_dupe("Hello, World!");
printf("%s\n", str);  // "Hello, World!"
string_dispose(str);
```

**NULL handling:**
```c
string null_str = string_dupe(NULL);  // Returns NULL
// No dispose needed for NULL
```

**Empty string:**
```c
string empty = string_dupe("");
printf("Length: %zu\n", string_length(empty));  // "Length: 0"
string_dispose(empty);
```

---

### string_copy

Duplicate an existing string (deep copy).

```c
string original = string_dupe("Original");
string copy = string_copy(original);

printf("Original: %s\n", original);  // "Original"
printf("Copy: %s\n", copy);          // "Original"

// Modify one doesn't affect the other
// (both are independent allocations)

string_dispose(original);
string_dispose(copy);
```

**Use case:** Pass string ownership to different subsystems.

---

### string_length

Get the length of a string (character count, excluding null terminator).

```c
string str = string_dupe("Hello");
usize len = string_length(str);
printf("Length: %zu\n", len);  // "Length: 5"
string_dispose(str);
```

**NULL safety:**
```c
usize len = string_length(NULL);  // Returns 0
```

---

### string_concat

Concatenate two strings into a new string.

```c
string first = string_dupe("Hello");
string second = string_dupe("World");
string combined = string_concat(first, second);

printf("%s\n", combined);  // "HelloWorld"

string_dispose(first);
string_dispose(second);
string_dispose(combined);
```

**Add spacing:**
```c
string space = string_dupe(" ");
string hello = string_dupe("Hello");
string world = string_dupe("World");

string temp = string_concat(hello, space);
string result = string_concat(temp, world);

printf("%s\n", result);  // "Hello World"

string_dispose(space);
string_dispose(hello);
string_dispose(world);
string_dispose(temp);
string_dispose(result);
```

---

### string_compare

Compare two strings (wrapper around `strcmp`).

```c
string a = string_dupe("apple");
string b = string_dupe("banana");
string c = string_dupe("apple");

int cmp1 = string_compare(a, b);  // < 0 (a comes before b)
int cmp2 = string_compare(a, c);  // == 0 (equal)
int cmp3 = string_compare(b, a);  // > 0 (b comes after a)

if (string_compare(a, c) == 0) {
    printf("Strings are equal\n");
}

string_dispose(a);
string_dispose(b);
string_dispose(c);
```

**NULL handling:**
```c
string_compare(NULL, NULL);  // 0 (considered equal)
string_compare("hello", NULL);  // > 0
string_compare(NULL, "hello");  // < 0
```

---

### string_format

Create formatted strings (printf-style).

```c
string name = string_dupe("Alice");
int age = 30;
double height = 5.7;

string fmt = string_dupe("%s is %d years old and %.1f feet tall");
string bio = string_format(fmt, name, age, height);
printf("%s\n", bio);
// "Alice is 30 years old and 5.7 feet tall"

string_dispose(name);
string_dispose(bio);
string_dispose(fmt);
```

**Complex formatting:**
```c
string fmt = string_dupe(
    "%-10s | %5d | %8.2f\n"
    "%-10s | %5d | %8.2f\n"
);
string report = string_format(
    fmt,
    "Item A", 100, 1234.56,
    "Item B", 200, 9876.54
);
printf("%s", report);
/*
Item A     |   100 |  1234.56
Item B     |   200 |  9876.54
*/
string_dispose(report);
string_dispose(fmt);
```

---

### string_to_array

Convert string to char array (creates independent copy).

```c
string str = string_dupe("Hello");
char *arr = string_to_array(str);

printf("String: %s\n", str);  // "Hello"
printf("Array: %s\n", arr);   // "Hello"

// arr is independent of str
arr[0] = 'J';
printf("String: %s\n", str);  // "Hello" (unchanged)
printf("Array: %s\n", arr);   // "Jello"

string_dispose(str);
free(arr);  // char arrays use free() directly
```

**Use case:** Interfacing with APIs that require `char*` ownership.

---

### string_dispose

Free string memory. Always call for allocated strings.

```c
string str = string_dupe("Temporary");
// ... use str ...
string_dispose(str);
```

**NULL safety:**
```c
string_dispose(NULL);  // Safe, no-op
```

**Stack/literal strings:**
```c
const char *literal = "Stack string";
// string_dispose((string)literal);  // ❌ NEVER DO THIS
// Only dispose heap-allocated strings from this library
```

---

## StringBuilder API

StringBuilder provides efficient string building with automatic buffer management.

### stringbuilder_new

Create a new StringBuilder with initial capacity.

```c
string_builder sb = stringbuilder_new(64);  // 64-byte initial capacity
// ... use sb ...
stringbuilder_dispose(sb);
```

**Default capacity:**
```c
string_builder sb = stringbuilder_new(0);  // Uses default (16 bytes)
stringbuilder_dispose(sb);
```

---

### stringbuilder_snew

Create StringBuilder from existing string.

```c
string initial = string_dupe("Starting text");
string_builder sb = stringbuilder_snew(initial);

stringbuilder_append(sb, " - more content");
string result = stringbuilder_toString(sb);

printf("%s\n", result);  // "Starting text - more content"

string_dispose(initial);
string_dispose(result);
stringbuilder_dispose(sb);
```

---

### stringbuilder_append

Append a string to the builder.

```c
string_builder sb = stringbuilder_new(0);

stringbuilder_append(sb, "Hello");
stringbuilder_append(sb, " ");
stringbuilder_append(sb, "World");

string result = stringbuilder_toString(sb);
printf("%s\n", result);  // "Hello World"

string_dispose(result);
stringbuilder_dispose(sb);
```

**NULL safety:**
```c
stringbuilder_append(sb, NULL);  // Safe, no-op
```

---

### stringbuilder_appendf

Append formatted string (printf-style).

```c
string_builder sb = stringbuilder_new(0);

stringbuilder_append(sb, "Report:\n");
string fmt1 = string_dupe("  Count: %d\n");
stringbuilder_appendf(sb, fmt1, 42);
string fmt2 = string_dupe("  Rate: %.2f%%\n");
stringbuilder_appendf(sb, fmt2, 98.5);

string result = stringbuilder_toString(sb);
printf("%s", result);
/*
Report:
  Count: 42
  Rate: 98.50%
*/

string_dispose(result);
string_dispose(fmt1);
string_dispose(fmt2);
stringbuilder_dispose(sb);
```

---

### stringbuilder_appendl

Append a line (automatically adds newline).

```c
string_builder sb = stringbuilder_new(0);

stringbuilder_appendl(sb, "Line 1");
stringbuilder_appendl(sb, "Line 2");
stringbuilder_appendl(sb, "Line 3");

string result = stringbuilder_toString(sb);
printf("%s", result);
/*
Line 1
Line 2
Line 3
*/

string_dispose(result);
stringbuilder_dispose(sb);
```

---

### stringbuilder_lappends

Append multiple lines at once (variadic, NULL-terminated).

```c
string_builder sb = stringbuilder_new(0);

stringbuilder_append(sb, "Header\n");
stringbuilder_lappends(sb, 
    "First line",
    "Second line",
    "Third line",
    NULL  // ⚠️ MUST end with NULL
);

string result = stringbuilder_toString(sb);
printf("%s", result);
/*
Header
First line
Second line
Third line
*/

string_dispose(result);
stringbuilder_dispose(sb);
```

**⚠️ Warning:** Forgetting `NULL` terminator causes undefined behavior.

---

### stringbuilder_lappendf

Append formatted line (printf-style with newline).

```c
string_builder sb = stringbuilder_new(0);
string fmt = string_dupe("Item %d: Value = %d");

for (int i = 0; i < 3; i++) {
    stringbuilder_lappendf(sb, fmt, i, i * 10);
}

string result = stringbuilder_toString(sb);
printf("%s", result);
/*
Item 0: Value = 0
Item 1: Value = 10
Item 2: Value = 20
*/

string_dispose(result);
string_dispose(fmt);
stringbuilder_dispose(sb);
```

---

### stringbuilder_clear

Clear content but keep capacity (fast reset).

```c
string_builder sb = stringbuilder_new(128);

// Build first message
stringbuilder_append(sb, "First message");
string msg1 = stringbuilder_toString(sb);
printf("%s\n", msg1);
string_dispose(msg1);

// Reuse builder
stringbuilder_clear(sb);
stringbuilder_append(sb, "Second message");
string msg2 = stringbuilder_toString(sb);
printf("%s\n", msg2);
string_dispose(msg2);

stringbuilder_dispose(sb);
```

**Use case:** Reuse builder in loops without reallocation overhead.

---

### stringbuilder_toString

Extract final string from builder (creates copy).

```c
string_builder sb = stringbuilder_new(0);
stringbuilder_append(sb, "Content");

string result = stringbuilder_toString(sb);
printf("%s\n", result);  // "Content"

// Builder still valid, can keep using
stringbuilder_append(sb, " More");
string result2 = stringbuilder_toString(sb);
printf("%s\n", result2);  // "Content More"

string_dispose(result);
string_dispose(result2);
stringbuilder_dispose(sb);
```

---

### stringbuilder_toStream

Write builder content to FILE stream (no allocation).

```c
string_builder sb = stringbuilder_new(0);
stringbuilder_appendl(sb, "Log entry 1");
stringbuilder_appendl(sb, "Log entry 2");

// Write to file
FILE *f = fopen("output.log", "w");
if (f) {
    stringbuilder_toStream(sb, f);
    fclose(f);
}

// Write to stdout
stringbuilder_toStream(sb, stdout);

stringbuilder_dispose(sb);
```

**Use case:** Direct streaming without intermediate string allocation.

---

### stringbuilder_length

Get current content length.

```c
string_builder sb = stringbuilder_new(0);
stringbuilder_append(sb, "Hello");

usize len = stringbuilder_length(sb);
printf("Length: %zu\n", len);  // "Length: 5"

stringbuilder_dispose(sb);
```

---

### stringbuilder_capacity

Get current buffer capacity.

```c
string_builder sb = stringbuilder_new(32);

usize cap = stringbuilder_capacity(sb);
printf("Capacity: %zu\n", cap);  // "Capacity: 32"

stringbuilder_dispose(sb);
```

---

### stringbuilder_setCapacity

Manually set buffer capacity (grow or shrink).

```c
string_builder sb = stringbuilder_new(16);
printf("Initial: %zu\n", stringbuilder_capacity(sb));  // 16

// Grow
stringbuilder_setCapacity(sb, 128);
printf("After grow: %zu\n", stringbuilder_capacity(sb));  // 128

// Shrink (keeps content if it fits)
stringbuilder_setCapacity(sb, 32);
printf("After shrink: %zu\n", stringbuilder_capacity(sb));  // 32

stringbuilder_dispose(sb);
```

**Use case:** Pre-allocate for large content to avoid reallocations.

---

### stringbuilder_dispose

Free StringBuilder and internal buffers.

```c
string_builder sb = stringbuilder_new(0);
// ... use sb ...
stringbuilder_dispose(sb);
```

**NULL safety:**
```c
stringbuilder_dispose(NULL);  // Safe, no-op
```

---

## Common Patterns

### Building SQL Queries

```c
string_builder query = stringbuilder_new(256);

stringbuilder_append(query, "SELECT * FROM users WHERE ");
string fmt1 = string_dupe("age > %d AND ");
stringbuilder_appendf(query, fmt1, 18);
string fmt2 = string_dupe("country = '%s'");
stringbuilder_appendf(query, fmt2, "US");

string sql = stringbuilder_toString(query);
printf("%s\n", sql);
// "SELECT * FROM users WHERE age > 18 AND country = 'US'"

string_dispose(sql);
string_dispose(fmt1);
string_dispose(fmt2);
stringbuilder_dispose(query);
```

---

### Building JSON

```c
string_builder json = stringbuilder_new(0);

stringbuilder_appendl(json, "{");
string fmt1 = string_dupe("  \"name\": \"%s\",");
stringbuilder_lappendf(json, fmt1, "Alice");
string fmt2 = string_dupe("  \"age\": %d,");
stringbuilder_lappendf(json, fmt2, 30);
stringbuilder_appendl(json, "  \"active\": true");
stringbuilder_append(json, "}");

string result = stringbuilder_toString(json);
printf("%s\n", result);

string_dispose(result);
string_dispose(fmt1);
string_dispose(fmt2);
stringbuilder_dispose(json);
```

---

### CSV Generation

```c
string_builder csv = stringbuilder_new(1024);

// Header
stringbuilder_appendl(csv, "Name,Age,Score");

// Data rows
const char *names[] = {"Alice", "Bob", "Carol"};
int ages[] = {25, 30, 28};
double scores[] = {95.5, 87.3, 92.1};

string fmt = string_dupe("%s,%d,%.1f");
for (int i = 0; i < 3; i++) {
    stringbuilder_lappendf(csv, fmt, 
                           names[i], ages[i], scores[i]);
}

// Write to file
FILE *f = fopen("data.csv", "w");
if (f) {
    stringbuilder_toStream(csv, f);
    fclose(f);
}
string_dispose(fmt);

stringbuilder_dispose(csv);
```

---

### Configuration File Builder

```c
string_builder config = stringbuilder_new(512);

stringbuilder_appendl(config, "[server]");
string fmt1 = string_dupe("host = %s");
stringbuilder_lappendf(config, fmt1, "localhost");
string fmt2 = string_dupe("port = %d");
stringbuilder_lappendf(config, fmt2, 8080);
stringbuilder_appendl(config, "");
stringbuilder_appendl(config, "[database]");
string fmt3 = string_dupe("url = %s");
stringbuilder_lappendf(config, fmt3, "postgresql://localhost/mydb");

string result = stringbuilder_toString(config);
printf("%s\n", result);

string_dispose(result);
string_dispose(fmt1);
string_dispose(fmt2);
string_dispose(fmt3);
stringbuilder_dispose(config);
```

---

### Path Manipulation

```c
string base = string_dupe("/home/user");
string sep = string_dupe("/");
string dir = string_dupe("documents");
string file = string_dupe("readme.txt");

// Build path
string temp1 = string_concat(base, sep);
string temp2 = string_concat(temp1, dir);
string temp3 = string_concat(temp2, sep);
string path = string_concat(temp3, file);

printf("%s\n", path);  // "/home/user/documents/readme.txt"

string_dispose(base);
string_dispose(sep);
string_dispose(dir);
string_dispose(file);
string_dispose(temp1);
string_dispose(temp2);
string_dispose(temp3);
string_dispose(path);
```

**Better with StringBuilder:**
```c
string_builder sb = stringbuilder_new(128);
stringbuilder_append(sb, "/home/user");
stringbuilder_append(sb, "/");
stringbuilder_append(sb, "documents");
stringbuilder_append(sb, "/");
stringbuilder_append(sb, "readme.txt");

string path = stringbuilder_toString(sb);
printf("%s\n", path);

string_dispose(path);
stringbuilder_dispose(sb);
```

---

## Memory Management

### Rules

1. **Always dispose heap-allocated strings:**
   ```c
   string str = string_dupe("text");
   string_dispose(str);  // ✅ Required
   ```

2. **Never dispose stack/literal strings:**
   ```c
   const char *literal = "text";
   // string_dispose((string)literal);  // ❌ Undefined behavior
   ```

3. **NULL disposal is safe:**
   ```c
   string_dispose(NULL);  // ✅ No-op
   stringbuilder_dispose(NULL);  // ✅ No-op
   ```

4. **One allocation = one disposal:**
   ```c
   string a = string_dupe("text");
   string b = string_copy(a);
   string_dispose(a);  // ✅
   string_dispose(b);  // ✅
   ```

---

### Ownership Conventions

**Caller owns returned strings:**
```c
string fmt = string_dupe("Value: %d");
string result = string_format(fmt, 42);
// Caller must dispose
string_dispose(result);
string_dispose(fmt);
```

**StringBuilder extract creates copy:**
```c
string_builder sb = stringbuilder_new(0);
stringbuilder_append(sb, "text");
string s = stringbuilder_toString(sb);  // Creates new allocation
// Now have 2 independent allocations
string_dispose(s);
stringbuilder_dispose(sb);
```

---

### Leak Prevention

**Use goto cleanup pattern:**
```c
int process_data(const char *input) {
    string a = NULL;
    string b = NULL;
    string result = NULL;
    string fmt = NULL;
    int status = -1;
    
    a = string_dupe(input);
    if (!a) goto cleanup;
    
    fmt = string_dupe("Processed: %s");
    if (!fmt) goto cleanup;
    
    b = string_format(fmt, a);
    if (!b) goto cleanup;
    
    result = string_concat(a, b);
    if (!result) goto cleanup;
    
    // Success path
    status = 0;
    
cleanup:
    string_dispose(a);
    string_dispose(b);
    string_dispose(result);
    string_dispose(fmt);
    return status;
}
```

---

### Valgrind Verification

Check for leaks during development:

```bash
valgrind --leak-check=full ./myapp
```

Expected output:
```
HEAP SUMMARY:
    in use at exit: 0 bytes in 0 blocks
  total heap usage: N allocs, N frees, X bytes allocated

All heap blocks were freed -- no leaks are possible
```

---

## Build & Link

### Manual Build

```bash
# Compile object file
gcc -c src/string_malloc.c -o build/string_malloc.o \
    -Iinclude -std=c2x -Wall

# Create static library
ar rcs build/libsigma_string.a build/string_malloc.o

# Link application
gcc myapp.c -o myapp -Iinclude -Lbuild -lsigma_string
```

---

### Installed Library

After toolsmith installation:

```bash
gcc myapp.c -o myapp -lsigma_string
```

Header location: `/usr/local/include/sigma/string_malloc.h`  
Library location: `/usr/local/lib/libsigma_string.a`

---

### Makefile Integration

```makefile
CC := gcc
CFLAGS := -std=c2x -Wall -Wextra
LDFLAGS := -lsigma_string

myapp: myapp.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f myapp
```

---

## Error Handling

### NULL Return Values

Most functions return `NULL` on allocation failure:

```c
string str = string_dupe("text");
if (!str) {
    fprintf(stderr, "Allocation failed\n");
    return -1;
}
// ... use str ...
string_dispose(str);
```

---

### Defensive Checks

```c
string safe_concat(string a, string b) {
    if (!a || !b) {
        return NULL;
    }
    return string_concat(a, b);
}

string safe_format(const char *fmt, ...) {
    if (!fmt) {
        return NULL;
    }
    va_list args;
    va_start(args, fmt);
    // ... format logic ...
    va_end(args);
    return result;
}
```

---

### StringBuilder Error Checking

```c
string_builder sb = stringbuilder_new(64);
if (!sb) {
    fprintf(stderr, "Failed to create StringBuilder\n");
    return -1;
}

// ... use sb ...

string result = stringbuilder_toString(sb);
if (!result) {
    fprintf(stderr, "Failed to extract string\n");
    stringbuilder_dispose(sb);
    return -1;
}

// Success
printf("%s\n", result);
string_dispose(result);
stringbuilder_dispose(sb);
```

---

## API Quick Reference

### String Functions (8)

| Function | Description |
|----------|-------------|
| `string_dupe(cstr)` | Create string from C string |
| `string_copy(src)` | Duplicate existing string |
| `string_dispose(s)` | Free string memory |
| `string_length(s)` | Get string length |
| `string_concat(s1, s2)` | Concatenate strings |
| `string_compare(s1, s2)` | Compare strings |
| `string_format(fmt, ...)` | Create formatted string (fmt is string) |
| `string_to_array(s)` | Convert to char array |

---

### StringBuilder Functions (14)

| Function | Description |
|----------|-------------|
| `stringbuilder_new(capacity)` | Create new builder |
| `stringbuilder_snew(str)` | Create from string |
| `stringbuilder_dispose(sb)` | Free builder |
| `stringbuilder_append(sb, str)` | Append string |
| `stringbuilder_appendf(sb, fmt, ...)` | Append formatted (fmt is string) |
| `stringbuilder_appendl(sb, line)` | Append line |
| `stringbuilder_lappends(sb, ...)` | Append multiple lines |
| `stringbuilder_lappendf(sb, fmt, ...)` | Append formatted line (fmt is string) |
| `stringbuilder_clear(sb)` | Clear content |
| `stringbuilder_toString(sb)` | Extract string |
| `stringbuilder_toStream(sb, file)` | Write to stream |
| `stringbuilder_length(sb)` | Get content length |
| `stringbuilder_capacity(sb)` | Get buffer capacity |
| `stringbuilder_setCapacity(sb, cap)` | Set capacity |

---

## Additional Resources

- **Build/Test Guide:** `docs/sigma.string.a.md`
- **API Header:** `include/string_malloc.h`
- **Implementation:** `src/string_malloc.c`
- **Test Suite:** `test/standalone/test_string_malloc.c`

---

**Zero dependencies. Pure malloc. ABI compatible with sigma.core ecosystem.**
