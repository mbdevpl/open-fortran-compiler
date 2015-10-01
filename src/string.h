#ifndef __string_h__
#define __string_h__

#include <stdbool.h>

typedef struct
{
	char*    base;
	unsigned size;
} string_t;

string_t string_create(const char* base, unsigned size);
string_t string_copy(const string_t src);
void     string_delete(string_t string);

bool string_empty(const string_t string);

/* May return truncated string if string contains nul characters. */
const char* string_strz(const string_t string);

unsigned string_length(const string_t string);
bool     string_equal(const string_t a, const string_t b);

bool string_printf(string_t string,	const char* format, ...);

bool string_print(int fd, const string_t string);
bool string_print_escaped(int fd, const string_t string);

#endif
