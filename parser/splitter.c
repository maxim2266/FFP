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

#include "fix_parser_impl.h"

#include <memory.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

static
int is_alnum(int c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

NOINLINE static
void report_splitter_error(struct fix_parser* parser, const char* fmt, ...)
{
	static const char def_msg[] = "Unknown FIX message splitter error (Invalid error message format)";

	int n;
	va_list args;
	char buff[1000];

	va_start(args, fmt);
	n = VSPRINTF_S(buff, sizeof(buff), fmt, args);

	if(n > 0)
		set_parser_error(parser, buff, n + 1);
	else
		set_parser_error(parser, def_msg, sizeof(def_msg));
}

NOINLINE static
void report_unexpected_symbol(struct fix_parser* parser, int c)
{
	if(isgraph(c))
		report_splitter_error(parser, "Unexpected byte '%c' in FIX message", (char)c);
	else
		report_splitter_error(parser, "Unexpected byte 0x%X in FIX message", c);
}

// macro for the splitter
#define _STATE_LABEL(l)	\
	case l:	\
		if(s == end) { sp->state = l; parser->ptr = end; return; } else ((void)0)

#define STATE_LABEL	_STATE_LABEL(__COUNTER__)

#define NEXT_CHAR()	\
	STATE_LABEL;	\
	c = CHAR_TO_INT(*s++)

#define NEXT_CHAR_CS()	\
	NEXT_CHAR();	\
	sp->check_sum += (char)c

#define MATCH(x)	\
	NEXT_CHAR();	\
	if(c != (x)) { report_unexpected_symbol(parser, c); return; } else ((void)0)

#define MATCH_CS(x)	\
	NEXT_CHAR_CS();	\
	if(c != (x)) { report_unexpected_symbol(parser, c); return; } else ((void)0)

#define UPDATE_BYTE_COUNT()	\
	if(--sp->byte_counter == 0) {	\
		report_splitter_error(parser, "Unexpected end of FIX message");	\
		return;	\
	} else ((void)0)

#define MATCH_CS_COUNTED(x)	\
	MATCH_CS(x);	\
	UPDATE_BYTE_COUNT()

#define BEGIN_MATCH	\
	NEXT_CHAR();	\
	switch(c) {

#define BEGIN_MATCH_CS	\
	NEXT_CHAR_CS();	\
	switch(c) {

#define END_MATCH	\
	default:	\
		report_unexpected_symbol(parser, (char)c);	\
		return;	\
	}

void read_message(struct fix_parser* parser)
{
	int c;
	size_t n;
	struct splitter_data* const sp = &parser->splitter;
	const char* s = parser->ptr;
	const char* const end = parser->end;

	switch(sp->state)
	{
		MATCH_CS('8');
		MATCH_CS('=');
		MATCH_CS('F');
		MATCH_CS('I');
		MATCH_CS('X');

		BEGIN_MATCH_CS
			case 'T':
				parser->message.properties.version = FIX_5_0;
				goto BODY_LENGTH_FIX_5;
			case '.':
				break;
		END_MATCH

		MATCH_CS('4');
		MATCH_CS('.');

		BEGIN_MATCH_CS
			case '2':
				parser->message.properties.version = FIX_4_2;
				goto BODY_LENGTH;
			case '3':
				parser->message.properties.version = FIX_4_3;
				goto BODY_LENGTH;
			case '4':
				parser->message.properties.version = FIX_4_4;
				goto BODY_LENGTH;
		END_MATCH

	BODY_LENGTH_FIX_5:
		MATCH_CS('.');
		MATCH_CS('1');
		MATCH_CS('.');
		MATCH_CS('1');

	BODY_LENGTH:
		MATCH_CS(SOH);
		MATCH_CS('9');
		MATCH_CS('=');

		BEGIN_MATCH_CS
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				sp->byte_counter = (unsigned char)(c - '0');
				break;
		END_MATCH

		for(;;)
		{
			BEGIN_MATCH_CS
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					sp->byte_counter = sp->byte_counter * 10 + (unsigned char)(c - '0');

					if(sp->byte_counter > MAX_MESSAGE_LEN)
					{
						report_splitter_error(parser, "FIX message longer than " STR(MAX_MESSAGE_LEN) " bytes");
						return;
					}

					break;

				case SOH:
					if(sp->byte_counter < 5)
					{
						report_splitter_error(parser, "Invalid FIX message length: %Iu", sp->byte_counter);
						return;
					}

					goto MSG_TYPE;
			END_MATCH
		}

	MSG_TYPE:
		MATCH_CS_COUNTED('3');
		MATCH_CS_COUNTED('5');
		MATCH_CS_COUNTED('=');

		sp->counter = 0;

		for(;;)
		{
			NEXT_CHAR_CS();
			UPDATE_BYTE_COUNT();

			if(is_alnum(c))
			{
				if(sp->counter > 2)
				{
					report_splitter_error(parser, "Invalid FIX message type");
					return;
				}

				parser->message.properties.type[sp->counter++] = (char)c;
			}
			else if(c == SOH)
			{
				if(sp->counter == 0)
				{
					report_splitter_error(parser, "Invalid FIX message type");
					return;
				}

				parser->message.properties.type[sp->counter] = 0;
				string_buffer_ensure_capacity(&parser->buffer, sp->byte_counter);
				break;
			}
			else
			{
				report_unexpected_symbol(parser, c);
				return;
			}
		}

		// copy message body
		while(sp->byte_counter > 0) 
		{
			STATE_LABEL;
			n = (size_t)(end - s);

			if(sp->byte_counter < n)
				n = sp->byte_counter;

			sp->check_sum += append_bytes_to_string_buffer_with_checksum(&parser->buffer, s, n);
			s += n;
			sp->byte_counter -= n;
		}
		
		if(parser->buffer.size > 0 && parser->buffer.str[parser->buffer.size - 1] != SOH)
			report_splitter_error(parser, "FIX message body is not terminated with SOH");

		// checksum
		MATCH('1');
		MATCH('0');
		MATCH('=');

		for(;;)
		{
			BEGIN_MATCH
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					if(++sp->byte_counter == 4)
					{
						report_unexpected_symbol(parser, c);
						return;
					}

					sp->their_sum = sp->their_sum * 10 + (char)(c - '0');
					break;
				case SOH:
					if(sp->byte_counter != 3 || sp->their_sum != sp->check_sum)
					{
						report_splitter_error(parser, "Invalid FIX message checksum");
						return;
					}

					// all done
					parser->ptr = s;
					parse_message(parser);
					INIT_SPLITTER(sp);
					return;
			END_MATCH
		}

		break;
		default:
			report_splitter_error(parser, "Invalid FIX splitter state %d", sp->state);
	}
}
