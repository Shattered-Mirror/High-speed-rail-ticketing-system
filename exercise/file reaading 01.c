#include <stdio.h>
#include <stdlib.h>
int main() 
{
    FILE *fp;
    //判断文件是否成功打开
    if(fopen_s(&fp, "01.txt", "r"))
        exit(1); //结束程序

    char ch;
    while((ch = fgetc(fp)) != EOF) //逐字符读取文件内容
        putchar(ch); //将读取的字符输出
    fclose(fp); //关闭文件
    return 0;

}