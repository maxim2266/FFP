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

#include "../fix_parser.h"
#include <functional>

// assertion
template< class T >
inline
void ensure(T r, const char* msg = nullptr)
{
	if(!r)
		throw std::runtime_error(msg ? msg : "Assertion failed");
}

// empty classifier
const fix_tag_classifier* get_dummy_classifier(fix_message_version, const char*);

// tag validators
void ensure_tag(const fix_group_node* node, size_t tag, const char* value);
const struct fix_group_node* ensure_group_tag(const fix_group_node* node, size_t tag, size_t len);
void ensure_tag_as_integer(const fix_group_node* node, size_t tag, const int64_t value);

// validator factories
typedef std::function< void (const fix_group_node*, size_t) > tag_validator;

tag_validator make_validator(const char* value);
tag_validator make_validator(const int64_t value);
tag_validator make_validator(const int64_t value, int num_frac);
