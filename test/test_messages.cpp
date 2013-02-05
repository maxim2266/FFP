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

#include "test_messages.h"

// test messages and specifications
// 1. simple message
const char simple_message[] = "8=FIX.4.4\x01" "9=122\x01" "35=D\x01" "34=215\x01" "49=CLIENT12\x01" "52=20100225-19:41:57.316\x01" "56=B\x01" 
							  "1=Marcel\x01" "11=13346\x01" "21=1\x01" "40=2\x01" "44=5\x01" "54=1\x01" "59=0\x01" "60=20100225-19:39:52.020\x01" "10=072\x01";

const size_t simple_message_size = sizeof(simple_message) - 1;

MESSAGE(simple_message_spec)
	VALID_TAGS(simple_message_spec)
		TAG(34)
		TAG(49)
		TAG(52)
		TAG(56)
		TAG(1)
		TAG(11)
		TAG(21)
		TAG(40)
		TAG(44)
		TAG(54)
		TAG(59)
		TAG(60)
	END_VALID_TAGS

	NO_DATA_TAGS(simple_message_spec)
	NO_GROUPS(simple_message_spec)
END_NODE(simple_message_spec);

const struct fix_tag_classifier* simple_message_classifier(fix_message_version version, const char* msg_type)
{
	return (version == FIX_4_4 && msg_type[0] == 'D' && msg_type[1] == 0) ? PARSER_TABLE_ADDRESS(simple_message_spec) : NULL; 
}

static const char* const msg_begin = &simple_message[21];
static const char* const msg_end = &simple_message[sizeof(simple_message) - 8];

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

void validate_simple_message(const fix_message* pm)
{
	ensure(pm->version == FIX_4_4);
	ensure(pm->type[0] == 'D' && pm->type[1] == 0);

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

std::string copy_simple_message(size_t n)
{
	std::string s(simple_message, simple_message_size);

	for(size_t i = 0; i < n - 1; ++i)
		s.append(simple_message, simple_message_size);

	return s;
}

// 2. message with groups
const char message_with_groups[] = "8=FIX.4.2\x01" "9=196\x01" "35=X\x01" "49=A\x01" "56=B\x01" "34=12\x01" "52=20100318-03:21:11.364\x01" "262=A\x01" "268=2\x01" "279=0\x01" "269=0\x01" "278=BID\x01" "55=EUR/USD\x01" "270=1.37215\x01" "15=EUR\x01" "271=2500000\x01" "346=1\x01" "279=0\x01" "269=1\x01" "278=OFFER\x01" "55=EUR/USD\x01" "270=1.37224\x01" "15=EUR\x01" "271=2503200\x01" "346=1\x01" "10=171\x01";

const size_t message_with_groups_size = sizeof(message_with_groups) - 1;

// parser specs
GROUP_NODE(my_node, 279)
	VALID_TAGS(my_node)
		TAG(279)
		TAG(269)
		TAG(278)
		TAG(55)
		TAG(270)
		TAG(15)
		TAG(271)
		TAG(346)
	END_VALID_TAGS

	NO_DATA_TAGS(my_node)
	NO_GROUPS(my_node)
END_NODE(my_node);

MESSAGE(my_root)
	VALID_TAGS(my_root)
		TAG(49)
		TAG(56)
		TAG(34)
		TAG(52)
		TAG(262)
		TAG(268)	// group
	END_VALID_TAGS

	NO_DATA_TAGS(my_root)

	GROUPS(my_root)
		GROUP_TAG(268, my_node)
	END_GROUPS
END_NODE(my_root);

const struct fix_tag_classifier* message_with_groups_classifier(fix_message_version version, const char* msg_type)
{
	return (version == FIX_4_2 && msg_type[0] == 'X' && msg_type[1] == 0) ? PARSER_TABLE_ADDRESS(my_root) : NULL; 
}

std::string copy_message_with_groups(size_t n)
{
	std::string s(message_with_groups, message_with_groups_size);

	for(size_t i = 0; i < n - 1; ++i)
		s.append(message_with_groups, message_with_groups_size);

	return s;
}

struct item_value
{
	size_t tag;
	tag_validator validator;
};

static
const struct item_value values[][8] = 
{
	{
		{ 279, make_validator(0LL) },
		{ 269, make_validator(0LL) },
		{ 278, make_validator("BID") },
		{ 55,  make_validator("EUR/USD") },
		{ 270, make_validator(137215LL, 5) },	// "1.37215"
		{ 15,  make_validator("EUR") },
		{ 271, make_validator(2500000LL) },
		{ 346, make_validator(1LL) }
	},
	{
		{ 279, make_validator(0LL) },
		{ 269, make_validator(1LL) },
		{ 278, make_validator("OFFER") },
		{ 55,  make_validator("EUR/USD") },
		{ 270, make_validator(137224LL, 5) },	// "1.37224"
		{ 15,  make_validator("EUR") },
		{ 271, make_validator(2503200LL) },
		{ 346, make_validator(1LL) }
	}
};

void validate_message_with_groups(const fix_message* pm)
{
	const fix_group_node* node = get_fix_message_root_node(pm);

	ensure(get_fix_node_size(node) == 6);
	ensure_tag(node, 49, "A");
	ensure_tag(node, 56, "B");
	ensure_tag_as_integer(node, 34, 12LL);
	ensure_tag_as_utc_timestamp(node, 52, "20100318-03:21:11.364");
	ensure_tag(node, 262, "A");

	size_t i = 0;

	for(node = ensure_group_tag(node, 268, 2); node; ++i, node = get_next_fix_node(node))
	{
		ensure(i < 2);
		ensure(get_fix_node_size(node) == 8);

		for(size_t j = 0; j < 8; ++j)
		{
			const item_value* p = &values[i][j];

			p->validator(node, p->tag);
		}
	}
}


