#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <fcntl.h>  
#include <sys/stat.h>
#include "a2_helper.h"

int count=0;
pthread_mutex_t mutex;
sem_t turnstile;
sem_t turnstile2;
sem_t s;
sem_t startT5_5;
sem_t endT5_3;
pthread_mutex_t mutex5;


void* f(void* arg){//little books of semaphores
    int id=(*(int*)arg);
    sem_wait(&s);
    info(BEGIN,4,id); 
    pthread_mutex_lock(&mutex);
    count += 1;
    if (count == 5){
        sem_wait(&turnstile2);
        sem_post(&turnstile) ;
    }
       
 pthread_mutex_unlock(&mutex);

    sem_wait(&turnstile);
    sem_post(&turnstile) ;

    if(id==14){
        info(END,4,14);
    }
 pthread_mutex_lock(&mutex);
 count -= 1;
 if (count == 0){
    sem_wait(&turnstile);
    sem_post(&turnstile2) ;
 }

pthread_mutex_unlock(&mutex);

    sem_wait(&turnstile2);
    sem_post(&turnstile2) ;

    if(id!=14)
        info(END,4,id);
    sem_post(&s);
    return NULL;



}

void process9(){
    
}


void process8(){
    
}


void process7(){
    
}

void process6(){
    
}


void* general_thread(void* arg) {
    int id = *(int*)arg;

    switch(id) {
        case 3:
            sem_wait(&startT5_5);
            break;
        case 5:
            info(BEGIN, 5, 5);
            sem_post(&startT5_5);
            break;
    }

    if(id!=5){
        info(BEGIN,5,id);
    }

    switch(id) {
        case 5:
            sem_wait(&endT5_3);
            break;
        case 3:
            info(END, 5, 3);
            sem_post(&endT5_3);
            break;
    }

    if (id != 3) {
        info(END, 5, id);
    }

    return NULL;
}


void process5() {
    pthread_t threads[6]; 

    sem_init(&startT5_5, 0, 0);
    sem_init(&endT5_3, 0, 0);
    pthread_mutex_init(&mutex5, NULL);

    for(int i=1;i<=5;i++){
        int *threadid=malloc(sizeof(int));
        *threadid=i;
        pthread_create(&threads[i],NULL,general_thread,threadid);
    }

    for(int i=1;i<=5;i++){
        pthread_join(threads[i],NULL);
    }

   
    sem_destroy(&startT5_5);
    sem_destroy(&endT5_3);
    pthread_mutex_destroy(&mutex5);
}



void process4(){
    int pid7,pid8;
    pid7=fork();
    if(pid7==0){
        info(BEGIN,7,0);
        process7();

        info(END,7,0);
        exit(0);
    }   

    pid8=fork();
    if(pid8==0){
        info(BEGIN,8,0);
        process8();

        info(END,8,0);
        exit(0);
    }

    pthread_t *th=malloc(40*sizeof(pthread_t));
    pthread_mutex_init(&mutex,NULL);
    sem_init(&turnstile,0,0);
    sem_init(&turnstile2,0,1);
    sem_init(&s,0,5);

    for(int i=1;i<=40;i++){
        int *threadid=malloc(sizeof(int));
        *threadid=i;
        pthread_create(&th[i],NULL,f,threadid);
    }

    for(int i=1;i<=40;i++){
        pthread_join(th[i],NULL);
    }


    sem_destroy(&turnstile);
    sem_destroy(&turnstile2);
    pthread_mutex_destroy(&mutex);
    sem_destroy(&s);

    
    wait(NULL);
    wait(NULL);
}

void process3(){
    
}

void process2(){
    int pid3,pid4,pid5;
    pid3=fork();
    if(pid3==0){
        info(BEGIN,3,0);
        process3();

        info(END,3,0);
        exit(0);
    }

    pid4=fork();
    if(pid4==0){
        info(BEGIN,4,0);
        process4();

        info(END,4,0);
        exit(0);
    }

    pid5=fork();
    if(pid5==0){
        info(BEGIN,5,0);
        process5();

        info(END,5,0);
        exit(0);
    }

    wait(NULL);
    wait(NULL);
    wait(NULL);
}

void process1(){
    int pid2,pid6,pid9;
    pid2=fork();
    if(pid2==0){
        info(BEGIN,2,0);
        process2();

        info(END,2,0);
        exit(0);
    }

    pid6=fork();
    if(pid6==0){
        info(BEGIN,6,0);
        process6();

        info(END,6,0);
        exit(0);
    }

    pid9=fork();
    if(pid9==0){
        info(BEGIN,9,0);
        process9();

        info(END,9,0);
        exit(0);
    }

    wait(NULL);
    wait(NULL);
    wait(NULL);
}


int main(int argc,char** argv)
{
    init();
    info(BEGIN,1,0);
    process1();
    info(END,1,0);

}
