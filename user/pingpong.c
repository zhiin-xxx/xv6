#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
int main(int argc,char *argv[]){
    int pipefd1[2],pipefd2[2];
    if(pipe(pipefd1)!=0){
        fprintf(2,"pipe1 error\n");
        exit(1);
    } 
    if(pipe(pipefd2)!=0){
        fprintf(2,"pipe2 error\n");
        exit(1);
    }

    int cpid=fork();
    if (cpid == -1) {
        fprintf(2,"fork error\n");
        exit(1);
    }

  if(cpid==0){ 
    close(pipefd1[0]); //1 write
    close(pipefd2[1]); //2 read
    char buf[10];
    while (read(pipefd2[0], &buf, 1) > 0)
        printf("%d: received ping\n", getpid());
    char c='c';
    write(pipefd1[1], &c, 1);
    close(pipefd1[1]);
    close(pipefd2[0]);
    exit(0);
  }
//father
    close(pipefd2[0]); //2 write
    close(pipefd1[1]); //1 read
    char p='p';
    write(pipefd2[1], &p, 1);
    close(pipefd2[1]);

    char buf[10];
    while (read(pipefd1[0], &buf, 1) > 0)
        printf("%d: received pong\n", getpid());
        
    close(pipefd1[0]);
    wait(0);                /* Wait for child */
    exit(0);
  
}