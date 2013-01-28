/*
Copyright (c) 2013, Maxim Konakov
 All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list 
   of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list 
   of conditions and the following disclaimer in the documentation and/or other materials 
   provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS 
OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY 
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER 
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <crtdbg.h>

#include "../fix_parser.h"

typedef enum { NO, YES } boolean;

// string buffer ----------------------------------------------------------------------------------
struct string_buffer
{
	size_t size, capacity;
	char* str;
};

void append_char_to_string_buffer(struct string_buffer* s, char c);
void copy_bytes_to_string_buffer(struct string_buffer* s, const char* bytes, size_t n);
void set_buffer_empty(struct string_buffer* s);

// FIX message node -------------------------------------------------------------------------------
struct fix_group_node
{
	size_t size, cap_index;
	struct fix_tag* buff;
	struct fix_group_node* next;
};

struct fix_group_node* alloc_group_node();
void clear_group_node(struct fix_group_node* pnode);
void set_group_node_empty(struct fix_group_node* pnode);
struct fix_tag* add_fix_tag(struct fix_group_node* pnode, const struct fix_tag* new_tag);

// FIX message ------------------------------------------------------------------------------------
struct real_fix_message
{
	struct fix_message properties;
	struct fix_group_node root;
	boolean complete;
};

void set_message_empty(struct real_fix_message* msg);

// splitter ---------------------------------------------------------------------------------------
typedef enum { SP_HEADER, SP_FIX_4, SP_BODY_LENGTH, SP_MSG_TYPE, SP_MSG, SP_CHECK_SUM } splitter_state;

struct splitter_data
{
	const char* pattern;
	splitter_state state;
	size_t byte_counter, counter;
	char check_sum, their_sum;
};

void init_splitter(struct splitter_data* sp);
void read_message(struct fix_parser* parser);

// FIX parser -------------------------------------------------------------------------------------
struct fix_parser
{
	const char *ptr, *end, *error;
	classifier_func get_classifier;
	struct string_buffer buffer;
	struct real_fix_message message;
	struct splitter_data splitter;
};

void set_parser_error(struct fix_parser* parser, const char* text, size_t n);
void parse_message(struct fix_parser* parser);

// tag reader -------------------------------------------------------------------------------------
struct tag_reader
{
	char* ptr;
	const char* end;
	struct fix_tag current;
	boolean has_unread_tag;
	size_t recursion_level;
	struct fix_parser* parser;
};

typedef enum { TR_OK, TR_DONE, TR_ERROR = -1 } tag_reader_status;

void init_tag_reader(struct fix_parser* parser, struct tag_reader* reader);
tag_reader_status read_next_tag(struct tag_reader* reader);
tag_reader_status read_binary_tag(struct tag_reader* reader, size_t tag);

// helpers ----------------------------------------------------------------------------------------
#define ALLOC(type)				(type*)malloc(sizeof(type))
#define ALLOC_Z(type)			(type*)calloc(1, sizeof(type))
#define ALLOC_NZ(n, type)		(type*)calloc((n), sizeof(type))
#define REALLOC(type, ptr, n)	(type*)realloc((ptr), (n) * sizeof(type))
#define FREE(ptr)				free(ptr)
#define MSIZE(ptr)				_msize(ptr)

#define ZERO_FILL(ptr)	memset((ptr), 0, sizeof(*(ptr)))
#define STR(x) #x
#define CHAR_TO_INT(c) ((int)(unsigned char)(c))

#define SOH ((char)1)

void report_message_error(struct fix_parser* parser, const char* fmt, ...);
const char* read_fix_uint(const char* s, const char* const end, size_t* result_ptr);

