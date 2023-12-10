#include <stdio.h>

int main(){
	int A[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	int B[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	int i, j;
	int k = 37;
	j = 0;
	for(i = 0; i < 10; i++) {
  		B[i] = k * 2 + A[j] * 23 + i;
  		if (i % 7 == 0){
  			if (i % 2 == 1)
  				j = i;
  			else
  				k = i + 1;
  		} 
		printf("%d\n", B[i]);
	}
	return 0;
}
