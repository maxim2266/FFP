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

#include "../fix_parser.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>

// Examples.
// The first part explains the message specification language, the second part gives some coding examples.

// Disclaimer.
// This file is for illustrative purposes only. The code below compiles, but was never tested for correctness.

// Part 1. Message specification examples.

// 1. Logout message spec (a simplified version from FIX.4.4)
// This is a simple message without groups
// Begin with "MESSAGE()" macro and supply a unique name for the message ("Logout" in this case)
MESSAGE(Logout)
	// This section contains _all_ possible tags valid for this type of message
	VALID_TAGS(Logout)	// The parameter must match the name of the message
		// header (tags 8, 9 and 35 should not be included)
		TAG(49)		// "SenderCompID"
		TAG(56)		// "TargetCompID"
		TAG(34)		// "MsgSeqNum"
		TAG(347)	// "MessageEncoding"
		// message body
		TAG(58)		// "Text"
		TAG(354)	// "EncodedTextLen"
		TAG(355)	// "EncodedText"
	END_VALID_TAGS	

	// We have one raw data tag here
	DATA_TAGS(Logout)
		DATA_TAG(354, 355)	// ("EncodedTextLen", "EncodedText"); only "EncodedText" tag will be present in the parsed message
	END_DATA_TAGS
	
	// No groups in this message
	NO_GROUPS(Logout)
END_MESSAGE(Logout);	// End of "Logout" message spec.

// 2. Logon message spec
// Every spec for a message with repeated groups must be defined in reverse order, starting
// from the innermost group and ending with the message spec itself. 
// In this example there is one group with the node here called MsgTypes.
GROUP_NODE(MsgTypes, 372)	// (GroupName, MandatoryFirstTag); in the parent group/message spec this group is referred to by its name, so make no mistake. :)
	VALID_TAGS(MsgTypes)
		TAG(372)	// "RefMsgType" <- mandatory first tag
		TAG(385)	// "MsgDirection"
	END_VALID_TAGS

	// No complex things here
	NO_DATA_TAGS(MsgTypes)
	NO_GROUPS(MsgTypes)
END_NODE(MsgTypes);	// End of the group spec

// Here is the message itself
MESSAGE(Logon)
	VALID_TAGS(Logon)
		TAG(49)		// SenderCompID
		TAG(56)		// TargetCompID
		TAG(34)		// MsgSeqNum
		TAG(108)	// HeartBtInt  
		TAG(141)	// ResetSeqNumFlag   
		TAG(789)	// NextExpectedMsgSeqNum   
		TAG(383)	// MaxMessageSize
		TAG(384)	// NoMsgTypes (group "MsgTypes") <- this tag starts a group, see "GROUPS" section below.
		TAG(464)	// TestMessageIndicator   
		TAG(553)	// Username
		TAG(554)	// Password
	END_VALID_TAGS

	NO_DATA_TAGS(Logon)

	// Here are links to all groups in a message; for this message only one group is specified.
	GROUPS(Logon)
		GROUP_TAG(384, MsgTypes)	// Tag 384 ("NoMsgTypes") starts a group called "MsgTypes" (see above spec for the group).
	END_GROUPS
END_MESSAGE(Logon);

// 3. New Order Single
// A complex message with nested groups, as specified in FIX 4.4. 
// The whole spec should better be read from the bottom (i.e. from the message itself) to the top.
// Hopefully the specification language is clear by now, so no more comments. :)
// One last note: the spec is not tested, so please don't use it in production.
GROUP_NODE(Hop, 628)
	VALID_TAGS(Hop)
		TAG(628)	// HopCompID
		TAG(629)	// HopSendingTime
		TAG(630)	// HopRefID
	END_VALID_TAGS

	NO_DATA_TAGS(Hop)
	NO_GROUPS(Hop)
END_NODE(Hop);

GROUP_NODE(Party, 448)
	VALID_TAGS(Party)
		TAG(448)	// PartyID
		TAG(447)	// PartyIDSource
		TAG(452)	// PartyRole
	END_VALID_TAGS

	NO_DATA_TAGS(Party)
	NO_GROUPS(Party)
END_NODE(Party);

