#include <iostream>
#include <unistd.h>
#include <vector>
#include <thread>
#include <chrono>
#include <string.h>
#include <fcntl.h>
#include <mutex>
#include <map>
#include <algorithm>
#include <sys/wait.h>
using namespace std;

#define NUM_OF_PHILOSOPHERS 5

#define READ 0
#define WRITE 1
#define RIGHT 0
#define LEFT 1

#define REQ 0
#define RES 1

#define OK 0
#define P_ERROR 1
#define F_ERROR 3

int pipeline[NUM_OF_PHILOSOPHERS][2][2];

struct Message
{
    int senderId;
    int timestamp;
    int side;
    int requestType;
};

int setLocalLogicClock(int c_i, int c_j)
{
    return max(c_i, c_j) + 1;
}

Message createMessage(int senderId, int timestamp, int side, char requestType)
{
    return {senderId = senderId, timestamp = timestamp, side = side, requestType = requestType};
}

void philosopher(int id)
{
    // setup
    int leftPhilosopher = (id - 1 + NUM_OF_PHILOSOPHERS) % NUM_OF_PHILOSOPHERS;
    int rightPhilosopher = (id + 1) % NUM_OF_PHILOSOPHERS;
    int logicClock = time(0);

    // think and eat forever
    while (true)
    {
        cout << "P" << id << " is thinking" << endl;
        this_thread::sleep_for(chrono::milliseconds(1000));

        // ask for chopsticks
        // left
        Message reqLeft = createMessage(id, logicClock, LEFT, REQ);
        write(pipeline[id][RIGHT][WRITE], &reqLeft, sizeof(reqLeft));
        cout << "P" << id << " --> REQ --> LEFT --> " << rightPhilosopher << " " << reqLeft.timestamp << endl;
        // right
        Message reqRight = createMessage(id, logicClock, RIGHT, REQ);
        write(pipeline[id][LEFT][WRITE], &reqRight, sizeof(reqRight));
        cout << "P" << id << " --> REQ --> RIGHT --> " << leftPhilosopher << " " << reqRight.timestamp << endl;

        // respond to all philosophers waiting for response
        vector<Message> respondLater;

        // wait for responses
        bool resLeft = false;
        bool resRight = false;
        while (!(resLeft && resRight))
        {
            Message receivedMsgRight;
            Message receivedMsgLeft;
            if (read(pipeline[leftPhilosopher][RIGHT][READ], &receivedMsgRight, sizeof(receivedMsgRight)) != -1)
            {
                logicClock = setLocalLogicClock(logicClock, receivedMsgRight.timestamp);
                if (receivedMsgRight.requestType == REQ)
                {
                    cout << "P" << id << " <-- REQ <-- RIGHT <-- " << receivedMsgRight.senderId << " " << receivedMsgRight.timestamp << endl;
                    // request has higher priority ? respond : delay answer
                    if (receivedMsgRight.timestamp < reqLeft.timestamp || (receivedMsgRight.timestamp == reqLeft.timestamp && receivedMsgRight.senderId < id))
                    {
                        Message odgovor = createMessage(id, receivedMsgRight.timestamp, LEFT, RES);
                        write(pipeline[id][LEFT][WRITE], &odgovor, sizeof(odgovor));
                        cout << "P" << id << " --> RES --> " << receivedMsgRight.senderId << " " << receivedMsgRight.timestamp << endl;
                    }
                    else
                    {
                        cout << "P" << id << " --> SAVES REQ --> RIGHT --> " << receivedMsgRight.senderId << " " << receivedMsgRight.timestamp << endl;
                        respondLater.push_back({id, receivedMsgRight.timestamp, receivedMsgRight.side, RES}); // spremi odgovor
                    }
                }
                else if (receivedMsgRight.requestType == RES)
                {
                    // we got response message -> yay
                    resLeft = true;
                    cout << "P" << id << " <-- RES <-- " << receivedMsgRight.senderId << " " << receivedMsgRight.timestamp << endl;
                }
            }
            else if (read(pipeline[rightPhilosopher][LEFT][READ], &receivedMsgLeft, sizeof(receivedMsgLeft)) != -1)
            {
                logicClock = setLocalLogicClock(logicClock, receivedMsgLeft.timestamp);
                if (receivedMsgLeft.requestType == REQ)
                {
                    cout << "P" << id << " <-- REQ <-- LEFT " << receivedMsgLeft.senderId << " " << receivedMsgLeft.timestamp << endl;
                    // if (request has higher priority) -> respond : delay answer
                    if (receivedMsgLeft.timestamp < reqRight.timestamp || (receivedMsgLeft.timestamp == reqRight.timestamp && receivedMsgLeft.senderId < id))
                    {
                        Message res = createMessage(id, receivedMsgLeft.timestamp, RIGHT, RES);
                        write(pipeline[id][RIGHT][WRITE], &res, sizeof(res));
                        cout << "P" << id << " --> RES " << receivedMsgLeft.senderId << endl;
                    }
                    else
                    {
                        cout << "P" << id << " --> SAVE REQ --> LEFT " << receivedMsgLeft.senderId << " " << receivedMsgLeft.timestamp << endl;
                        respondLater.push_back({id, receivedMsgLeft.timestamp, receivedMsgLeft.side, RES}); // spremi odgovor
                    }
                }
                else if (receivedMsgLeft.requestType == RES)
                {
                    // we got response message -> yay
                    resRight = true;
                    cout << "P" << id << " <-- RES <-- " << receivedMsgLeft.senderId << " " << receivedMsgLeft.timestamp << endl;
                }
            }
        }

        // eating
        cout << "--> P" << id << " is eating " << endl;
        this_thread::sleep_for(chrono::milliseconds(1000));

        // send delayed responses
        for (Message delayedResponse : respondLater)
        {
            write(pipeline[id][delayedResponse.side][WRITE], &delayedResponse, sizeof(delayedResponse));
            cout << "P" << id << " --> DELAYED RES" << " --> " << (delayedResponse.side ? "LEFT" : "RIGHT") << " --> " << (delayedResponse.side ? rightPhilosopher : leftPhilosopher) << " " << delayedResponse.timestamp << endl;
        }
    }
}

void sigint_handler(int signum)
{
    cout << "SIGINT ---> cleaning up and exiting" << endl;
    for (int i = 0; i < NUM_OF_PHILOSOPHERS; i++)
    {
        close(pipeline[i][RIGHT][READ]);
        close(pipeline[i][RIGHT][WRITE]);
        close(pipeline[i][LEFT][READ]);
        close(pipeline[i][LEFT][WRITE]);
    }

    exit(OK);
}

int main(void)
{
    signal(SIGINT, sigint_handler);

    // create pipeline for each philosopher
    for (int i = 0; i < NUM_OF_PHILOSOPHERS; i++)
    {
        if (pipe(pipeline[i][RIGHT]) == -1 || pipe(pipeline[i][LEFT]) == -1)
            exit(P_ERROR);

        fcntl(pipeline[i][RIGHT][READ], F_SETFL, O_NONBLOCK);
        fcntl(pipeline[i][LEFT][READ], F_SETFL, O_NONBLOCK);
    }

    // fork child process
    for (int i = 0; i < NUM_OF_PHILOSOPHERS; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            philosopher(i);
            exit(OK);
        }
        else if (pid < 0)
        {
            exit(P_ERROR);
        }
    }

    // wait for process exit
    for (int i = 0; i < NUM_OF_PHILOSOPHERS; ++i)
        wait(NULL);

    return OK;
}