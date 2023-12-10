#include <stdio.h>

int main()
{
	int x = 2;
	if (1)
	{
		x = 3;
	}
	else
	{
		int y = x;
	}
	printf("x: %i", x);
	return x;
}
