/*================================================================
* Filename:test1.c
* Author: KCN_yu
* Createtime:Mon 09 Nov 2020 06:43:01 PM CST
================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <error.h>
#include <string.h>
#include <signal.h>

#define MAX 256
int semid, shmid;
char* shmaddr;

void sigHndlr(int s){
    shmdt(shmaddr);
    shmctl(shmid,IPC_RMID,NULL);
    semctl(semid,0,IPC_RMID,(int) 0);
    printf("free!\n");
    exit(0);
}
int main(int argc, char *argv[])
{
    signal(SIGINT,sigHndlr);
    char str[MAX];
    key_t key;
    key = ftok("./test3.c",'s');
    struct sembuf sops;
    semid = semget(key,1,0666|IPC_CREAT);
    shmid = shmget(key,MAX,0666|IPC_CREAT);
    shmaddr = (char*)shmat(shmid,NULL,0);
    semctl(semid,0,SETVAL,(int) 0);
    sops.sem_num = 0;
    sops.sem_flg = 0;
    do{
        sops.sem_op = 3;
        semop(semid,&sops,1);
        printf("waiting!\n");
        sops.sem_op = 0;
        semop(semid,&sops,1);
        strcpy(str,shmaddr);
        //puts(str);
        int num = 0;
        int len = strlen(str);
        for(int i = 0; i < len-1; i++){
            int tmp = str[i] - '0';
            num += tmp;
        }
        if(num%3 == 0) strcpy(shmaddr,"Yes\n");
        else strcpy(shmaddr,"No\n");
    }while(1);
    /*
    shmdt(shmaddr);
    shmctl(shmid,IPC_RMID,NULL);
    semctl(semid,0,IPC_RMID,(int) 0);
    */
    return 0;
}