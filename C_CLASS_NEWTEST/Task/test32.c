/*************************************************************************
	> File Name: test32.c
	> Author: 
	> Mail: 
	> Created Time: Wed 20 Nov 2019 11:37:22 PM CST
 ************************************************************************/

#include<stdio.h>
#include<stdbool.h>
int Myatoi(char *s){
    int n = 0;
    bool isMinus = false;
    if(*s == '+' || *s == '-'){
        if(*s == '-'){
            isMinus = true;
        }
        s++;
    }
    while(*s != '\0' && *s >= '0' && *s <= '9'){
        n = n * 10 + *s - '0';
        s++;
    }
    return n * (isMinus ? -1 : 1); 
}
int main(int argc, char *argv[]){
    int n =Myatoi(argv[1]);
    printf("%d\n",n);
    return 0;
}