GROUP_NODE(NestedParty, 524)
	VALID_TAGS(NestedParty)
		TAG(524)	// NestedPartyID
		TAG(525)	// NestedPartyIDSource
		TAG(538)	// NestedPartyRole 
	END_VALID_TAGS

	NO_DATA_TAGS(NestedParty)
	NO_GROUPS(NestedParty)
END_NODE(NestedParty);

GROUP_NODE(PreAllocGrp, 79)
	VALID_TAGS(PreAllocGrp)
		TAG(79)		// AllocAccount
		TAG(661)	// AllocAcctIDSource
		TAG(736)	// AllocSettlCurrency
		TAG(467)	// IndividualAllocID
		TAG(539)	// NoNestedPartyIDs (group NestedParties)
		TAG(80)		// AllocQty 
	END_VALID_TAGS

	NO_DATA_TAGS(PreAllocGrp)

	GROUPS(PreAllocGrp)
		GROUP_TAG(539, NestedParty)	// NestedParties
	END_GROUPS
END_NODE(PreAllocGrp);

GROUP_NODE(TrdgSesGrp, 336)
	VALID_TAGS(TrdgSesGrp)
		TAG(336)	// TradingSessionID
		TAG(625)	// TradingSessionSubID 
	END_VALID_TAGS

	NO_DATA_TAGS(TrdgSesGrp)
	NO_GROUPS(TrdgSesGrp)
END_NODE(TrdgSesGrp);

GROUP_NODE(SecAltIDGrp, 455)
	VALID_TAGS(SecAltIDGrp)
		TAG(455)	// SecurityAltID
		TAG(456)	// SecurityAltIDSource 
	END_VALID_TAGS

	NO_DATA_TAGS(SecAltIDGrp)
	NO_GROUPS(SecAltIDGrp)
END_NODE(SecAltIDGrp);

GROUP_NODE(EvntGrp, 865)
	VALID_TAGS(EvntGrp)
		TAG(865)	// EventType
		TAG(866)	// EventDate
		TAG(867)	// EventPx
		TAG(868)	// EventText
	END_VALID_TAGS

	NO_DATA_TAGS(EvntGrp)
	NO_GROUPS(EvntGrp)
END_NODE(EvntGrp);

GROUP_NODE(UndSecAltIDGrp, 458)
	VALID_TAGS(UndSecAltIDGrp)
		TAG(458)	// UnderlyingSecurityAltID
		TAG(459)	// UnderlyingSecurityAltIDSource
	END_VALID_TAGS

	NO_DATA_TAGS(UndSecAltIDGrp)
	NO_GROUPS(UndSecAltIDGrp)
END_NODE(UndSecAltIDGrp);

GROUP_NODE(UnderlyingStipulations, 888)
	VALID_TAGS(UnderlyingStipulations)
		TAG(888)	// UnderlyingStipType
		TAG(889)	// UnderlyingStipValue
	END_VALID_TAGS

	NO_DATA_TAGS(UnderlyingStipulations)
	NO_GROUPS(UnderlyingStipulations)
END_NODE(UnderlyingStipulations);

