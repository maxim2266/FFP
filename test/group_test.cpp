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

#include <time.h>
#include <string>

// message
static const char msg[] = "8=FIX.4.2\x01" "9=196\x01" "35=X\x01" "49=A\x01" "56=B\x01" "34=12\x01" "52=20100318-03:21:11.364\x01" "262=A\x01" "268=2\x01" "279=0\x01" "269=0\x01" "278=BID\x01" "55=EUR/USD\x01" "270=1.37215\x01" "15=EUR\x01" "271=2500000\x01" "346=1\x01" "279=0\x01" "269=1\x01" "278=OFFER\x01" "55=EUR/USD\x01" "270=1.37224\x01" "15=EUR\x01" "271=2503200\x01" "346=1\x01" "10=171\x01";

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

const struct fix_tag_classifier* my_classifier(fix_message_version version, const char* msg_type)
{
	return (version == FIX_4_2 && msg_type[0] == 'X' && msg_type[1] == 0) ? PARSER_TABLE_ADDRESS(my_root) : NULL; 
}

// test
struct item_value
{
	size_t tag;
	tag_validator validator;
};

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

static
void validate_my_message(const fix_message* pm)
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

static
void simple_group_test()
{
	fix_parser* parser = create_fix_parser(my_classifier);
	const fix_message* pm = get_first_fix_message(parser, msg, sizeof(msg) - 1);

	ensure(pm);
	ensure(pm->error == nullptr);
	validate_my_message(pm);
	ensure(!get_next_fix_message(parser));
	free_fix_parser(parser);
}

static
void simple_group_test2()
{
	const char m[] = "8=FIX.4.2\x01" "9=196\x01" "35=X\x01" "49=A\x01" "56=B\x01" "52=20100318-03:21:11.364\x01" "262=A\x01" "268=2\x01" "279=0\x01" "269=0\x01" "278=BID\x01" "55=EUR/USD\x01" "270=1.37215\x01" "15=EUR\x01" "271=2500000\x01" "346=1\x01" "279=0\x01" "269=1\x01" "278=OFFER\x01" "55=EUR/USD\x01" "270=1.37224\x01" "15=EUR\x01" "271=2503200\x01" "346=1\x01" "34=12\x01" "10=171\x01";
	fix_parser* parser = create_fix_parser(my_classifier);
	const fix_message* pm = get_first_fix_message(parser, m, sizeof(m) - 1);

	ensure(pm);
	ensure(pm->error == nullptr);
	validate_my_message(pm);
	ensure(!get_next_fix_message(parser));
	free_fix_parser(parser);
}

#define WITH_VALIDATION

static 
void speed_test()
{
	const int M = 10;
	std::string s(msg, sizeof(msg) - 1);

	for(int i = 0; i < M - 1; ++i)
		s.append(msg, sizeof(msg) - 1);

	const int step = 101, 
#ifdef _DEBUG
		N = 1000;
#else
		N = 100000;
#endif

	const char* const end = s.c_str() + s.size();
	fix_parser* const parser = create_fix_parser(my_classifier);
	int count = 0;
	clock_t t = clock();

	for(int i = 0; i < N; ++i)
	{
		for(const char* p = s.c_str(); p < end; p += step)
		{
			for(const fix_message* pm = get_first_fix_message(parser, p, std::min(step, end - p)); pm; pm = get_next_fix_message(parser))
			{
				ensure(!pm->error);
#ifdef WITH_VALIDATION
				validate_my_message(pm);
#else
				ensure(get_fix_node_size(get_fix_message_root_node(pm)) == 6);
#endif
				++count;
			}
	
			ensure(!get_fix_parser_error(parser));
		}
	}

	t = clock() - t;
	free_fix_parser(parser);
	ensure(count == N * M);

	const double 
		total_time = (double)t/CLOCKS_PER_SEC,
		msg_time = total_time/(N * M),
		msg_per_sec = 1./msg_time;

	printf("Running time for %u messages: %.3f s (%.3f us/message, %u messages/s)\n", N * M, total_time, msg_time * 1000000., (unsigned)msg_per_sec);
}


// batch
void all_group_tests()
{
	simple_group_test();
	simple_group_test2();
	speed_test();
}