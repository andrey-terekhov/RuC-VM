/*
 *	Copyright 2021 Andrey Terekhov
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

/** Errors codes */
typedef enum ERROR
{
    index_out_of_range,
    wrong_kop,
    wrong_arr_init,
    wrong_number_of_elems,
    zero_division,
    float_zero_division,
    mem_overflow,
    sqrt_from_negat,
    log_from_negat,
    log10_from_negat,
    wrong_asin,
    wrong_string_init,
    printf_runtime_crash,
    init_err,
} error_t;

#ifdef __cplusplus
} /* extern "C" */
#endif
