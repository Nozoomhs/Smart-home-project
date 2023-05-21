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

#define CAST                   *(float*)
#define OFFSET                 sizeof(float)
//This offset enables us to access the heating attribute in the shared memory for the first room
//Heating is indicated by 'h' while cooling is indicated by 'c'.
#define BEGIN_HEAT             20 * OFFSET + 1
#define BUFFER_SIZE            20 * OFFSET + 11
//This offset enables us to access the attribute in the shared memory which indicates whether the state is "away" or "home".
//Away is represented by 'a' and home by 'h'
#define AWAY_ELEMENT           shmem[20 * OFFSET]
//This macro enables children to access their respective heating attribute
#define HEATING_ELEMENT        shmem[20 * OFFSET + roomPointer + 1]
#define MAX_CHILD_NUMBER       10
//Next three macros represent the offsets for each of the values in the shared memory
//They are contained in such a way: [Desired temperature when away | Desired temperature when away | Actual measured temperature]
//All values are single precision floating point numbers
#define OFFSET_FOR_ACTUAL      3 * OFFSET + 2 * OFFSET
#define OFFSET_FOR_AWAY        3 * OFFSET
#define OFFSET_FOR_HOME        3 * OFFSET + OFFSET
#define TOPIC_SENSOR           "/sensordata\0"
#define TOPIC_ADD_ROOM         "/control/addroom\0"
#define TOPIC_CONTROL          "/room/heatingInfo\0"
#define TOPIC_CONTROL_TEMP     "/control/temp\0"
#define TOPIC_CONTROL_HOMEAWAY "/control/isHome\0"

char *shmem                                = NULL;
//Tracks the number of children
static int roomPointer                     = 0;
//Indicates whether at least one signal was recieved by the parent
volatile sig_atomic_t ParentRecievedSignal = 0;
//Tracks whether new room has to be created
volatile sig_atomic_t CreateNewRoom        = 0;
//Holds the state of all the rooms
static char diff[MAX_CHILD_NUMBER]          = {'c','c','c','c','c','c','c','c','c','c'};
//Holds the PID-s of all the children so the parent can fire the signals
static int children[MAX_CHILD_NUMBER];

void* create_shared_memory(size_t size) {
    int protection = PROT_READ | PROT_WRITE;
    int visibility = MAP_SHARED | MAP_ANONYMOUS;
    return mmap(NULL, size + 1, protection, visibility, -1, 0);
}

void sig_handler_child(){}

void sig_handler_parent() {ParentRecievedSignal = 1;}

void sig_handler_parent_pipe() {CreateNewRoom = 1;}

void taskOfChildren(struct sigaction *const act){
    sigset_t set; int a = 0;
    sigaddset(&set, SIGRTMIN);
    act->sa_sigaction = sig_handler_child;
    sigaction(SIGRTMIN, act, NULL);
//Children will only signal parent if there was a change
    while(1){
        if(HEATING_ELEMENT == 'c'){
            if(AWAY_ELEMENT == 'a'){
                if(CAST(shmem + roomPointer * OFFSET_FOR_AWAY) > 
                    CAST(shmem + roomPointer * OFFSET_FOR_ACTUAL))
                        kill(getppid(),SIGRTMIN);
            }else{
                if(CAST(shmem + roomPointer * OFFSET_FOR_HOME) > 
                    CAST(shmem + roomPointer * OFFSET_FOR_ACTUAL))
                            kill(getppid(),SIGRTMIN);
            }
        }
        else{
             if(AWAY_ELEMENT == 'a'){
                if(CAST(shmem + roomPointer * OFFSET_FOR_AWAY) <
                    CAST(shmem + roomPointer * OFFSET_FOR_ACTUAL))
                        kill(getppid(),SIGRTMIN);
            }else{
                if(CAST(shmem + roomPointer * OFFSET_FOR_HOME) <
                    CAST(shmem + roomPointer * OFFSET_FOR_ACTUAL))
                            kill(getppid(),SIGRTMIN);
            }
        }	
        sigwait(&set, &a);
    }
}

void addRoom(int ID){
    children[roomPointer] = ID;
    roomPointer++;
}

typedef enum {
    ACTUAL,
    HOME,
    AWAY
} TempType;

