#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<string.h>
#include<errno.h>

unsigned int snooze(unsigned int secs){
    int realSecs=sleep(secs);
    printf("\nsleep %d of %d secs.\n",secs-realSecs,secs);
    return realSecs;
}

void dealIntSig(int sig){
    return;// do nothing
}

int main(int argc,char* argv[]){
    if(argc!=2){
        fprintf(stderr,"%s: the input args are not valid, 2 params should be called\n",argv[0]);
        exit(0);
    }
    
    if(signal(SIGINT,dealIntSig)==SIG_ERR){
        fprintf(stderr,"%s : %s","signal error",strerror(errno));
        exit(0);
    }
    int val=atoi(argv[1]);
    int rs=snooze(val);
    exit(0);
}