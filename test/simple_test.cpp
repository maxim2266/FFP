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

#include "../parser/fix_parser_impl.h"
#include "test_utils.h"

#include <string.h>
#include <stdexcept>

// FIX message for tests
static const char msg[] = "8=FIX.4.4\x01" "9=122\x01" "35=D\x01" "34=215\x01" "49=CLIENT12\x01" "52=20100225-19:41:57.316\x01" "56=B\x01" 
						  "1=Marcel\x01" "11=13346\x01" "21=1\x01" "40=2\x01" "44=5\x01" "54=1\x01" "59=0\x01" "60=20100225-19:39:52.020\x01" "10=072\x01";

static const char* const msg_begin = &msg[21];
static const char* const msg_end = &msg[sizeof(msg) - 8];

static
void ensure_buffer(const string_buffer& buffer)
{
	for(int i = 0; i < msg_end - msg_begin; ++i)
	{
		if(msg_begin[i] == SOH)
			ensure(buffer.str[i] == (char)0);
		else
			ensure(buffer.str[i] == msg_begin[i]);
	}
}

static
void validate_message(const struct fix_message* pm)
{
	const fix_parser* const parser = (const fix_parser*) ((const char*)pm - offsetof(fix_parser, message));

	ensure(pm->version == FIX_4_4);
	ensure(strcmp(pm->type, "D") == 0);
	ensure_buffer(parser->buffer);

	const fix_group_node* node = get_fix_message_root_node(pm);

	ensure(node != nullptr);
	ensure(get_fix_node_size(node) == 12);
	ensure_tag(node, 34, "215");
	ensure_tag(node, 49, "CLIENT12");
	ensure_tag_as_utc_timestamp(node, 52, "20100225-19:41:57.316");
	ensure_tag(node, 56, "B");
	ensure_tag(node, 1, "Marcel");
	ensure_tag(node, 11, "13346");
	ensure_tag(node, 21, "1");
	ensure_tag(node, 40, "2");
	ensure_tag(node, 44, "5");
	ensure_tag(node, 54, "1");
	ensure_tag(node, 59, "0");
	ensure_tag_as_utc_timestamp(node, 60, "20100225-19:39:52.020");
}

static
void basic_test()
{
	fix_parser* parser = create_fix_parser(get_dummy_classifier);
	const fix_message* pm = get_first_fix_message(parser, msg, sizeof(msg) - 1);

	ensure(pm != nullptr);
	ensure(!get_fix_parser_error(parser));

	validate_message(pm);

	ensure(!get_next_fix_message(parser));
	free_fix_parser(parser);
}

static
void splitter_test()
{
	std::string s(msg);

	s += msg;
	s += msg;
	s += msg;

	fix_parser* parser = create_fix_parser(get_dummy_classifier);

	ensure(parser != nullptr);

	const int n = 100;
	size_t counter = 0;
	const char* p = s.c_str();
	const char* const end = p + s.size();

	for(; p < end; p += n)
	{
		for(const fix_message* pm = get_first_fix_message(parser, p, std::min(n, end - p)); pm; pm = get_next_fix_message(parser))
		{
			ensure(!get_fix_parser_error(parser));
			validate_message(pm);
			++counter;
		}
	}

	free_fix_parser(parser);
	ensure(counter == 4);
}

static
void invalid_message_test()
{
	const char m[] = "8=FIX.4.4\x01" "9=122\x01" "35=D\x02";

	fix_parser* parser = create_fix_parser(get_dummy_classifier);
	const fix_message* pm = get_first_fix_message(parser, m, sizeof(m) - 1);

	ensure(!pm);
	ensure(strcmp(get_fix_parser_error(parser), "Unexpected byte 0x2 in FIX message type") == 0);
	ensure(!get_next_fix_message(parser));
	free_fix_parser(parser);
}

// batch
void all_simple_tests()
{
	basic_test();
	splitter_test();
	invalid_message_test();
}