void setTemp(const struct mosquitto_message *message, const TempType type){
//Based upon the type of messege recieved we put values in the proper place in the shared memory
    if(type == ACTUAL){
        float r = atof(message->payload);
        for(int i = 0; i < roomPointer; i++){
            memcpy(shmem + i * OFFSET_FOR_ACTUAL, &r, OFFSET);
		}
    }else{
        char* mess = message->payload;  
        char* token = strtok(mess, " ");  
        int counter = 0;
        int roomcounter = 0;
        while(token !=NULL){
            float r = atof(token);
            if(counter%2 == 0){
                memcpy(shmem + roomcounter * OFFSET_FOR_AWAY, &r, OFFSET);
            }else{
                memcpy(shmem + roomcounter * OFFSET_FOR_HOME, &r, OFFSET);
                roomcounter++;
            }
            token = strtok(NULL, " ");      
            counter++;
        }
    }
}

void HomeAway(const struct mosquitto_message *message){
    if(strcmp(message->payload, "true")==0)
        AWAY_ELEMENT = 'h';
    else
        AWAY_ELEMENT = 'a';
}

void signalChildren(){
    for(int i=0;i<roomPointer;i++)
        kill(children[i],SIGRTMIN);
}

void checkValues( struct mosquitto *  const mosq){
    int flag = 0;
	int mid_sent;
    for(int i = 0; i < roomPointer; i++){
        if(AWAY_ELEMENT == 'a'){
            if(CAST(shmem + i * OFFSET_FOR_AWAY) > CAST(shmem + i * OFFSET_FOR_ACTUAL)){
                if(shmem[BEGIN_HEAT + i ] == 'c'){
                    shmem[BEGIN_HEAT + i ] = 'h';
                    diff[i] = 'h';
                    flag = 1;
                }
            }else{
                if(shmem[BEGIN_HEAT + i ] == 'h'){
                    shmem[BEGIN_HEAT + i ] = 'c';
                    diff[i] = 'c';
                    flag = 1;
                }
            }
        }else{
            if(CAST(shmem + i * OFFSET_FOR_HOME) > CAST(shmem + i * OFFSET_FOR_ACTUAL)){
                if(shmem[BEGIN_HEAT + i ] == 'c'){
                    shmem[BEGIN_HEAT + i ] = 'h';
                    diff[i] = 'h';
                    flag = 1;
                }
            }else{
                if(shmem[BEGIN_HEAT + i ] == 'h'){
                    shmem[BEGIN_HEAT + i ] = 'c';
                    diff[i] = 'c';
                    flag = 1;
                }
            }
        }
    }
//We will only send message if there is a change to avoid cluttering
    if(flag == 1)
	    mosquitto_publish(mosq, &mid_sent, TOPIC_CONTROL, roomPointer, diff, 2, 0);
}
//Mosquitto related functions-------------------------------------------
void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{ 
    if (message->payloadlen ){
        if(strcmp(message->topic, TOPIC_ADD_ROOM) == 0){
                CreateNewRoom = 1;
        }
        else if(strcmp(message->topic, TOPIC_SENSOR) == 0){
            setTemp(message,ACTUAL);
        }
        else if(strcmp(message->topic, TOPIC_CONTROL_TEMP)==0){
            if(AWAY_ELEMENT == 'h')
                setTemp(message,HOME);
            else
                setTemp(message,AWAY);
        }
        else if(strcmp(message->topic, TOPIC_CONTROL_HOMEAWAY)==0){
            HomeAway(message);
        }
    }else{
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
        mosquitto_subscribe(mosq, NULL, TOPIC_CONTROL_HOMEAWAY, 0);
    }else{
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
    if(argc != 2)
        return -1;
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
//This element indicates whether the user is home or not
    AWAY_ELEMENT = 'a';
//Initializing the last 10 bytes of the memory to house whether the room is cooling or heating
    memset(&shmem[BUFFER_SIZE - 10],'c', 10 );

    
    if (sigaction (SIGRTMIN, &act, NULL) == -1)
           return -1;

    while(1){
        int forkResult = fork();
        if(forkResult == 0){
//Children
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
//Main thread
                mosquitto_loop(mosq,-1,1);
                signalChildren();
                sleep(1);
//Children signal the parent if they detect a change in their state (one change is enough)
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
