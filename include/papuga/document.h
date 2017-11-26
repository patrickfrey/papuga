/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structures and functions for scanning papuga XML and JSON documents for further processing
/// \file request.h
#ifndef _PAPUGA_REQUEST_H_INCLUDED
#define _PAPUGA_REQUEST_H_INCLUDED
#include "papuga/typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct papuga_RequestAutomaton papuga_RequestAutomaton;
typedef struct papuga_Request papuga_Request;

/*
 * \brief Create an automaton to configure
 * \return The automaton structure
 */
papuga_RequestAutomaton* papuga_create_RequestAutomaton();

/*
 * \brief Destroy an automaton
 * \param[in] self the automaton structure to free
 */
void papuga_destroy_RequestAutomaton( papuga_RequestAutomaton* self);

/*
 * \brief Get the last error when building the automaton
 * \param[in] self automaton to get the last error from
 */
papuga_ErrorCode papuga_RequestAutomaton_last_error( const papuga_RequestAutomaton* self);
