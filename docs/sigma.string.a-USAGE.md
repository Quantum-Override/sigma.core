# sigma.string.a Usage Guide

**Zero-dependency string utilities for C projects**

> **Important:** All functions are accessed through vtable instances (`String`, `StringBuilder`). Direct function calls are NOT available in the public API.

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
    // Create strings using String vtable
    string greeting = String.dupe("Hello");
    string name = String.dupe("World");
    
    // Combine them
    string fmt = String.dupe("%s, %s!");
    string message = String.format(fmt, greeting, name);
    printf("%s\n", message);  // "Hello, World!"
    
    // Clean up using String vtable
    String.dispose(greeting);
    String.dispose(name);
    String.dispose(message);
    String.dispose(fmt);
    
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

### String.dupe

Create a new string from a C string literal.

```c
string str = String.dupe("Hello, World!");
printf("%s\n", str);  // "Hello, World!"
String.dispose(str);
```

**NULL handling:**
```c
string null_str = String.dupe(NULL);  // Returns NULL
// No dispose needed for NULL
```

**Empty string:**
```c
string empty = String.dupe("");
printf("Length: %zu\n", String.length(empty));  // "Length: 0"
String.dispose(empty);
```

---

### String.copy

Duplicate an existing string (deep copy).

```c
string original = String.dupe("Original");
string copy = String.copy(original);

printf("Original: %s\n", original);  // "Original"
printf("Copy: %s\n", copy);          // "Original"

// Modify one doesn't affect the other
// (both are independent allocations)

String.dispose(original);
String.dispose(copy);
```

**Use case:** Pass string ownership to different subsystems.

---

### String.length

Get the length of a string (character count, excluding null terminator).

```c
string str = String.dupe("Hello");
usize len = String.length(str);
printf("Length: %zu\n", len);  // "Length: 5"
String.dispose(str);
```

**NULL safety:**
```c
usize len = String.length(NULL);  // Returns 0
```

---

### String.concat

Concatenate two strings into a new string.

```c
string first = String.dupe("Hello");
string second = String.dupe("World");
string combined = String.concat(first, second);

printf("%s\n", combined);  // "HelloWorld"

String.dispose(first);
String.dispose(second);
String.dispose(combined);
```

**Add spacing:**
```c
string space = String.dupe(" ");
string hello = String.dupe("Hello");
string world = String.dupe("World");

string temp = String.concat(hello, space);
string result = String.concat(temp, world);

printf("%s\n", result);  // "Hello World"

String.dispose(space);
String.dispose(hello);
String.dispose(world);
String.dispose(temp);
String.dispose(result);
```

---

### String.compare

Compare two strings (wrapper around `strcmp`).

```c
string a = String.dupe("apple");
string b = String.dupe("banana");
string c = String.dupe("apple");

int cmp1 = String.compare(a, b);  // < 0 (a comes before b)
int cmp2 = String.compare(a, c);  // == 0 (equal)
int cmp3 = String.compare(b, a);  // > 0 (b comes after a)

if (String.compare(a, c) == 0) {
    printf("Strings are equal\n");
}

String.dispose(a);
String.dispose(b);
String.dispose(c);
```

**NULL handling:**
```c
String.compare(NULL, NULL);  // 0 (considered equal)
String.compare("hello", NULL);  // > 0
String.compare(NULL, "hello");  // < 0
```

---

### String.format

Create formatted strings (printf-style).

```c
string name = String.dupe("Alice");
int age = 30;
double height = 5.7;

string fmt = String.dupe("%s is %d years old and %.1f feet tall");
string bio = String.format(fmt, name, age, height);
printf("%s\n", bio);
// "Alice is 30 years old and 5.7 feet tall"

String.dispose(name);
String.dispose(bio);
String.dispose(fmt);
```

**Complex formatting:**
```c
string fmt = String.dupe(
    "%-10s | %5d | %8.2f\n"
    "%-10s | %5d | %8.2f\n"
);
string report = String.format(
    fmt,
    "Item A", 100, 1234.56,
    "Item B", 200, 9876.54
);
printf("%s", report);
/*
Item A     |   100 |  1234.56
Item B     |   200 |  9876.54
*/
String.dispose(report);
String.dispose(fmt);
```

