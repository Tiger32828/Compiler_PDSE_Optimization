#include <stdio.h>

int main()
{
	int x = 2;
	int y = 3;
	int z = x;
	if (x == 2)
	{
		z = y;
	}
	else
	{
		z = 5;
	}
	printf("x: %i, y: %i, z: %i", x, y, z);
	return 0;
}
