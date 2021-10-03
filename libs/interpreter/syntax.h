/*
 *	Copyright 2020 Andrey Terekhov, Victor Y. Fadeev
 *
 *	Licensed under the Apache License, Version 2.0 (the "License");
 *	you may not use this file except in compliance with the License.
 *	You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 *	Unless required by applicable law or agreed to in writing, software
 *	distributed under the License is distributed on an "AS IS" BASIS,
 *	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	See the License for the specific language governing permissions and
 *	limitations under the License.
 */

#pragma once


#ifdef __cplusplus
extern "C" {
#endif

enum TYPE
{
	TYPE_VOID			= -6,
	TYPE_FLOATING		= -3,
    TYPE_CHAR,
	TYPE_INTEGER,

	TYPE_STRUCTURE		= 1002,
	TYPE_ARRAY			= 1004,
};

#ifdef __cplusplus
} /* extern "C" */
#endif