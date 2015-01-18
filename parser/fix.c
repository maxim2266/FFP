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
#include <time.h>
#include <assert.h>

// string buffer ----------------------------------------------------------------------------------
static
void realloc_string_buffer(struct string_buffer* s, size_t n)
{
	s->str = REALLOC(char, s->str, n);
	s->capacity = n;
}

void copy_bytes_to_string_buffer(struct string_buffer* s, const char* bytes, size_t n)
{
	if(s->capacity < n)
		realloc_string_buffer(s, n);

	memcpy(s->str, bytes, n);
	s->size = n;
}

char append_bytes_to_string_buffer_with_checksum(struct string_buffer* sb, const char* s, size_t n)
{
	char* p;
	const char* const end = s + n;
	char sum = 0;

	if(sb->capacity < n + sb->size)
		realloc_string_buffer(sb, n + sb->size);

	p = sb->str + sb->size;
	sb->size += n;

	while(s != end)
		sum += (*p++ = *s++);

	return sum;
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
		integer = integer * 10 + (unsigned char)(*s - '0');

	num_int = s - base;

	if(*s == '.')	// decimal point
	{
		// fractional part
		for(base = ++s; *s >= '0' && *s <= '9'; ++s)
			integer = integer * 10 + (unsigned char)(*s - '0');

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
	static const double mult[] = { 0., 1e-1, 1e-2, 1e-3, 1e-4, 1e-5, 1e-6, 1e-7, 1e-8, 1e-9, 1e-10, 1e-11, 1e-12, 1e-13, 1e-14, 1e-15 };

	int64_t val;
	int num_frac;

	if(!p_value)
		return -1;

	num_frac = get_fix_tag_as_real(node, tag, &val);

	assert(num_frac <= 15);

	if(num_frac > 0)
		*p_value = mult[num_frac] * (double)val;
	else if(num_frac == 0)
		*p_value = (double)val;
	
	return num_frac;
}

int get_fix_tag_as_boolean(const struct fix_group_node* node, size_t tag)
{
	const struct fix_tag* pt;

	if(!node)
		return -1;

	pt = get_fix_tag(node, tag);

	if(!pt || !pt->value || pt->length != 1)
		return -1;

	switch(*pt->value)
	{
	case 'Y':
		return 1;
	case 'N':
		return 0;
	default:
		return -1;
	}
}

// time conversion functions
#define IS_DIGIT(c)		((c) >= '0' && (c) <= '9')
#define DIGIT_TO_INT(c)	((unsigned char)((c) - '0'))

#define READ_2_DIGITS(r)	\
	if(!IS_DIGIT(s[0]) || !IS_DIGIT(s[1]))	\
		return (int64_t)-1;	\
	r = 10 * DIGIT_TO_INT(s[0]) + DIGIT_TO_INT(s[1]);	\
	s += 2

#define READ_3_DIGITS(r)	\
	if(!IS_DIGIT(s[0]) || !IS_DIGIT(s[1]) || !IS_DIGIT(s[2]))	\
		return (int64_t)-1;	\
	r = 100 * DIGIT_TO_INT(s[0]) + 10 * DIGIT_TO_INT(s[1]) + DIGIT_TO_INT(s[2]);	\
	s += 3

#define READ_4_DIGITS(r)	\
	if(!IS_DIGIT(s[0]) || !IS_DIGIT(s[1]) || !IS_DIGIT(s[2]) || !IS_DIGIT(s[3]))	\
		return (int64_t)-1;	\
	r = 1000 * DIGIT_TO_INT(s[0]) + 100 * DIGIT_TO_INT(s[1]) + 10 * DIGIT_TO_INT(s[2]) + DIGIT_TO_INT(s[3]);	\
	s += 4

#define MATCH(c)	\
	if(*s++ != c)	\
		return (int64_t)-1

#ifdef _WIN32

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

int64_t get_fix_tag_as_utc_timestamp(const struct fix_group_node* node, size_t tag)
{
	/* From the spec:
		String field representing Time/date combination represented in UTC (Universal Time Coordinated, also known as "GMT") 
		in either YYYYMMDD-HH:MM:SS (whole seconds) or YYYYMMDD-HH:MM:SS.sss (milliseconds) format, colons, dash, and period required.

		Valid values:
		* YYYY = 0000-9999, MM = 01-12, DD = 01-31, HH = 00-23, MM = 00-59, SS = 00-60 (60 only if UTC leap second) (without milliseconds).
		* YYYY = 0000-9999, MM = 01-12, DD = 01-31, HH = 00-23, MM = 00-59, SS = 00-60 (60 only if UTC leap second), sss=000-999 (indicating milliseconds).
	*/

	SYSTEMTIME st;
	FILETIME ft;
	const char* s = get_fix_tag_as_string(node, tag);

	if(!s)
		return (int64_t)-1;

	READ_4_DIGITS(st.wYear);
	READ_2_DIGITS(st.wMonth);
	READ_2_DIGITS(st.wDay);
	MATCH('-');
	READ_2_DIGITS(st.wHour);
	MATCH(':');
	READ_2_DIGITS(st.wMinute);
	MATCH(':');
	READ_2_DIGITS(st.wSecond);

	switch(*s++)
	{
	case 0:
		st.wMilliseconds = 0;
		break;
	case '.':
		READ_3_DIGITS(st.wMilliseconds);
		MATCH(0);
		break;
	default:
		return (int64_t)-1;
	}

	st.wDayOfWeek = 0;

	return SystemTimeToFileTime(&st, &ft) ? 
		(((int64_t)ft.dwHighDateTime << 32) | (int64_t)ft.dwLowDateTime)
		: 
		(int64_t)-1;
}

#else	// Linux

int64_t get_fix_tag_as_utc_timestamp(const struct fix_group_node* node, size_t tag)
{
	/* From the spec:
		String field representing Time/date combination represented in UTC (Universal Time Coordinated, also known as "GMT") 
		in either YYYYMMDD-HH:MM:SS (whole seconds) or YYYYMMDD-HH:MM:SS.sss (milliseconds) format, colons, dash, and period required.

		Valid values:
		* YYYY = 0000-9999, MM = 01-12, DD = 01-31, HH = 00-23, MM = 00-59, SS = 00-60 (60 only if UTC leap second) (without milliseconds).
		* YYYY = 0000-9999, MM = 01-12, DD = 01-31, HH = 00-23, MM = 00-59, SS = 00-60 (60 only if UTC leap second), sss=000-999 (indicating milliseconds).
	*/

	struct tm t;
	time_t sec;
	int64_t frac;
	const char* s = get_fix_tag_as_string(node, tag);

	if(!s)
		return (int64_t)-1;

	READ_4_DIGITS(t.tm_year);
	READ_2_DIGITS(t.tm_mon);
	READ_2_DIGITS(t.tm_mday);
	MATCH('-');
	READ_2_DIGITS(t.tm_hour);
	MATCH(':');
	READ_2_DIGITS(t.tm_min);
	MATCH(':');
	READ_2_DIGITS(t.tm_sec);

	// correction
	t.tm_year -= 1900;
	t.tm_mon -= 1;
	t.tm_yday = t.tm_isdst = 0;
	
	// convert
	sec = timegm(&t);
	
	if(sec == (time_t)-1)
		return (int64_t)-1;

	// milliseconds
	switch(*s++)
	{
	case 0:
		return (int64_t)sec * 10000000;	// 100 ns intervals
	case '.':
		READ_3_DIGITS(frac);
		MATCH(0);
		return (int64_t)sec * 10000000 + frac * 10000;	// 100 ns intervals
	default:
		return (int64_t)-1;
	}
}

#endif	// Linux

time_t get_fix_tag_as_local_mkt_date(const struct fix_group_node* node, size_t tag)
{
	/* From the spec:
		String field represening a Date of Local Market (as oppose to UTC) in YYYYMMDD format. This is the "normal" date field used by the FIX Protocol.
		Valid values:
		YYYY = 0000-9999, MM = 01-12, DD = 01-31.
	*/

	struct tm t;
	const char* s = get_fix_tag_as_string(node, tag);

	if(!s)
		return (int64_t)-1;

	READ_4_DIGITS(t.tm_year);
	READ_2_DIGITS(t.tm_mon);
	READ_2_DIGITS(t.tm_mday);
	MATCH(0);

	t.tm_year -= 1900;
	t.tm_mon -= 1;
	t.tm_hour = t.tm_min = t.tm_sec = t.tm_yday = t.tm_isdst = 0;

	return mktime(&t);
}

