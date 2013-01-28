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

#include "test_utils.h"

#include <string.h>
#include <stdexcept>

// dummy classifier -------------------------------------------------------------------------------
static
int is_valid_tag(size_t)
{
	return 1;
}

static
size_t get_raw_data_tag(size_t)
{
	return 0;
}

static
int is_first_in_group(size_t)
{
	return 0;
}

static
const fix_tag_classifier* get_group_classifier(size_t)
{
	return NULL;
}

static 
fix_tag_classifier dummy_classifier = 
{
	&is_valid_tag,
	&get_raw_data_tag,
	&is_first_in_group,
	&get_group_classifier
};

const  fix_tag_classifier* get_dummy_classifier(fix_message_version, const char*)
{
	return &dummy_classifier;
}

// tag validation ---------------------------------------------------------------------------------
void ensure_tag(const fix_group_node* node, size_t tag, const char* value)
{
	const fix_tag* pt = get_fix_tag(node, tag);

	ensure(pt != nullptr);
	ensure(pt->tag == tag);
	ensure(pt->length == strlen(value));
	ensure(memcmp(pt->value, value, pt->length) == 0);
}

const struct fix_group_node* ensure_group_tag(const struct fix_group_node* node, size_t tag, size_t len)
{
	const fix_group_node* group;
	const fix_tag* pt = get_fix_tag(node, tag);

	ensure(pt != nullptr);
	ensure(pt->tag == tag);
	ensure(pt->length == len);
	ensure(!pt->value);

	group = pt->group;

	ensure(group);
	return group;
}

void ensure_tag_as_integer(const fix_group_node* node, size_t tag, const int64_t value)
{
	int64_t r;

	ensure(get_fix_tag_as_integer(node, tag, &r));
	ensure(r == value);
}

void ensure_tag_as_real(const fix_group_node* node, size_t tag, const int64_t value, int num_frac)
{
	int64_t my_value;
	double my_val, expected_val;

	// check as long long value
	ensure(get_fix_tag_as_real(node, tag, &my_value) == num_frac);
	ensure(my_value == value);

	// check conversion to double
	expected_val = value * pow(10.0, -num_frac);

	ensure(get_fix_tag_as_double(node, tag, &my_val) == num_frac);
	ensure(abs(expected_val - my_val) < pow(10.0, -num_frac));
}

// validator factories
tag_validator make_validator(const char* value)
{
	return [value](const fix_group_node* node, size_t tag)
	{
		ensure_tag(node, tag, value);
	};
}

tag_validator make_validator(const int64_t value)
{
	return [value](const fix_group_node* node, size_t tag)
	{
		ensure_tag_as_integer(node, tag, value);
	};
}

tag_validator make_validator(const int64_t value, int num_frac)
{
	return [value, num_frac](const fix_group_node* node, size_t tag)
	{
		ensure_tag_as_real(node, tag, value, num_frac);
	};
}

