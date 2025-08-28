#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
int main(int argc,char *argv[]){
    
    char buf[20];
    memset(buf,0,sizeof(buf));
    char ch;
    while(read(0,&ch,1)>0)
    {
        if(ch=='\n'){
            int cpid=fork();
            if (cpid == -1) {
                fprintf(2,"fork error\n");
                exit(1);
            }
            if(cpid==0){
                char* myargv[20];
                memset(myargv,0,sizeof(myargv));
                for(int i=0;i<argc-1;i++)
                    myargv[i]=argv[i+1];
                int myargc=argc-1;
                myargv[myargc++]=buf;
                myargv[myargc+1]=0;

                // for(int i=0;i<myargc+1;i++)
                // printf("myargv[%d]: %s\n",i,myargv[i]);
                // printf("%d\n",myargc);
    
                exec(myargv[0], myargv);
                exit(0);
            }
            else {
                wait(0);                /* Wait for child */
                memset(buf,0,sizeof(buf));
            }
        }
        else buf[strlen(buf)]=ch;
    }
 
    exit(0);
}