#include <stdio.h>

int main()
{

	int x = 2;
	int y = 3;
	int z = 1;
	if (x == 2)
	{
		z = x;
		if (y == 2)
		{
			z = y;
		}
	}
	printf("x: %i, y: %i, z: %i", x, y, z);
	return z;
}
