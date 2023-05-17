#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#define CAST             *(double*)
#define OFFSET           sizeof(double)
#define BUFFER_SIZE      20 * OFFSET
#define MAX_CHILD_NUMBER 10

char *shmem                                = NULL;
static int roomPointer                     = 0;
volatile sig_atomic_t ParentRecievedSignal = 0;
volatile sig_atomic_t CreateNewRoom        = 0;
static int children[MAX_CHILD_NUMBER];

void* create_shared_memory(size_t size) {
    int protection = PROT_READ | PROT_WRITE;
    int visibility = MAP_SHARED | MAP_ANONYMOUS;
    return mmap(NULL, size, protection, visibility, -1, 0);
}

void sig_handler_child(){}

void sig_handler_parent() {ParentRecievedSignal = 1;}

void sig_handler_parent_pipe() {CreateNewRoom = 1;}

void taskOfChildren(struct sigaction *const act){
   // int current_temp = 0;
    sigset_t set; int a = 0;
    sigaddset(&set, SIGRTMIN);
   // signal(SIGPIPE, sig_handler_child);
    act->sa_sigaction = sig_handler_child;
    sigaction(SIGRTMIN, act, NULL);
    double tempArray[2];
    while(1){
    printf("%f - %f\n",CAST(shmem + roomPointer * OFFSET),CAST(shmem + roomPointer *OFFSET + OFFSET));
        if(CAST(shmem + roomPointer *OFFSET) > 
           CAST(shmem + roomPointer *OFFSET + OFFSET))
            kill(getppid(),SIGRTMIN);
        sigwait(&set, &a);
    }
}

void addRoom(int ID){
    children[roomPointer] = ID;
    roomPointer++;
}

//This function shall be replaced by the real values read from mosquitto
void getTemp(){
    double tempArray[20];
    for(int i = 0; i < roomPointer; i++){
        double r1 = (double)(rand()%128);
        double r2 = (double)(rand()%128);

        tempArray[i]     = r1;
        tempArray[i + 1] = r2; 
        memcpy(shmem + i * OFFSET * 2, tempArray, OFFSET * 2);
    }
}
//-----------------------------------------------------------------------

void signalChildren(){
    for(int i=0;i<roomPointer;i++)
        kill(children[i],SIGRTMIN);
}

void checkValues(){
    for(int i = 0; i < roomPointer; i++){
        if(CAST(shmem + i * OFFSET) > CAST(shmem + i * OFFSET + OFFSET)){
            printf("Room %d is HEATING\n",i);
        }else{
            printf("Room %d is COOLING\n",i);
        }
    }
}

int main(){
//Allocating shared memory
    if(!shmem)
        shmem = create_shared_memory(BUFFER_SIZE);
    memset(shmem, '\0', BUFFER_SIZE);
//Initialization of signal
    struct sigaction act;
    act.sa_flags = 0;
    memset (&act, 0, sizeof (act));

    
    if (sigaction (SIGRTMIN, &act, NULL) == -1)
           return -1;

    while(1){
        int forkResult = fork();
        printf("fork result %d\n",forkResult);
        if(forkResult == 0){
            printf("Child starts\n");
            taskOfChildren(&act);
        }else{
            addRoom(forkResult);
//Set signal handler for SIGRTMIN
            struct sigaction act2;
            act2.sa_flags = 0;
            memset (&act2, 0, sizeof (act));
            act2.sa_sigaction = sig_handler_parent_pipe;
            act.sa_sigaction = sig_handler_parent;
            sigaction(SIGRTMIN, &act, NULL);
            sigaction(SIGPIPE, &act2, NULL);
//Waiting 'till child initiates it's handler
            sleep(1);
            while(1){
                getTemp(shmem);
                sleep(2);
                signalChildren();
                sleep(2);
                //printf("Parent signal status = %d\n",ParentRecievedSignal);
                printf("-----Next step-----\n");
                if(ParentRecievedSignal)
                    checkValues();
                ParentRecievedSignal = 0;
                if(CreateNewRoom && roomPointer < MAX_CHILD_NUMBER){
                    CreateNewRoom = 0;
                    break;
                }
            }
        }
    }
    return 0;
}
