#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
void Childp(int* pipefd){
    close(pipefd[1]); //read
    char buf[10];
    int num=0;
    if(read(pipefd[0], &buf, sizeof(int))==0){
        close(pipefd[0]);
        exit(0);
    }
    num=*((int*)buf);
    printf("prime %d\n", num);
    //have read
    int pipefd1[2];
    if(pipe(pipefd1)!=0){
        fprintf(2,"pipe1 error\n");
        close(pipefd[0]);
        exit(1);
    }
    int cpid1=fork();
    if (cpid1 == -1) {
        fprintf(2,"fork error\n");
        close(pipefd[0]);
        close(pipefd1[0]);
        close(pipefd1[1]);
        exit(1);
    }
    
    if(cpid1==0){
        Childp(pipefd1);
    }
    //
    while (read(pipefd[0], &buf, sizeof(int)) > 0){
        int cur=*((int*)buf);
        if(cur%num!=0){
        write(pipefd1[1], &cur, sizeof(int));}
    }
    close(pipefd[0]);
    close(pipefd1[1]);
    wait(0);
    exit(0);
}
int main(int argc,char *argv[]){
    int pipefd[2];
    if(pipe(pipefd)!=0){
        fprintf(2,"pipe error\n");
        exit(1);
    } 

    int cpid=fork();
    if (cpid == -1) {
        fprintf(2,"fork error\n");
        close(pipefd[0]);
        close(pipefd[1]);
        exit(1);
    }

  if(cpid==0){ 
    Childp(pipefd);
  }
//father
    close(pipefd[0]); //w
    for(int i=2;i<=35;i++)
        write(pipefd[1],&i,sizeof(int));
    close(pipefd[1]);

    wait(0);                /* Wait for child */
    exit(0);
}