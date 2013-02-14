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

#include <stddef.h>
#include <stdint.h>

#ifndef _MSC_VER
#include <time.h>	// for time_t definition
#endif

#ifdef __cplusplus 
extern "C" 
{
#endif

// configuration
#define MAX_MESSAGE_LEN 100000
#define MAX_GROUP_DEPTH 10

// FIX version
typedef enum { FIX_4_2, FIX_4_3, FIX_4_4, FIX_5_0 } fix_message_version;

// FIX message attributes
struct fix_message
{
	const char* error;
	fix_message_version version;
	char type[4];
};

// FIX group node
struct fix_group_node;

// tag
struct fix_tag
{
	size_t tag, length;
	const char* value;
	struct fix_group_node* group;
};

// parser control table entry
struct fix_tag_classifier
{
	int (*is_valid_tag)(size_t tag);
	size_t (*get_data_tag)(size_t tag);
	int (*is_first_in_group)(size_t tag);
	const struct fix_tag_classifier* (*get_group_classifier)(size_t tag);
};

// parser control table entry
typedef const struct fix_tag_classifier* (*classifier_func)(fix_message_version version, const char* msg_type);

// parser
struct fix_parser;

// parser constructor
// given a classifier function returns a new parser instance
struct fix_parser* create_fix_parser(classifier_func cf);

// parser destructor
void free_fix_parser(struct fix_parser* parser);

// returns parser error or NULL
const char* get_fix_parser_error(struct fix_parser* parser);

// message iterator
const struct fix_message* get_first_fix_message(struct fix_parser* parser, const void* bytes, size_t n);
const struct fix_message* get_next_fix_message(struct fix_parser* parser);

// returns message root node
const struct fix_group_node* get_fix_message_root_node(const struct fix_message* msg);

// returns number of tags in a group node
size_t get_fix_node_size(const struct fix_group_node* node);

// group node iterator
const struct fix_group_node* get_next_fix_node(const struct fix_group_node* pnode);

// returns pointer to struct fix_tag or NULL if tag not found
const struct fix_tag* get_fix_tag(const struct fix_group_node* node, size_t tag);

// returns tag value as string or NULL if tag is not found
const char* get_fix_tag_as_string(const struct fix_group_node* node, size_t tag);

// converts the tag value to a floating point number, extracting the value as 64bit integer
// and returning the number of digits after the decimal point or -1 if conversion fails or tag not found.
// double = value / pow(10.0, num_frac);
int get_fix_tag_as_real(const struct fix_group_node* node, size_t tag, int64_t* p_value);

// converts the tag value to double,
// returns number of digits after the decimal point or -1 if conversion fails or tag not found.
int get_fix_tag_as_double(const struct fix_group_node* node, size_t tag, double* p_value);

// converts the tag value to a 64 bit signed integer and returns non-zero on success or 
// 0 if conversion fails or tag not found.
int get_fix_tag_as_integer(const struct fix_group_node* node, size_t tag, int64_t* p);

// interprets tag as boolean, returning
// 1 for "true"
// 0 for "false"
// -1 if conversion fails or tag not found
int get_fix_tag_as_boolean(const struct fix_group_node* node, size_t tag);

// time conversion functions
//// TODO: revise
// Treats the tag value as UTCTimestamp FIX type and returns the number of 100-nanosecond intervals since January 1, 1601 (for Windows platform),
// or -1LL if conversion fails or tag not found.
// Note: The range of dates is more narrow than in the FIX specification.
int64_t get_fix_tag_as_utc_timestamp(const struct fix_group_node* node, size_t tag);

// Treats the tag value as LocalMktDate FIX type and returns it as time_t or (time_t)-1 if conversion fails or tag not found.
time_t get_fix_tag_as_local_mkt_date(const struct fix_group_node* node, size_t tag);

// parser table helper macros
#define GROUP_NODE(name, first_tag)	\
	static int is_first_in_group_ ## name(size_t __tag) { return (__tag == (first_tag)) ? 1 : 0; }

#define MESSAGE(name)	\
	static int is_first_in_group_ ## name(size_t __tag) { (void)__tag; return 0; }

#define PARSER_TABLE(name)	\
	tag_table_ ## name

#define PARSER_TABLE_ADDRESS(name)	\
	&PARSER_TABLE(name)

#define END_NODE(name)	\
	static const struct fix_tag_classifier PARSER_TABLE(name) = { &is_valid_in_ ## name, &get_data_tag_in_ ## name, &is_first_in_group_ ## name, &get_group_classifier_in_ ## name }

#define END_MESSAGE(name)	\
	END_NODE(name)

#define VALID_TAGS(name)	\
	static int is_valid_in_ ## name(size_t __tag) { switch(__tag) {

#define TAG(t)	\
	case t:

#define END_VALID_TAGS	\
	return 1;	default: return 0; } }

#define DATA_TAGS(name)	\
	static size_t get_data_tag_in_ ## name(size_t __tag) { switch(__tag) {

#define DATA_TAG(length_tag, data_tag)	\
	case length_tag: return data_tag;
	
#define END_DATA_TAGS	\
	default: return 0; } }

#define NO_DATA_TAGS(name)	\
	static size_t get_data_tag_in_ ## name(size_t __tag) { (void)__tag; return 0; }

#define GROUPS(name)	\
	static const struct fix_tag_classifier* get_group_classifier_in_ ## name(size_t __tag) { switch(__tag) {

#define GROUP_TAG(length_tag, node_name)	\
	case length_tag: return &tag_table_ ## node_name;

#define END_GROUPS	\
	default: return NULL; } }

#define NO_GROUPS(name)	\
	static const struct fix_tag_classifier* get_group_classifier_in_ ## name(size_t __tag) { (void)__tag; return NULL; }

#ifdef __cplusplus 
}
#endif
