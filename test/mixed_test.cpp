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
#include <string.h>

static
std::string create_test_data(size_t n)
{
	std::string s(copy_simple_message());

	for(size_t i = 1; i < n; ++i)
		s.append(((i & 1u) != 0) ? copy_message_with_groups() : copy_simple_message());

	return s;
}

static
const fix_tag_classifier* mixed_message_classifier(fix_message_version version, const char* msg_type)
{
	const fix_tag_classifier* const pftc = simple_message_classifier(version, msg_type);

	return pftc ? pftc : message_with_groups_classifier(version, msg_type);
}

static
void validate_mixed_message(const fix_message* pm)
{
	ensure(pm->type[1] == 0);

#ifdef WITH_VALIDATION
	switch(pm->type[0])
	{ 
	case 'D':
		validate_simple_message(pm);
		break;
	case 'X':
		validate_message_with_groups(pm);
		break;
	default:
		ensure(false, "Unknown message type");
		break;
	}
#else
	size_t size;

	switch(pm->type[0])
	{ 
	case 'D':
		size = 12;
		break;
	case 'X':
		size = 6;
		break;
	default:
		ensure(false, "Unknown message type");
		return;
	}

	ensure(get_fix_node_size(get_fix_message_root_node(pm)) == size);
#endif
}

static
void mixed_speed_test()
{
	const int M = 10;
	const std::string s(create_test_data(M));

	const int step = 101, 
#ifdef _DEBUG
		N = 1000;
#else
		N = 100000;
#endif

	const char* const end = s.c_str() + s.size();
	fix_parser* const parser = create_fix_parser(mixed_message_classifier);
	int count = 0;
	clock_t t_start = clock();

	for(int i = 0; i < N; ++i)
	{
		for(const char* p = s.c_str(); p < end; p += step)
		{
			for(const fix_message* pm = get_first_fix_message(parser, p, std::min(step, end - p)); pm; pm = get_next_fix_message(parser))
			{
				ensure(!pm->error);
				validate_mixed_message(pm);
				++count;
			}
	
			ensure(!get_fix_parser_error(parser));
		}
	}

	const clock_t t_end = clock();

	free_fix_parser(parser);
	ensure(count == N * M);

	print_running_time("Mixed messages", N * M, t_start, t_end);
}

// batch
void all_mixed_tests()
{
	mixed_speed_test();
}
