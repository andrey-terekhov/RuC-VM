/*
 *	Copyright 2019 Andrey Terekhov, Victor Y. Fadeev
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

#include "import.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char *argv[])
{
	fprintf(stdout, "stdout\n");
	fflush(stdout);
	fprintf(stderr, "stderr\n");
	fflush(stderr);
#ifdef TESTING_EXIT_CODE
	exit(TESTING_EXIT_CODE);
#else
	exit(33);
#endif
	if (argc < 2)
	{
		import("export.txt");
	}
	else
	{
		import(argv[1]);
	}

	return 0;
}