GROUP_NODE(UnderlyingInstrument, 311)
	VALID_TAGS(UnderlyingInstrument)
		TAG(311)	// UnderlyingSymbol
		TAG(312)	// UnderlyingSymbolSfx
		TAG(309)	// UnderlyingSecurityID
		TAG(305)	// UnderlyingSecurityIDSource
		TAG(457)	// NoUnderlyingSecurityAltID (group "UndSecAltIDGrp")
		TAG(462)	// UnderlyingProduct
		TAG(463)	// UnderlyingCFICode
		TAG(310)	// UnderlyingSecurityType
		TAG(763)	// UnderlyingSecuritySubType
		TAG(313)	// UnderlyingMaturityMonthYear
		TAG(542)	// UnderlyingMaturityDate
		TAG(315)	// UnderlyingPutOrCall      
		TAG(241)	// UnderlyingCouponPaymentDate
		TAG(242)	// UnderlyingIssueDate
		TAG(246)	// UnderlyingFactor
		TAG(256)	// UnderlyingCreditRating
		TAG(595)	// UnderlyingInstrRegistry
		TAG(592)	// UnderlyingCountryOfIssue
		TAG(593)	// UnderlyingStateOrProvinceOfIssue
		TAG(594)	// UnderlyingLocaleOfIssue
		TAG(316)	// UnderlyingStrikePrice
		TAG(941)	// UnderlyingStrikeCurrency
		TAG(317)	// UnderlyingOptAttribute
		TAG(436)	// UnderlyingContractMultiplier
		TAG(435)	// UnderlyingCouponRate
		TAG(308)	// UnderlyingSecurityExchange
		TAG(306)	// UnderlyingIssuer
		TAG(362)	// EncodedUnderlyingIssuerLen
		TAG(363)	// EncodedUnderlyingIssuer
		TAG(307)	// UnderlyingSecurityDesc
		TAG(364)	// EncodedUnderlyingSecurityDescLen
		TAG(365)	// EncodedUnderlyingSecurityDesc
		TAG(877)	// UnderlyingCPProgram
		TAG(878)	// UnderlyingCPRegType
		TAG(318)	// UnderlyingCurrency
		TAG(879)	// UnderlyingQty
		TAG(810)	// UnderlyingPx
		TAG(882)	// UnderlyingDirtyPrice
		TAG(883)	// UnderlyingEndPrice
		TAG(884)	// UnderlyingStartValue
		TAG(885)	// UnderlyingCurrentValue
		TAG(886)	// UnderlyingEndValue
		TAG(887)	// NoUnderlyingStips (group "UnderlyingStipulations")
	END_VALID_TAGS

	DATA_TAGS(UnderlyingInstrument)
		DATA_TAG(362, 363)	// EncodedUnderlyingIssuer
		DATA_TAG(364, 365)	// EncodedUnderlyingSecurityDesc
	END_DATA_TAGS

	GROUPS(UnderlyingInstrument)
		GROUP_TAG(457, UndSecAltIDGrp)
		GROUP_TAG(887, UnderlyingStipulations)
	END_GROUPS
END_NODE(UnderlyingInstrument);

GROUP_NODE(Stipulations, 233)
	VALID_TAGS(Stipulations)
		TAG(233)	// StipulationType
		TAG(234)	// StipulationValue
	END_VALID_TAGS

	NO_DATA_TAGS(Stipulations)
	NO_GROUPS(Stipulations)
END_NODE(Stipulations);