---

### String.to_array

Convert string to char array (creates independent copy).

```c
string str = String.dupe("Hello");
char *arr = String.to_array(str);

printf("String: %s\n", str);  // "Hello"
printf("Array: %s\n", arr);   // "Hello"

// arr is independent of str
arr[0] = 'J';
printf("String: %s\n", str);  // "Hello" (unchanged)
printf("Array: %s\n", arr);   // "Jello"

String.dispose(str);
free(arr);  // char arrays use free() directly
```

**Use case:** Interfacing with APIs that require `char*` ownership.

---

### String.dispose

Free string memory. Always call for allocated strings.

```c
string str = String.dupe("Temporary");
// ... use str ...
String.dispose(str);
```

**NULL safety:**
```c
String.dispose(NULL);  // Safe, no-op
```

**Stack/literal strings:**
```c
const char *literal = "Stack string";
// String.dispose((string)literal);  // ❌ NEVER DO THIS
// Only dispose heap-allocated strings from this library
```

---

## StringBuilder API

StringBuilder provides efficient string building with automatic buffer management.

### StringBuilder.new

Create a new StringBuilder with initial capacity.

```c
string_builder sb = StringBuilder.new(64);  // 64-byte initial capacity
// ... use sb ...
StringBuilder.dispose(sb);
```

**Default capacity:**
```c
string_builder sb = StringBuilder.new(0);  // Uses default (16 bytes)
StringBuilder.dispose(sb);
```

---

### StringBuilder.snew

Create StringBuilder from existing string.

```c
string initial = String.dupe("Starting text");
string_builder sb = StringBuilder.snew(initial);

StringBuilder.append(sb, " - more content");
string result = StringBuilder.toString(sb);

printf("%s\n", result);  // "Starting text - more content"

String.dispose(initial);
String.dispose(result);
StringBuilder.dispose(sb);
```

---

### StringBuilder.append

Append a string to the builder.

```c
string_builder sb = StringBuilder.new(0);

StringBuilder.append(sb, "Hello");
StringBuilder.append(sb, " ");
StringBuilder.append(sb, "World");

string result = StringBuilder.toString(sb);
printf("%s\n", result);  // "Hello World"

String.dispose(result);
StringBuilder.dispose(sb);
```

**NULL safety:**
```c
StringBuilder.append(sb, NULL);  // Safe, no-op
```

---

### StringBuilder.appendf

Append formatted string (printf-style).

```c
string_builder sb = StringBuilder.new(0);

StringBuilder.append(sb, "Report:\n");
string fmt1 = String.dupe("  Count: %d\n");
StringBuilder.appendf(sb, fmt1, 42);
string fmt2 = String.dupe("  Rate: %.2f%%\n");
StringBuilder.appendf(sb, fmt2, 98.5);

string result = StringBuilder.toString(sb);
printf("%s", result);
/*
Report:
  Count: 42
  Rate: 98.50%
*/

String.dispose(result);
String.dispose(fmt1);
String.dispose(fmt2);
StringBuilder.dispose(sb);
```

---

### StringBuilder.appendl

Append a line (automatically adds newline).

```c
string_builder sb = StringBuilder.new(0);

StringBuilder.appendl(sb, "Line 1");
StringBuilder.appendl(sb, "Line 2");
StringBuilder.appendl(sb, "Line 3");

string result = StringBuilder.toString(sb);
printf("%s", result);
/*
Line 1
Line 2
Line 3
*/

String.dispose(result);
StringBuilder.dispose(sb);
```

---

### StringBuilder.lappends

Append multiple lines at once (variadic, NULL-terminated).

```c
string_builder sb = StringBuilder.new(0);

StringBuilder.append(sb, "Header\n");
StringBuilder.lappends(sb, 
    "First line",
    "Second line",
    "Third line",
    NULL  // ⚠️ MUST end with NULL
);

string result = StringBuilder.toString(sb);
printf("%s", result);
/*
Header
First line
Second line
Third line
*/

String.dispose(result);
StringBuilder.dispose(sb);
```

