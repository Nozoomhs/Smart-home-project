#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <mosquitto.h>
#include <sys/mman.h>


#define CAST             *(float*)
#define OFFSET           sizeof(float)
#define BUFFER_SIZE      20 * OFFSET
#define MAX_CHILD_NUMBER 10
#define TOPIC_SENSOR        "/sensordata\0"
#define TOPIC_ADD_ROOM      "/addroom\0"
#define TOPIC_CONTROL       "/control/feedback\0"
#define TOPIC_CONTROL_TEMP  "/control/temp\0"
#define TOPIC_CONTROL_GPS  "/control/isHome\0"

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
int lock_memory(char   *addr,
            size_t  size)
{
  unsigned long    page_offset, page_size;

  page_size = sysconf(_SC_PAGE_SIZE);
  page_offset = (unsigned long) addr % page_size;

  addr -= page_offset;  /* Adjust addr to page boundary */
  size += page_offset;  /* Adjust size with page_offset */

  return ( mlock(addr, size) );  /* Lock the memory */
}
int unlock_memory(char   *addr,
              size_t  size)
{
  unsigned long    page_offset, page_size;

  page_size = sysconf(_SC_PAGE_SIZE);
  page_offset = (unsigned long) addr % page_size;

  addr -= page_offset;  /* Adjust addr to page boundary */
  size += page_offset;  /* Adjust size with page_offset */

  return ( munlock(addr, size) );  /* Unlock the memory */
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
    float tempArray[2];
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


// 
//This function shall be replaced by the real values read from mosquitto
void setTemp(const struct mosquitto_message *message, bool isDesired){
    for(int i = 0; i < roomPointer; i++){
        char* mess = message->payload;  
        char *temp = strtok(mess, " ");  
        float r = atof(temp);
        if(!isDesired){
            memcpy(shmem+i*2*OFFSET+OFFSET, &r, OFFSET);
        }
        else
            memcpy(shmem+i*2*OFFSET, &r, OFFSET);

    }

}
void arrivingHome(){
    printf("Somebody is getting home ");
}

//-----------------------------------------------------------------------

void signalChildren(){
    for(int i=0;i<roomPointer;i++)
        kill(children[i],SIGRTMIN);
}

void checkValues( struct mosquitto *  const mosq){
    char buffer[BUFFER_SIZE];
//MQTT will write return value here
    int mid_sent;
    for(int i = 0; i < roomPointer; i++){
        if(CAST(shmem + i * OFFSET) > CAST(shmem + i * OFFSET + OFFSET)){
            snprintf(buffer,BUFFER_SIZE,"Room %d is HEATING\n",i);
            printf("Room %d is HEATING\n",i);
        }else{
            snprintf(buffer,BUFFER_SIZE,"Room %d is COOLING\n",i);
            printf("Room %d is COOLING\n",i);
        }
//QoS=2 means the message will *exactly* arrive once
        mosquitto_publish(mosq, &mid_sent, TOPIC_CONTROL, strlen(buffer), buffer, 2, 0);
        //mosquitto_loop(mosq,1,1);
        memset(buffer,0,BUFFER_SIZE);
    }
}
//Mosquitto related functions-------------------------------------------
void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
    
    if (message->payloadlen ){
        if(strcmp(message->topic, TOPIC_ADD_ROOM) == 0){
                CreateNewRoom = 1;
        }
        else if(strcmp(message->topic, TOPIC_SENSOR) == 0){
            //printf("%s", message->payload);
            setTemp(message,false);
        }
        else if(strcmp(message->topic, TOPIC_CONTROL_TEMP)==0){
            setTemp(message,true);
        }
        else if(strcmp(message->topic, TOPIC_CONTROL_GPS)==0){
            arrivingHome();
        }
    }
    else{
        printf("%s (null)\n", message->topic);
    }
    
    fflush(stdout);
}

void my_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
    if (!result){
        /* Subscribe to broker information topics on successful connect. */
        mosquitto_subscribe(mosq, NULL, TOPIC_SENSOR, 0);
        mosquitto_subscribe(mosq, NULL, TOPIC_ADD_ROOM, 0);
        mosquitto_subscribe(mosq, NULL, TOPIC_CONTROL_TEMP, 0);
    }
    else{
        printf("Connect failed\n");
    }
}

void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
    printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
    for (int i = 1; i < qos_count; i++){
        printf(", %d", granted_qos[i]);
    }
    printf("\n");
}


int main(int argc, char *argv[]){
//Mosquitto related variables-------------------------------------------
    if(argc != 2){
        return -1;
    }

    struct mosquitto *mosq = NULL;
    mosquitto_lib_init();
    mosq = mosquitto_new("roommodule", true, NULL);
    // 1st arg: id
    // 2nd arg: clean_session
    // 3rd arg: userData
    if (!mosq){
        printf("Error: Out of memory.\n");
        return 1;
    }
    mosquitto_connect_callback_set(mosq, my_connect_callback);
    mosquitto_message_callback_set(mosq, my_message_callback);
    mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);
//60 is the keepalive
    if (mosquitto_connect(mosq, argv[1], 2023, 60)){
        printf("Unable to connect.\n");
        return 1;
    }
//2nd arg: timeout
//3rd: max_packets
//----------------------------------------------------------------------
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
                mosquitto_loop(mosq,-1,2);
                signalChildren();
                sleep(2);
                //printf("Parent signal status = %d\n",ParentRecievedSignal);
                printf("-----Next step-----\n");
                if(ParentRecievedSignal)
                    checkValues(mosq);
                ParentRecievedSignal = 0;
                if(CreateNewRoom && roomPointer < MAX_CHILD_NUMBER){
                    CreateNewRoom = 0;
                    break;
                }
            }
        }
    }
    
    mosquitto_disconnect(mosq);
    mosquitto_loop_stop(mosq,false);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}
