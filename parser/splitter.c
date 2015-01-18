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

#include <memory.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

static
boolean is_alnum(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ? YES : NO;
}

static
void set_splitter_state(struct splitter_data* splitter, splitter_state new_state, const char* patt)
{
	splitter->state = new_state;
	splitter->pattern = patt;
	splitter->counter = 0;
}

void init_splitter(struct splitter_data* sp)
{
	ZERO_FILL(sp);
	sp->pattern = "8=FIX";
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
void report_unexpected_symbol(struct fix_parser* parser, char c)
{
	static const char* const suff[] = 
	{
		"header",	// SP_HEADER
		"header",	// SP_FIX_4 
		"length",	// SP_BODY_LENGTH
		"type",		// SP_MSG_TYPE
		"body",		// SP_MSG
		"check sum"	// SP_CHECK_SUM
	};

	if(isgraph(CHAR_TO_INT(c)))
		report_splitter_error(parser, "Unexpected byte '%c' in FIX message %s", c, suff[parser->splitter.state]);
	else
		report_splitter_error(parser, "Unexpected byte 0x%X in FIX message %s", CHAR_TO_INT(c), suff[parser->splitter.state]);
}

static
void append_message_body(struct fix_parser* parser)
{
	struct splitter_data* const sp = &parser->splitter;

	const size_t
		num_bytes = (size_t)(parser->end - parser->ptr),
		msg_len = sp->byte_counter - 1,
		n = msg_len < num_bytes ? msg_len : num_bytes;

	if(n > 0)
	{
		sp->check_sum += append_bytes_to_string_buffer_with_checksum(&parser->buffer, parser->ptr, n);
		parser->ptr += n;
		sp->byte_counter -= n;
	}
}

void read_message(struct fix_parser* parser)
{
	struct splitter_data* const sp = &parser->splitter;

	while(parser->ptr != parser->end)
	{
		const char c = *parser->ptr++;

		if(*sp->pattern == 0)	// matching state data
		{
			switch(sp->state)
			{
			case SP_HEADER:
				sp->check_sum += c;

				switch(c)
				{
				case 'T':
					parser->message.properties.version = FIX_5_0;
					set_splitter_state(sp, SP_BODY_LENGTH, ".1.1\x01" "9=");
					break;
				case '.':
					set_splitter_state(sp, SP_FIX_4, "4.");
					break;
				default:
					report_unexpected_symbol(parser, c);
					return;
				}

				break;

			case SP_FIX_4:
				sp->check_sum += c;

				switch(c)
				{
				case '2':
					parser->message.properties.version = FIX_4_2;
					break;
				case '3':
					parser->message.properties.version = FIX_4_3;
					break;
				case '4':
					parser->message.properties.version = FIX_4_4;
					break;
				default:
					report_unexpected_symbol(parser, c);
					return;
				}

				set_splitter_state(sp, SP_BODY_LENGTH, "\x01" "9=");
				break;

			case SP_BODY_LENGTH:
				sp->check_sum += c;

				switch(c)
				{
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

					if(sp->byte_counter == 0)	// leading zeroes are not allowed
					{
						report_unexpected_symbol(parser, c);
						return;
					}

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

					set_splitter_state(sp, SP_MSG_TYPE, "35=");
					break;

				default:
					report_unexpected_symbol(parser, c);
					return;
				}

				break;

			case SP_MSG_TYPE:
				sp->check_sum += c;

				if(--sp->byte_counter == 0)
				{
					report_splitter_error(parser, "Unexpected end of FIX message");
					return;
				}

				if(is_alnum(c))
				{
					if(sp->counter > 2)
					{
						report_splitter_error(parser, "Invalid FIX message type");
						return;
					}

					parser->message.properties.type[sp->counter++] = c;
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
					append_message_body(parser);
					set_splitter_state(sp, SP_MSG, "");
				}
				else
				{
					report_unexpected_symbol(parser, c);
					return;
				}

				break;

			case SP_MSG:
				sp->check_sum += c;

				if(--sp->byte_counter > 0)
				{
					append_char_to_string_buffer(&parser->buffer, c);
					append_message_body(parser);
				}
				else if(c == SOH)
				{
					append_char_to_string_buffer(&parser->buffer, SOH);
					set_splitter_state(sp, SP_CHECK_SUM, "10=");
				}
				else
				{
					report_splitter_error(parser, "Invalid FIX message type");
					return;
				}

				break;

			case SP_CHECK_SUM:
				switch(c)
				{
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
					if(sp->their_sum != sp->check_sum)
					{
						report_splitter_error(parser, "Invalid FIX message checksum");
						return;
					}

					// all done
					parse_message(parser);
					init_splitter(sp);
					return;
				default:
					report_unexpected_symbol(parser, c);
					return;
				}
				break;

			default:
				report_splitter_error(parser, "Invalid FIX splitter state %d", sp->state);
				return;
			}
		}
		else if(*sp->pattern == c)	// matching state prefix
		{
			switch(sp->state)
			{
			case SP_HEADER:
			case SP_FIX_4:
			case SP_BODY_LENGTH:
				sp->check_sum += c;
				break;

			case SP_MSG_TYPE:
				if(--sp->byte_counter == 0)
				{
					report_splitter_error(parser, "Unexpected end of FIX message");
					return;
				}

				sp->check_sum += c;
				break;

			case SP_CHECK_SUM:
				break;

			case SP_MSG:
			default:
				report_splitter_error(parser, "Invalid FIX splitter state %d", sp->state);
				return;
			}

			++sp->pattern;
		}
		else // mismatch
		{
			report_unexpected_symbol(parser, c);
			return;
		}
	}
}

