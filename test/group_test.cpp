/*
Copyright (c) 2013, 2014, 2015, Maxim Konakov
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

#include <string>

// message
// test
static
void simple_group_test()
{
	fix_parser* parser = create_fix_parser(message_with_groups_classifier);
	const fix_message* pm = get_first_fix_message(parser, message_with_groups, message_with_groups_size);

	ensure(pm);
	ensure(pm->error == nullptr);
	validate_message_with_groups(pm);
	ensure(!get_next_fix_message(parser));
	free_fix_parser(parser);
}

static
void simple_group_test2()
{
	const char m[] = "8=FIX.4.2\x01" "9=196\x01" "35=X\x01" "49=A\x01" "56=B\x01" "52=20100318-03:21:11.364\x01" "262=A\x01" "268=2\x01" "279=0\x01" "269=0\x01" "278=BID\x01" "55=EUR/USD\x01" "270=1.37215\x01" "15=EUR\x01" "271=2500000\x01" "346=1\x01" "279=0\x01" "269=1\x01" "278=OFFER\x01" "55=EUR/USD\x01" "270=1.37224\x01" "15=EUR\x01" "271=2503200\x01" "346=1\x01" "34=12\x01" "10=171\x01";
	fix_parser* parser = create_fix_parser(message_with_groups_classifier);
	const fix_message* pm = get_first_fix_message(parser, m, sizeof(m) - 1);

	ensure(pm);
	ensure(pm->error == nullptr);
	validate_message_with_groups(pm);
	ensure(!get_next_fix_message(parser));
	free_fix_parser(parser);
}

static 
void speed_test()
{
	test_for_speed("Message with groups", message_with_groups_classifier, copy_message_with_groups, validate_message_with_groups);
}

// batch
void all_group_tests()
{
	simple_group_test();
	simple_group_test2();
	speed_test();
}