#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sched.h>
#include <mosquitto.h>
#include <string.h>

struct position{
    short x;
    short y;
};
#define TOPIC_CONTROL_GPS  "/gps/isHome"
#define MAX_DISTANCE 100
double getTime()
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return now.tv_sec + now.tv_nsec * 1e-9;
}

void restartTimer(const timer_t timerID, struct itimerspec *const timer){
    timer->it_interval.tv_sec = 5;
    timer->it_value.tv_sec = 5;
    timer_settime(timerID, 0, timer, NULL);
}

int checkPosition(const struct position pos){
    return pos.x > 47 && pos.x < 80 &&
           pos.y > 47 && pos.y < 75;
}

short moveAround(const short val){
    short x = rand()%20;
//Making a coordinate system in a 100x100 mx
    if(val - x  < 0){
        return 1;
    }else if(val + x > 100){
        return -1;
    }else{
        if(x < 11)
            return -x;
        else
            return x;
    }
}

void sighandler(){
    exit(1);
}

   
void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
    printf("Sent message: %s",message->payload);
    fflush(stdout);
}

void my_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
    if (!result){
   
        printf("Connected");
    }
    else{
        printf("Connect failed\n");
    }
}


int main(int argc, char *argv[]){
    timer_t timerID;
    int count           = 0;
    int overrunsum      = 0;
    double startTime    = 0.0;
    double elapsedSum   = 0.0;
    struct position pos = {50,50};

    //SET RR SCHEDULER FOR THIS PROCESS
    struct sched_param schedpar;
    schedpar.sched_priority = 12;

    //CREATE TIMER WITH RT SIGNAL
    struct sigevent sigev;
    sigev.sigev_notify = SIGEV_SIGNAL;
    sigev.sigev_signo = SIGRTMIN + 4;
    sigev.sigev_value.sival_ptr = &timerID; //Passing the timer's ID for the sigactionHandler
    
    //1. parameter: The timer will use this clock
    //2. parameter: Raised sigevent on expiration (NULL means SIGALRM)
    //3. parameter: The generated timer's ID
    if (timer_create(CLOCK_REALTIME, &sigev, &timerID))
    {
        perror("Failed to create Timer");
        exit(1);
    }

    //Register signal handler
    struct sigaction sigact;
    sigemptyset(&sigact.sa_mask); //no blocked signals only the one, which arrives
    sigact.sa_sigaction = sighandler;
    sigact.sa_flags = SA_SIGINFO;
    sigaction(SIGRTMIN + 4, &sigact, NULL); //an alarm signal is set    

    struct itimerspec timer;
    timer.it_interval.tv_sec = 5;
    timer.it_interval.tv_nsec = 0;
    timer.it_value.tv_sec = 5;
    timer.it_value.tv_nsec = 0;

    timer_settime(timerID, 0, &timer, NULL);

    struct itimerspec expires;

    struct mosquitto *mosq = NULL;
    mosquitto_lib_init();
    mosq = mosquitto_new("gpsmodule", true, NULL);
    // 1st arg: id
    // 2nd arg: clean_session
    // 3rd arg: userData
    if (!mosq){
        printf("Error: Out of memory.\n");
        return 1;
    }

//60 is the keepalive
    if (mosquitto_connect(mosq, argv[1], 2023, 60)){
        printf("Unable to connect.\n");
        return 1;
    }

    mosquitto_connect_callback_set(mosq, my_connect_callback);
    mosquitto_message_callback_set(mosq, my_message_callback);
    int  return_value;
    int* pt;
    char* true_val = "true";
    char* false_val = "false";
    while (1)
    {
        pos.x += moveAround(pos.x);
        pos.y += moveAround(pos.y);
        printf("%d\n",checkPosition(pos));
        return_value = checkPosition(pos);
        pt = &return_value;
        if(return_value == 1)
        {
            mosquitto_publish(mosq, NULL, TOPIC_CONTROL_GPS , strlen(true_val),true_val, 0, false);
        }
        else
        mosquitto_publish(mosq, NULL, TOPIC_CONTROL_GPS , strlen(false_val),false_val, 0, false);
        //mosquitto_publish(mosq, NULL, TOPIC_CONTROL_GPS , sizeof(int),pt, 0, false);
        sleep(1);
        printf("Position: x:%d y:%d\n",pos.x,pos.y);
        timer_gettime(timerID, &expires);
        printf("\tTimer will be expired after %li seconds\n", expires.it_value.tv_sec);
        restartTimer(timerID, &timer);
    }

    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;

}
