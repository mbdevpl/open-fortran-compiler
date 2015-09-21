#ifndef __parse_keyword_h__
#define __parse_keyword_h__

typedef enum
{
	PARSE_KEYWORD_PROGRAM = 0,
	PARSE_KEYWORD_SUBROUTINE,
	PARSE_KEYWORD_FUNCTION,
	PARSE_KEYWORD_IF,
	PARSE_KEYWORD_THEN,
	PARSE_KEYWORD_ELSE_IF,
	PARSE_KEYWORD_ELSE,
	PARSE_KEYWORD_GO_TO,
	PARSE_KEYWORD_DO,
	PARSE_KEYWORD_CONTINUE,
	PARSE_KEYWORD_STOP,
	PARSE_KEYWORD_PAUSE,

	PARSE_KEYWORD_LOGICAL,
	PARSE_KEYWORD_CHARACTER,
	PARSE_KEYWORD_INTEGER,
	PARSE_KEYWORD_REAL,
	PARSE_KEYWORD_COMPLEX,
	PARSE_KEYWORD_BYTE,
	PARSE_KEYWORD_DOUBLE_PRECISION,
	PARSE_KEYWORD_DOUBLE_COMPLEX,

	PARSE_KEYWORD_TRUE,
	PARSE_KEYWORD_FALSE,

	PARSE_KEYWORD_IMPLICIT,
	PARSE_KEYWORD_IMPLICIT_NONE,

	PARSE_KEYWORD_COMMON,
	PARSE_KEYWORD_DIMENSION,
	PARSE_KEYWORD_EQUIVALENCE,

	PARSE_KEYWORD_KIND,

	PARSE_KEYWORD_ASSIGN,
	PARSE_KEYWORD_TO,

	PARSE_KEYWORD_CALL,

	PARSE_KEYWORD_DATA,
	PARSE_KEYWORD_WRITE,
	PARSE_KEYWORD_READ,
	PARSE_KEYWORD_FORMAT,

	PARSE_KEYWORD_REWIND,
	PARSE_KEYWORD_UNIT,
	PARSE_KEYWORD_IOSTAT,
	PARSE_KEYWORD_ERR,

	PARSE_KEYWORD_COUNT
} parse_keyword_e;



unsigned parse_name(
	const sparse_t* src, const char* ptr,
	str_ref_t* name);


const char* parse_keyword_name(
	parse_keyword_e keyword);

unsigned parse_keyword(
	const sparse_t* src, const char* ptr,
	parse_keyword_e keyword);

unsigned parse_keyword_named(
	const sparse_t* src, const char* ptr,
	parse_keyword_e keyword,
	str_ref_t* name);

unsigned parse_keyword_end(
	const sparse_t* src, const char* ptr,
	parse_keyword_e keyword);

unsigned parse_keyword_end_named(
	const sparse_t* src, const char* ptr,
	parse_keyword_e keyword,
	str_ref_t* name);

#endif
