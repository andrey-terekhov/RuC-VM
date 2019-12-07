///
/// main.c
///
/// Created by Victor Y. Fadeev on 07.12.2019.
/// Copyright (c) 2019 Andrey Terekhov. All rights reserved.
///

#include "import.h"


int main(int argc, const char *argv[])
{
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