**⚠️ Warning:** Forgetting `NULL` terminator causes undefined behavior.

---

### StringBuilder.lappendf

Append formatted line (printf-style with newline).

```c
string_builder sb = StringBuilder.new(0);
string fmt = String.dupe("Item %d: Value = %d");

for (int i = 0; i < 3; i++) {
    StringBuilder.lappendf(sb, fmt, i, i * 10);
}

string result = StringBuilder.toString(sb);
printf("%s", result);
/*
Item 0: Value = 0
Item 1: Value = 10
Item 2: Value = 20
*/

String.dispose(result);
String.dispose(fmt);
StringBuilder.dispose(sb);
```

---

### StringBuilder.clear

Clear content but keep capacity (fast reset).

```c
string_builder sb = StringBuilder.new(128);

// Build first message
StringBuilder.append(sb, "First message");
string msg1 = StringBuilder.toString(sb);
printf("%s\n", msg1);
String.dispose(msg1);

// Reuse builder
StringBuilder.clear(sb);
StringBuilder.append(sb, "Second message");
string msg2 = StringBuilder.toString(sb);
printf("%s\n", msg2);
String.dispose(msg2);

StringBuilder.dispose(sb);
```

**Use case:** Reuse builder in loops without reallocation overhead.

---

### StringBuilder.toString

Extract final string from builder (creates copy).

```c
string_builder sb = StringBuilder.new(0);
StringBuilder.append(sb, "Content");

string result = StringBuilder.toString(sb);
printf("%s\n", result);  // "Content"

// Builder still valid, can keep using
StringBuilder.append(sb, " More");
string result2 = StringBuilder.toString(sb);
printf("%s\n", result2);  // "Content More"

String.dispose(result);
String.dispose(result2);
StringBuilder.dispose(sb);
```

---

### StringBuilder.toStream

Write builder content to FILE stream (no allocation).

```c
string_builder sb = StringBuilder.new(0);
StringBuilder.appendl(sb, "Log entry 1");
StringBuilder.appendl(sb, "Log entry 2");

// Write to file
FILE *f = fopen("output.log", "w");
if (f) {
    StringBuilder.toStream(sb, f);
    fclose(f);
}

// Write to stdout
StringBuilder.toStream(sb, stdout);

StringBuilder.dispose(sb);
```

**Use case:** Direct streaming without intermediate string allocation.

---

### StringBuilder.length

Get current content length.

```c
string_builder sb = StringBuilder.new(0);
StringBuilder.append(sb, "Hello");

usize len = StringBuilder.length(sb);
printf("Length: %zu\n", len);  // "Length: 5"

StringBuilder.dispose(sb);
```

---

### StringBuilder.capacity

Get current buffer capacity.

```c
string_builder sb = StringBuilder.new(32);

usize cap = StringBuilder.capacity(sb);
printf("Capacity: %zu\n", cap);  // "Capacity: 32"

StringBuilder.dispose(sb);
```

---

### StringBuilder.setCapacity

Manually set buffer capacity (grow or shrink).

```c
string_builder sb = StringBuilder.new(16);
printf("Initial: %zu\n", StringBuilder.capacity(sb));  // 16

// Grow
StringBuilder.setCapacity(sb, 128);
printf("After grow: %zu\n", StringBuilder.capacity(sb));  // 128

// Shrink (keeps content if it fits)
StringBuilder.setCapacity(sb, 32);
printf("After shrink: %zu\n", StringBuilder.capacity(sb));  // 32

StringBuilder.dispose(sb);
```

**Use case:** Pre-allocate for large content to avoid reallocations.

---

### StringBuilder.dispose

Free StringBuilder and internal buffers.

```c
string_builder sb = StringBuilder.new(0);
// ... use sb ...
StringBuilder.dispose(sb);
```

**NULL safety:**
```c
StringBuilder.dispose(NULL);  // Safe, no-op
```

---

## Common Patterns

### Building SQL Queries

