#include <stdio.h>
#include <stdlib.h>

void testMalloc()
{
    int age[10];  // 栈上分配的数组（自动管理内存）
    
    // 使用栈上数组，避免未使用变量的警告
    for(int i = 0; i < 10; i++) {
        age[i] = i * 10;  // 初始化栈数组
    }
    printf("栈数组age的前3个元素: %d, %d, %d\n", age[0], age[1], age[2]);
    
    int *p = malloc(10 * sizeof(int));  // 堆上分配的数组（手动管理内存）
    
    // 检查内存分配是否成功
    if(!p)  // 等价于 if(p == NULL)
    {
        printf("内存申请失败!\n");
        return;
    }
    
    printf("堆内存分配成功！地址: %p\n", (void *)p);
        
    // 初始化数组：0, 2, 4, 6, ..., 18
    printf("堆数组p的元素: ");
    for(int i = 0; i < 10; i++)
    {
        *(p + i) = i * 2;  // 使用指针算术访问元素
        printf("%d ", *(p + i));  // 打印每个元素值
    }
    printf("\n");
    
    
    free(p);  // 释放内存
    p = NULL;  // 将指针设为NULL，避免悬垂指针
    
    printf("内存已释放，p已置为NULL\n");
    
    // 演示栈数组和堆数组的地址差异
    printf("栈数组地址（age）: %p\n", (void *)age);
    printf("堆数组地址（原p）: 已释放，不能访问\n");
}

int main() {
    printf("=== malloc使用示例 ===\n");
    testMalloc();
    printf("=== 程序结束 ===\n");
    return 0;
}