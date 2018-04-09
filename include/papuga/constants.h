/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _PAPUGA_CONSTANTS_H_INCLUDED
#define _PAPUGA_CONSTANTS_H_INCLUDED
/*
* @brief Some constants for limits etc.
* @file constants.h
*/

/*
 * @brief Maximum number of elements printed for serialized iterator contents in XML,JSON,etc.
 */
#define PAPUGA_MAX_ITERATOR_EXPANSION_LENGTH 100

/*
 * @brief Maximum depth of recursion in recursive expansions (like XML,JSON mapping) to avoid stack overflow
 */
#define PAPUGA_MAX_RECURSION_DEPTH 200

/*
 * @brief Name of element mapped to a link in case of HTML output
 */
#define PAPUGA_HTML_LINK_ELEMENT "link"

#endif