```c
string_builder query = StringBuilder.new(256);

StringBuilder.append(query, "SELECT * FROM users WHERE ");
string fmt1 = String.dupe("age > %d AND ");
StringBuilder.appendf(query, fmt1, 18);
string fmt2 = String.dupe("country = '%s'");
StringBuilder.appendf(query, fmt2, "US");

string sql = StringBuilder.toString(query);
printf("%s\n", sql);
// "SELECT * FROM users WHERE age > 18 AND country = 'US'"

String.dispose(sql);
String.dispose(fmt1);
String.dispose(fmt2);
StringBuilder.dispose(query);
```

---

### Building JSON

```c
string_builder json = StringBuilder.new(0);

StringBuilder.appendl(json, "{");
string fmt1 = String.dupe("  \"name\": \"%s\",");
StringBuilder.lappendf(json, fmt1, "Alice");
string fmt2 = String.dupe("  \"age\": %d,");
StringBuilder.lappendf(json, fmt2, 30);
StringBuilder.appendl(json, "  \"active\": true");
StringBuilder.append(json, "}");

string result = StringBuilder.toString(json);
printf("%s\n", result);

String.dispose(result);
String.dispose(fmt1);
String.dispose(fmt2);
StringBuilder.dispose(json);
```

---

### CSV Generation

```c
string_builder csv = StringBuilder.new(1024);

// Header
StringBuilder.appendl(csv, "Name,Age,Score");

// Data rows
const char *names[] = {"Alice", "Bob", "Carol"};
int ages[] = {25, 30, 28};
double scores[] = {95.5, 87.3, 92.1};

string fmt = String.dupe("%s,%d,%.1f");
for (int i = 0; i < 3; i++) {
    StringBuilder.lappendf(csv, fmt, 
                           names[i], ages[i], scores[i]);
}

// Write to file
FILE *f = fopen("data.csv", "w");
if (f) {
    StringBuilder.toStream(csv, f);
    fclose(f);
}
String.dispose(fmt);

StringBuilder.dispose(csv);
```

---

### Configuration File Builder

```c
string_builder config = StringBuilder.new(512);

StringBuilder.appendl(config, "[server]");
string fmt1 = String.dupe("host = %s");
StringBuilder.lappendf(config, fmt1, "localhost");
string fmt2 = String.dupe("port = %d");
StringBuilder.lappendf(config, fmt2, 8080);
StringBuilder.appendl(config, "");
StringBuilder.appendl(config, "[database]");
string fmt3 = String.dupe("url = %s");
StringBuilder.lappendf(config, fmt3, "postgresql://localhost/mydb");

string result = StringBuilder.toString(config);
printf("%s\n", result);

String.dispose(result);
String.dispose(fmt1);
String.dispose(fmt2);
String.dispose(fmt3);
StringBuilder.dispose(config);
```

---

### Path Manipulation

```c
string base = String.dupe("/home/user");
string sep = String.dupe("/");
string dir = String.dupe("documents");
string file = String.dupe("readme.txt");

// Build path
string temp1 = String.concat(base, sep);
string temp2 = String.concat(temp1, dir);
string temp3 = String.concat(temp2, sep);
string path = String.concat(temp3, file);

printf("%s\n", path);  // "/home/user/documents/readme.txt"

String.dispose(base);
String.dispose(sep);
String.dispose(dir);
String.dispose(file);
String.dispose(temp1);
String.dispose(temp2);
String.dispose(temp3);
String.dispose(path);
```

**Better with StringBuilder:**
```c
string_builder sb = StringBuilder.new(128);
StringBuilder.append(sb, "/home/user");
StringBuilder.append(sb, "/");
StringBuilder.append(sb, "documents");
StringBuilder.append(sb, "/");
StringBuilder.append(sb, "readme.txt");

string path = StringBuilder.toString(sb);
printf("%s\n", path);

String.dispose(path);
StringBuilder.dispose(sb);
```

---

## Memory Management

### Rules

1. **Always dispose heap-allocated strings:**
   ```c
   string str = String.dupe("text");
   String.dispose(str);  // ✅ Required
   ```

2. **Never dispose stack/literal strings:**
   ```c
   const char *literal = "text";
   // String.dispose((string)literal);  // ❌ Undefined behavior
   ```

