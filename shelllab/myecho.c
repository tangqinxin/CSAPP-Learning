#include<stdio.h>

int main(int argc,char* argv[],char* envp[]){
    printf("Command-line arguments:\n");
    for(int i=0;argv[i]!=NULL;i++){
        printf("argv[%02d]: %s\n",i,argv[i]); 
    }

    printf("Environment variables:\n");
    for(int i=0;envp[i]!=NULL;i++){
        printf("envp[%02d]: %s\n",i,envp[i]);
    }
    return 0;
}