MESSAGE(NewOrderSingle)
	VALID_TAGS(NewOrderSingle)
		// header
		TAG(49)		// SenderCompID
		TAG(56)		// TargetCompID
		TAG(115)	// OnBehalfOfCompID
		TAG(128)	// DeliverToCompID
		TAG(34)		// MsgSeqNum
		TAG(50)		// SenderSubID
		TAG(142)	// SenderLocationID
		TAG(57)		// TargetSubID
		TAG(143)	// TargetLocationID
		TAG(116)	// OnBehalfOfSubID
		TAG(144)	// OnBehalfOfLocationID
		TAG(129)	// DeliverToSubID
		TAG(145)	// DeliverToLocationID
		TAG(43)		// PossDupFlag
		TAG(97)		// PossResend
		TAG(52)		// SendingTime
		TAG(122)	// OrigSendingTime
		TAG(369)	// LastMsgSeqNumProcessed
		TAG(627)	// NoHops (group "Hop")
		// message body
		TAG(11)		// ClOrdID
		TAG(526)	// SecondaryClOrdID
		TAG(583)	// ClOrdLinkID 
		TAG(453)	// NoPartyIDs (group "Parties")
		TAG(229)	// TradeOriginationDate
		TAG(75)		// TradeDate
		TAG(1)		// Account
		TAG(660)	// AcctIDSource
		TAG(581)	// AccountType
		TAG(589)	// DayBookingInst
		TAG(590)	// BookingUnit
		TAG(591)	// PreallocMethod
		TAG(70)		// AllocID 
		TAG(76)		// NoAllocs (group "PreAllocGrp")
		TAG(63)		// SettlType
		TAG(64)		// SettlDate
		TAG(544)	// CashMargin
		TAG(635)	// ClearingFeeIndicator
		TAG(21)		// HandlInst
		TAG(18)		// ExecInst
		TAG(110)	// MinQty
		TAG(111)	// MaxFloor
		TAG(100)	// ExDestination 
		TAG(386)	// NoTradingSessions (group "TrdgSesGrp")
		TAG(81)		// ProcessCode 		
		// Component "Instrument"
		TAG(55)		// Symbol
		TAG(65)		// SymbolSfx
		TAG(48)		// SecurityID
		TAG(22)		// SecurityIDSource 
		TAG(454)	// NoSecurityAltID (group "SecAltIDGrp")
		TAG(460)	// Product
		TAG(461)	// CFICode
		TAG(167)	// SecurityType
		TAG(762)	// SecuritySubType
		TAG(200)	// MaturityMonthYear
		TAG(541)	// MaturityDate
		TAG(201)	// PutOrCall   
		TAG(224)	// CouponPaymentDate
		TAG(225)	// IssueDate
		TAG(239)	// RepoCollateralSecurityType
		TAG(226)	// RepurchaseTerm
		TAG(227)	// RepurchaseRate
		TAG(228)	// Factor
		TAG(255)	// CreditRating
		TAG(543)	// InstrRegistry
		TAG(470)	// CountryOfIssue
		TAG(471)	// StateOrProvinceOfIssue
		TAG(472)	// LocaleOfIssue
		TAG(240)	// RedemptionDate
		TAG(202)	// StrikePrice
		TAG(947)	// StrikeCurrency
		TAG(206)	// OptAttribute
		TAG(231)	// ContractMultiplier
		TAG(223)	// CouponRate
		TAG(207)	// SecurityExchange
		TAG(106)	// Issuer
		TAG(348)	// EncodedIssuerLen
		TAG(349)	// EncodedIssuer
		TAG(107)	// SecurityDesc
		TAG(350)	// EncodedSecurityDescLen
		TAG(351)	// EncodedSecurityDesc
		TAG(691)	// Pool
		TAG(667)	// ContractSettlMonth
		TAG(875)	// CPProgram
		TAG(876)	// CPRegType
		TAG(864)	// NoEvents (group "EvntGrp")
		TAG(873)	// DatedDate
		TAG(874)	// InterestAccrualDate
		// End Component "Instrument"
		// Component "FinancingDetails"
		TAG(913)	// AgreementDesc
		TAG(914)	// AgreementID
		TAG(915)	// AgreementDate
		TAG(918)	// AgreementCurrency
		TAG(788)	// TerminationType
		TAG(916)	// StartDate
		TAG(917)	// EndDate
		TAG(919)	// DeliveryType
		TAG(898)	// MarginRatio
		// Component "UndInstrmtGrp"
		TAG(711)	// NoUnderlyings (group "UnderlyingInstrument")
		// End Component "FinancingDetails"
		// End Component "FinancingDetails"		
		TAG(140)	// PrevClosePx
		TAG(54)		// Side
		TAG(114)	// LocateReqd
		TAG(60)		// TransactTime
		// Component "Stipulations"
		TAG(232)	// NoStipulations (group "Stipulations")
		// End Component "Stipulations"
		TAG(854)	// QtyType
		// Component "OrderQtyData"
		TAG(38)		// OrderQty
		TAG(152)	// CashOrderQty
		TAG(516)	// OrderPercent
		TAG(468)	// RoundingDirection
		TAG(469)	// RoundingModulus
		// End Component "OrderQtyData"
		TAG(40)		// OrdType
		TAG(423)	// PriceType
		TAG(44)		// Price
		TAG(99)		// StopPx
		// Component "SpreadOrBenchmarkCurveData"
		TAG(218)	// Spread
		TAG(220)	// BenchmarkCurveCurrency
		TAG(221)	// BenchmarkCurveName
		TAG(222)	// BenchmarkCurvePoint
		TAG(662)	// BenchmarkPrice
		TAG(663)	// BenchmarkPriceType
		TAG(699)	// BenchmarkSecurityID
		TAG(761)	// BenchmarkSecurityIDSource		
		// End Component "SpreadOrBenchmarkCurveData"
		// Component "YieldData"
		TAG(235)	// YieldType
		TAG(236)	// Yield
		TAG(701)	// YieldCalcDate
		TAG(696)	// YieldRedemptionDate
		TAG(697)	// YieldRedemptionPrice
		TAG(698)	// YieldRedemptionPriceType
		// End Component "YieldData"
		TAG(15)		// Currency
		TAG(376)	// ComplianceID
		TAG(377)	// SolicitedFlag
		TAG(23)		// IOIID
		TAG(117)	// QuoteID
		TAG(59)		// TimeInForce
		TAG(168)	// EffectiveTime
		TAG(432)	// ExpireDate
		TAG(126)	// ExpireTime
		TAG(427)	// GTBookingInst
		// Component "CommissionData"
		TAG(12)		// Commission
		TAG(13)		// CommType
		TAG(479)	// CommCurrency
		TAG(497)	// FundRenewWaiv
		// End Component "CommissionData"
		TAG(528)	// OrderCapacity
		TAG(529)	// OrderRestrictions
		TAG(582)	// CustOrderCapacity
		TAG(121)	// ForexReq
		TAG(120)	// SettlCurrency
		TAG(775)	// BookingType
		TAG(58)		// Text
		TAG(354)	// EncodedTextLen
		TAG(355)	// EncodedText
		TAG(193)	// SettlDate2
		TAG(192)	// OrderQty2
		TAG(640)	// Price2
		TAG(77)		// PositionEffect
		TAG(203)	// CoveredOrUncovered
		TAG(210)	// MaxShow
		// Component "PegInstructions"
		TAG(211)	// PegOffsetValue
		TAG(835)	// PegMoveType
		TAG(836)	// PegOffsetType
		TAG(837)	// PegLimitType
		TAG(838)	// PegRoundDirection
		TAG(840)	// PegScope
		// End Component "PegInstructions"
		// Component "DiscretionInstructions"
		TAG(388)	// DiscretionInst
		TAG(389)	// DiscretionOffsetValue
		TAG(841)	// DiscretionMoveType
		TAG(842)	// DiscretionOffsetType
		TAG(843)	// DiscretionLimitType
		TAG(844)	// DiscretionRoundDirection
		TAG(846)	// DiscretionScope
		// End Component "DiscretionInstructions"
		TAG(847)	// TargetStrategy
		TAG(848)	// TargetStrategyParameters
		TAG(849)	// ParticipationRate
		TAG(480)	// CancellationRights
		TAG(481)	// MoneyLaunderingStatus
		TAG(513)	// RegistID
		TAG(494)	// Designation
	END_VALID_TAGS

	DATA_TAGS(NewOrderSingle)
		DATA_TAG(354, 355)	// EncodedText
		DATA_TAG(348, 349)	// EncodedIssuer
		DATA_TAG(350, 351)	// EncodedSecurityDesc
	END_DATA_TAGS

	GROUPS(NewOrderSingle)
		GROUP_TAG(627, Hop)
		GROUP_TAG(453, Party)
		GROUP_TAG(76, PreAllocGrp)
		GROUP_TAG(386, TrdgSesGrp)
		GROUP_TAG(454, SecAltIDGrp)
		GROUP_TAG(864, EvntGrp)
		GROUP_TAG(711, UnderlyingInstrument)
		GROUP_TAG(232, Stipulations)
	END_GROUPS
