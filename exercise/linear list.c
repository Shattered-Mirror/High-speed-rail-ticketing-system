#include <stdio.h>
#include <stdlib.h>

#define MAX_SIZE 100  // 线性表最大容量

typedef struct {
    int data[MAX_SIZE];  // 存储数据的数组
    int length;          // 当前长度
} SeqList;

// 1. 初始化线性表
void InitList(SeqList *L) {
    L->length = 0;
}

// 2. 插入元素（在位置pos插入元素value）
int InsertList(SeqList *L, int pos, int value) {
    if (L->length >= MAX_SIZE) {
        printf("线性表已满！\n");
        return 0;
    }
    if (pos < 1 || pos > L->length + 1) {
        printf("插入位置不合法！\n");
        return 0;
    }
    
    // 将pos位置及之后的元素后移
    for (int i = L->length; i >= pos; i--) {
        L->data[i] = L->data[i - 1];
    }
    
    L->data[pos - 1] = value;  // 插入新元素
    L->length++;
    return 1;
}

// 3. 删除元素（删除位置pos的元素）
int DeleteList(SeqList *L, int pos) {
    if (pos < 1 || pos > L->length) {
        printf("删除位置不合法！\n");
        return 0;
    }
    
    // 将pos位置之后的元素前移
    for (int i = pos - 1; i < L->length - 1; i++) {
        L->data[i] = L->data[i + 1];
    }
    
    L->length--;
    return 1;
}

// 4. 查找元素（按值查找）
int FindList(SeqList L, int value) {
    for (int i = 0; i < L.length; i++) {
        if (L.data[i] == value) {
            return i + 1;  // 返回位置（从1开始）
        }
    }
    return 0;  // 未找到
}

// 5. 遍历打印线性表
void PrintList(SeqList L) {
    if (L.length == 0) {
        printf("线性表为空！\n");
        return;
    }
    
    printf("线性表内容：");
    for (int i = 0; i < L.length; i++) {
        printf("%d ", L.data[i]);
    }
    printf("\n");
}

// 6. 获取线性表长度
int GetLength(SeqList L) {
    return L.length;
}

int main() {
    SeqList list;
    int choice, pos, value;
    
    InitList(&list);
    
    // 测试数据
    InsertList(&list, 1, 10);
    InsertList(&list, 2, 20);
    InsertList(&list, 3, 30);
    
    while (1) {
        printf("\n===== 线性表操作菜单 =====\n");
        printf("1. 插入元素\n");
        printf("2. 删除元素\n");
        printf("3. 查找元素\n");
        printf("4. 显示线性表\n");
        printf("5. 显示长度\n");
        printf("0. 退出\n");
        printf("请选择操作：");
        scanf("%d", &choice);
        
        switch (choice) {
            case 1:
                printf("请输入插入位置和值（例如：2 25）：");
                scanf("%d %d", &pos, &value);
                if (InsertList(&list, pos, value)) {
                    printf("插入成功！\n");
                }
                break;
                
            case 2:
                printf("请输入删除位置：");
                scanf("%d", &pos);
                if (DeleteList(&list, pos)) {
                    printf("删除成功！\n");
                }
                break;
                
            case 3:
                printf("请输入要查找的值：");
                scanf("%d", &value);
                pos = FindList(list, value);
                if (pos) {
                    printf("元素 %d 在第 %d 个位置\n", value, pos);
                } else {
                    printf("未找到元素 %d\n", value);
                }
                break;
                
            case 4:
                PrintList(list);
                break;
                
            case 5:
                printf("线性表长度：%d\n", GetLength(list));
                break;
                
            case 0:
                printf("程序退出！\n");
                exit(0);
                
            default:
                printf("无效选择！\n");
        }
    }
    
    return 0;
}