3. **NULL disposal is safe:**
   ```c
   String.dispose(NULL);  // ✅ No-op
   StringBuilder.dispose(NULL);  // ✅ No-op
   ```

4. **One allocation = one disposal:**
   ```c
   string a = String.dupe("text");
   string b = String.copy(a);
   String.dispose(a);  // ✅
   String.dispose(b);  // ✅
   ```

---

### Ownership Conventions

**Caller owns returned strings:**
```c
string fmt = String.dupe("Value: %d");
string result = String.format(fmt, 42);
// Caller must dispose
String.dispose(result);
String.dispose(fmt);
```

**StringBuilder extract creates copy:**
```c
string_builder sb = StringBuilder.new(0);
StringBuilder.append(sb, "text");
string s = StringBuilder.toString(sb);  // Creates new allocation
// Now have 2 independent allocations
String.dispose(s);
StringBuilder.dispose(sb);
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
    
    a = String.dupe(input);
    if (!a) goto cleanup;
    
    fmt = String.dupe("Processed: %s");
    if (!fmt) goto cleanup;
    
    b = String.format(fmt, a);
    if (!b) goto cleanup;
    
    result = String.concat(a, b);
    if (!result) goto cleanup;
    
    // Success path
    status = 0;
    
cleanup:
    String.dispose(a);
    String.dispose(b);
    String.dispose(result);
    String.dispose(fmt);
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
string str = String.dupe("text");
if (!str) {
    fprintf(stderr, "Allocation failed\n");
    return -1;
}
// ... use str ...
String.dispose(str);
```

---

### Defensive Checks

```c
string safe_concat(string a, string b) {
    if (!a || !b) {
        return NULL;
    }
    return String.concat(a, b);
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
string_builder sb = StringBuilder.new(64);
if (!sb) {
    fprintf(stderr, "Failed to create StringBuilder\n");
    return -1;
}

// ... use sb ...

string result = StringBuilder.toString(sb);
if (!result) {
    fprintf(stderr, "Failed to extract string\n");
    StringBuilder.dispose(sb);
    return -1;
}

// Success
printf("%s\n", result);
String.dispose(result);
StringBuilder.dispose(sb);
```

---

## API Quick Reference

### String Functions (8)

| Function | Description |
|----------|-------------|
| `String.dupe(cstr)` | Create string from C string |
| `String.copy(src)` | Duplicate existing string |
| `String.dispose(s)` | Free string memory |
| `String.length(s)` | Get string length |
| `String.concat(s1, s2)` | Concatenate strings |
| `String.compare(s1, s2)` | Compare strings |
| `String.format(fmt, ...)` | Create formatted string (fmt is string) |
| `String.to_array(s)` | Convert to char array |

---

### StringBuilder Functions (14)

| Function | Description |
|----------|-------------|
| `StringBuilder.new(capacity)` | Create new builder |
| `StringBuilder.snew(str)` | Create from string |
| `StringBuilder.dispose(sb)` | Free builder |
| `StringBuilder.append(sb, str)` | Append string |
| `StringBuilder.appendf(sb, fmt, ...)` | Append formatted (fmt is string) |
| `StringBuilder.appendl(sb, line)` | Append line |
| `StringBuilder.lappends(sb, ...)` | Append multiple lines |
| `StringBuilder.lappendf(sb, fmt, ...)` | Append formatted line (fmt is string) |
| `StringBuilder.clear(sb)` | Clear content |
| `StringBuilder.toString(sb)` | Extract string |
| `StringBuilder.toStream(sb, file)` | Write to stream |
| `StringBuilder.length(sb)` | Get content length |
| `StringBuilder.capacity(sb)` | Get buffer capacity |
| `StringBuilder.setCapacity(sb, cap)` | Set capacity |

---

## Additional Resources

- **Build/Test Guide:** `docs/sigma.string.a.md`
- **API Header:** `include/string_malloc.h`
- **Implementation:** `src/string_malloc.c`
- **Test Suite:** `test/standalone/test_string_malloc.c`

---

**Zero dependencies. Pure malloc. ABI compatible with sigma.core ecosystem.**
