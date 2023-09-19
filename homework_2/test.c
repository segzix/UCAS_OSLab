#include <stdio.h>
#include <stdlib.h>
int a;
int b = 20;
int main()
{
    int* c;
    c = (int *)malloc(sizeof(int));

    *c = a + b;
    printf("%d\n",c[0]);
}