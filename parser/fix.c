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

#include "fix_parser_impl.h"

#include <malloc.h>
#include <memory.h>
#include <assert.h>

// string buffer ----------------------------------------------------------------------------------
static
void realloc_string_buffer(struct string_buffer* s, size_t n)
{
	s->str = REALLOC(char, s->str, n);
	s->capacity = MSIZE(s->str);
}

void copy_bytes_to_string_buffer(struct string_buffer* s, const char* bytes, size_t n)
{
	if(s->capacity < n)
		realloc_string_buffer(s, n);

	memcpy(s->str, bytes, n);
	s->size = n;
}

void append_char_to_string_buffer(struct string_buffer* s, char c)
{
	if(s->size == s->capacity)
		realloc_string_buffer(s, s->capacity > 0 ? (s->capacity * 2) : 300);

	s->str[s->size++] = c;
}

void set_buffer_empty(struct string_buffer* s)
{
	s->size = 0;
}

// real FIX message -------------------------------------------------------------------------------
void set_message_empty(struct real_fix_message* msg)
{
	msg->properties.error = NULL;
	msg->properties.type[0] = 0;
	set_group_node_empty(&msg->root);
	msg->complete = NO;
}

// FIX message ------------------------------------------------------------------------------------
const struct fix_group_node* get_fix_message_root_node(const struct fix_message* msg)
{
	return msg ? &((const struct real_fix_message*)msg)->root : NULL;	// sorry...
}

// helpers ----------------------------------------------------------------------------------------
// utility for reading tags and lengths
// returns pointer to the first non-digit or NULL on error
const char* read_fix_uint(const char* s, const char* const end, size_t* result_ptr)
{
	size_t r;

	assert(result_ptr && s && s <= end);

	if(s == end)	// empty string
		return NULL;

	if(*s < '1' || *s > '9')	// note: no leading zeroes
		return NULL;

	r = (unsigned char)(*s - '0');

	while(++s < end && *s >= '0' && *s <= '9')
	{
		const size_t rr = 10 * r + (unsigned char)(*s - '0');

		if(rr < r)	// overflow
			return NULL;

		r = rr;
	}

	*result_ptr = r;
	return s;
}

// conversion routines ----------------------------------------------------------------------------
const char* get_fix_tag_as_string(const struct fix_group_node* node, size_t tag)
{
	const struct fix_tag* pt;

	if(!node)
		return NULL;

	pt = get_fix_tag(node, tag);

	return pt ? pt->value : NULL;
}

int get_fix_tag_as_integer(const struct fix_group_node* node, size_t tag, int64_t* p)
{
	/* From the spec:	
		Sequence of digits without commas or decimals and optional sign character (ASCII characters "-" and "0" - "9" ). 
		The sign character utilizes one byte (i.e. positive int is "99999" while negative int is "-99999"). 
		Note that int values may contain leading zeros (e.g. "00023" = "23"). */

	int64_t r; 
	boolean positive;
	const char *s;

	if(!p)
		return 0;

	s = get_fix_tag_as_string(node, tag);

	if(!s)
		return 0;

	if(*s == '-')
	{
		++s;
		positive = NO;
	}
	else
		positive = YES;

	if(*s < '0' || *s > '9')
		return 0;

	r = (unsigned char)(*s - '0');

	for(++s; *s >= '0' && *s <= '9'; ++s)
	{
		const int64_t r2 = r * 10 + (unsigned char)(*s - '0');

		if(r2 < r)	// overflow
			return 0;

		r = r2;
	}

	if(*s != 0)
		return 0;

	*p = positive ? r : -r;
	return 1;
}

int get_fix_tag_as_real(const struct fix_group_node* node, size_t tag, int64_t* p_value)
{
	/* From the spec:
		Sequence of digits with optional decimal point and sign character (ASCII characters "-", "0" - "9" and "."); 
		the absence of the decimal point within the string will be interpreted as the float representation of an integer value. 
		All float fields must accommodate up to fifteen significant digits. The number of decimal places used should be 
		a factor of business/market needs and mutual agreement between counterparties. Note that float values may contain 
		leading zeros (e.g. "00023.23" = "23.23") and may contain or omit trailing zeros after the decimal 
		point (e.g. "23.0" = "23.0000" = "23" = "23."). */

	int64_t integer;
	boolean positive;
	int num_int, num_frac;	// number of digits in the integer and fractional parts
	const char *s, *base;

	if(!p_value)
		return -1;

	s = get_fix_tag_as_string(node, tag);

	if(!s)
		return -1;

	// sign
	if(*s == '-')
	{
		++s;
		positive = NO;
	}
	else
		positive = YES;

	base = s;

	// integer part
	if(*s < '0' || *s > '9')
		return -1;

	integer = (unsigned char)(*s - '0');

	for(++s; *s >= '0' && *s <= '9'; ++s)
		integer = integer * 10LL + (unsigned char)(*s - '0');

	num_int = s - base;

	if(*s == '.')	// decimal point
	{
		// fractional part
		for(base = ++s; *s >= '0' && *s <= '9'; ++s)
			integer = integer * 10LL + (unsigned char)(*s - '0');

		num_frac = s - base;
	}
	else
		num_frac = 0;

	if(*s != 0 || num_int + num_frac > 15)
		return -1;

	// all done
	*p_value = positive ? integer : -integer;

	return num_frac;
}

int get_fix_tag_as_double(const struct fix_group_node* node, size_t tag, double* p_value)
{
	if(p_value)
	{
		static const double mult[] = { 0., 1e-1, 1e-2, 1e-3, 1e-4, 1e-5, 1e-6, 1e-7, 1e-8, 1e-9, 1e-10, 1e-11, 1e-12, 1e-13, 1e-14, 1e-15 };

		int64_t val;
		const int num_frac = get_fix_tag_as_real(node, tag, &val);

		assert(num_frac <= 15);

		if(num_frac > 0)
			*p_value = mult[num_frac] * (double)val;
		else if(num_frac == 0)
			*p_value = (double)val;
	
		return num_frac;
	}

	return -1;
}


