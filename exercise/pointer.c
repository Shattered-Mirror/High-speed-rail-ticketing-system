/*
#include<stdio.h>

int main()
{
    int num = 10;
    int *p ;
    p = &num;

    printf("num的值是:%d\n",num);
    printf("num的地址是:%p\n",&num);
    printf("p的值是:%p\n",p);

    printf("p的地址是:%p\n",&p);
    printf("*p的值是:%d\n",*p);

    *p = 20;
    printf("num的值是:%d\n",num);
    printf("*p的值是:%d\n",*p);

    return 0;

}
*/
#include<stdio.h>
void Swap(int *p1, int *p2)
{
    int temp = 0;
    temp = *p1;
    *p1 = *p2;
    *p2 = temp;
}

int main()
{
    int a = 10;
    int b = 20;

    Swap(&a,&b);

    printf("%d\n%d\n",a,b);
}