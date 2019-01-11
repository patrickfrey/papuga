/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_TEST_EXECUTE_REQUEST_HPP_INCLUDED
#define _PAPUGA_TEST_EXECUTE_REQUEST_HPP_INCLUDED
/// \brief Function to execute a request
/// \file execRequest.hpp
#include "papuga.h"
#include <string>

struct RequestVariable
{
	const char* name;
	const char* value;
};

bool papuga_execute_request(
		const papuga_RequestAutomaton* atm,
		papuga_ContentType doctype,
		papuga_StringEncoding encoding,
		const std::string& doc,
		const RequestVariable* variables,
		papuga_ErrorCode& errcode,
		std::string& resultblob,
		std::string& logout);

#endif


