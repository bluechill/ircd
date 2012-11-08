#include <stdio.h>
#include <stdlib.h>

int main()
{
	char* hello  = "Hello %s";
	printf(hello, "world");

	return 0;
}