END_MESSAGE(NewOrderSingle);

// Finally, the classifier function required to construct a parser
// The function uses the above three message specs
const struct fix_tag_classifier* example_classifier_func(fix_message_version version, const char* msg_type)
{
	if(version != FIX_4_4)
		return NULL;	// only v4.4 is accepted

	if(msg_type[1] != 0)
		return NULL;	// only one-symbol types in our example

	switch(msg_type[0])
	{
	case 'A':
		return PARSER_TABLE_ADDRESS(Logon);
	case '5':
		return PARSER_TABLE_ADDRESS(Logout);
	case 'D':
		return PARSER_TABLE_ADDRESS(NewOrderSingle);
	default:
		return NULL;
	}
}

// Part 2. Code examples.
// parser constructor
struct fix_parser* create_my_parser()
{
	return create_fix_parser(example_classifier_func);
}

// Example of logout message parsing
// The semantics implemented is absolutely fictional and provided for example only.
// First, a "business" structure is defined to pass the message data to the rest of the system.
struct logout_message
{
	const char *sender, *receiver, *text;
	int64_t seq_num;
};

// here the data from the fix message are extracted to fill in a new "business" structure, 
// i.e. conversion: struct fix_message -> struct logout_message
static
void process_logout(const struct fix_message* msg)
{
	// allocate target structure
	struct logout_message* const msg_data = (struct logout_message*)malloc(sizeof(struct logout_message));

	// get root node
	const struct fix_group_node* const node = get_fix_message_root_node(msg);	// root node always exists

	// sender string
	msg_data->sender = get_fix_tag_as_string(node, 49);	// SenderCompID(49)

	if(!msg_data->sender)	// error check
	{
		printf("Sender string not found\n");	// replace with real error handling
		free(msg_data);

		return;
	}

	// copy the sender string so that the msd_data structure may be processed later, e.g. in another thread
	msg_data->sender = _strdup(msg_data->sender);	

	// same for the receiver
	msg_data->receiver = get_fix_tag_as_string(node, 56);	// TargetCompID(56)

	if(!msg_data->receiver)	// error check
	{
		printf("Receiver string not found\n");	// replace with real error handling

		free((void*)msg_data->sender);
		free(msg_data);

		return;
	}

	msg_data->receiver = _strdup(msg_data->receiver);

	// sequence number
	if(!get_fix_tag_as_integer(node, 34, &msg_data->seq_num) || msg_data->seq_num <= 0L)	// MsgSeqNum(34)
	{
		printf("Something is wrong with the sequence number\n");	// replace with real error handling

		free((void*)msg_data->sender);
		free((void*)msg_data->receiver);
		free(msg_data);

		return;
	}

	// Here, the example logic is that either text or encoded text or none of them is specified.
	msg_data->text = get_fix_tag_as_string(node, 58);	// Text(58)

	if(msg_data->text)
		msg_data->text = _strdup(msg_data->text);
	else
	{
		const struct fix_tag* const tag = get_fix_tag(node, 355); // EncodedText(355)

		if(tag)	// got encoded text
		{
			// msg_data->text = decode(tag->value, tag->length);
		}
		else
			msg_data->text = NULL;
	}

	// all done, do further processing
	// do_logout(msg_data), or something like that
}

// logon message
static
void process_logon(const struct fix_message* msg)
{
	const struct fix_group_node* const node = get_fix_message_root_node(msg);

	// process root node as in the example above
	// ...

	// process group
	const struct fix_group_node* pnode;
	const struct fix_tag* pt = get_fix_tag(node, 384);	// GROUP_TAG(384, MsgTypes), "NoMsgTypes"

	if(!pt)	// assuming the tag must be present, even if the group is empty
	{
		printf("No group tag\n");	// error
		return;
	}

	for(pnode = pt->group; pnode; pnode = get_next_fix_node(pnode))	// loop over all group nodes
	{
		// process node here
	}
}

// new order single, here left as an exercise for the reader :)
static
void process_new_order_single(const struct fix_message* msg)
{
	msg;
	//
}

// message dispatcher
static
void dispatch_message(const struct fix_message* msg)
{
	switch(msg->type[0])
	{
	case 'A':
		process_logon(msg);
		break;
	case '5':
		process_logout(msg);
		break;
	case 'D':
		process_new_order_single(msg);
		break;
	default:
		// must never happen
		break;
	}
}

// message error handler
static
void process_message_error(fix_message_version version, const char* type, const char* error)
{
	version;
	type;
	error;

	// error handling code goes here
	// note: get_fix_message_root_node() does not work on error
}

// example parser entry point
// this function is usually called when a chunk of data arrives e.g. from a socket
int parse(struct fix_parser* parser, const void* bytes, size_t n)
{
	const struct fix_message* msg;

	// loop until all the messages are processed
	for(msg = get_first_fix_message(parser, bytes, n); msg; msg = get_next_fix_message(parser))
	{
		if(!msg->error)
			dispatch_message(msg);
		else
			process_message_error(msg->version, msg->type, msg->error);
	}

	// check if the parser is still in a valid state
	if(get_fix_parser_error(parser))
	{
		free_fix_parser(parser);	// the parser is dead, nothing else can be done
		return 0;
	}

	return 1;
}