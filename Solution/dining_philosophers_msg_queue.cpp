#include <iostream>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <vector>
#include <atomic>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <sys/wait.h>
#include <cstring>
#include <queue>
#include <map>
using namespace std;

#define NUM_OF_PHILOSOPHERS 5

#define REQUEST 1
#define RESPONSE 2
#define EXIT 3

#define RIGHT 1
#define LEFT 2

#define OK 0
#define P_ERROR 1
#define F_ERROR 3

vector<int> msgQueueIds(NUM_OF_PHILOSOPHERS);

struct Message
{
    long mtype;
    int senderId;
    int timestamp;
    int requestType;
    int side;
};

struct CompareFunction
{
    bool operator()(const Message &a, const Message &b)
    {
        return a.timestamp != b.timestamp ? a.timestamp > b.timestamp : a.senderId > b.senderId;
    }
};

void cleanQueue(std::priority_queue<Message, std::vector<Message>, CompareFunction> &msgQueue, Message &receivedMsg)
{
    std::priority_queue<Message, std::vector<Message>, CompareFunction> tempQueue;
    while (!msgQueue.empty())
    {
        Message msg = msgQueue.top();
        msgQueue.pop();
        if (!(msg.side == receivedMsg.side && msg.senderId == receivedMsg.senderId))
            tempQueue.push(msg);
    }

    msgQueue.swap(tempQueue);
}

int setLocalLogicClock(int c_i, int c_j)
{
    return max(c_i, c_j) + 1;
}

Message createMessage(int mtype, int senderId, int timestamp, int requestType, int side)
{
    return {mtype = mtype, senderId = senderId, timestamp = timestamp, requestType = requestType, side = side};
}

bool canEnterCriticalSection(int responses, int id, std::priority_queue<Message, std::vector<Message>, CompareFunction> &msgQueue)
{
    if (responses < 2 || msgQueue.size() < 2)
        return false;

    Message first = msgQueue.top();
    msgQueue.pop();
    Message second = msgQueue.top();
    msgQueue.push(first);

    return first.senderId == id && second.senderId == id ? true : false;
}

void philosopher(int id)
{
    // setup
    int leftPhilosopher = (id - 1 + NUM_OF_PHILOSOPHERS) % NUM_OF_PHILOSOPHERS;
    int rightPhilosopher = (id + 1) % NUM_OF_PHILOSOPHERS;
    int logicClock = time(0);                                           // local time
    priority_queue<Message, vector<Message>, CompareFunction> msgQueue; // philospher's message queue

    // think and eat forever
    while (true)
    {
        cout << "P" << id << " is thinking" << endl;
        this_thread::sleep_for(chrono::milliseconds(1000));

        // ask for chopsticks
        // left
        Message requestMsgLeft = createMessage(1, id, logicClock, REQUEST, LEFT);
        msgQueue.push(requestMsgLeft);
        if (msgsnd(msgQueueIds[leftPhilosopher], &requestMsgLeft, sizeof(requestMsgLeft), 0) == -1)
            exit(F_ERROR);
        cout << "P" << id << " REQUESTS LEFT " << logicClock << endl;
        // right
        Message requestMsgRight = createMessage(1, id, logicClock, REQUEST, RIGHT);
        msgQueue.push(requestMsgRight);
        if (msgsnd(msgQueueIds[rightPhilosopher], &requestMsgRight, sizeof(requestMsgRight), 0) == -1)
            exit(F_ERROR);
        cout << "P" << id << " REQUESTS RIGHT " << logicClock << endl;

        // wait for responses
        int responses = 0;
        do
        {
            Message receivedMsg;
            if (msgrcv(msgQueueIds[id], &receivedMsg, sizeof(receivedMsg), 1, 0) != -1)
            {
                // refresh local logic clock
                logicClock = setLocalLogicClock(logicClock, receivedMsg.timestamp);
                if (receivedMsg.requestType == REQUEST)
                {
                    cout << "P" << id << " RECEIVES REQUEST F" << receivedMsg.senderId << " " << logicClock << endl;
                    msgQueue.push(receivedMsg);
                    Message res = createMessage(1, receivedMsg.senderId, logicClock, RESPONSE, receivedMsg.side);
                    if (msgsnd(msgQueueIds[receivedMsg.senderId], &res, sizeof(res), 0) == -1)
                        exit(F_ERROR);

                    cout << "P" << id << " SENDS RESPONSE P" << receivedMsg.senderId << " " << logicClock << endl;
                }
                else if (receivedMsg.requestType == RESPONSE)
                {
                    cout << "P" << id << " RECEIVES RESPONSE P" << receivedMsg.senderId << " " << logicClock << endl;
                    responses++;
                }
                else if (receivedMsg.requestType == EXIT)
                {
                    cout << "P" << id << " RECEIVES EXIT F" << receivedMsg.senderId << " " << logicClock << endl;
                    cleanQueue(msgQueue, receivedMsg);
                }
            }
        } while (!canEnterCriticalSection(responses, id, msgQueue));

        // eating
        cout << "P" << id << " is eating" << endl;
        this_thread::sleep_for(chrono::milliseconds(1000));

        msgQueue.pop();
        msgQueue.pop();

        Message exitLeft = createMessage(1, id, requestMsgLeft.timestamp, EXIT, requestMsgLeft.side);
        Message exitRight = createMessage(1, id, requestMsgRight.timestamp, EXIT, requestMsgRight.side);

        if (msgsnd(msgQueueIds[leftPhilosopher], &exitLeft, sizeof(exitLeft), 0) == -1)
            exit(F_ERROR);
        cout << "P" << id << " EXITS LEFT" << endl;

        if (msgsnd(msgQueueIds[rightPhilosopher], &exitRight, sizeof(exitRight), 0) == -1)
            exit(F_ERROR);
        cout << "P" << id << " EXITS RIGHT" << endl;
    }
}

void sigintHandler(int signum)
{
    cout << "SIGINT ---> cleaning up and exiting" << endl;
    for (int i = 0; i < NUM_OF_PHILOSOPHERS; ++i)
        msgctl(msgQueueIds[i], IPC_RMID, NULL);

    exit(OK);
}

int main(void)
{
    // SIGINT handler
    signal(SIGINT, sigintHandler);

    // message queue for each philosopher
    for (int i = 0; i < NUM_OF_PHILOSOPHERS; i++)
    {
        key_t key = getuid() + i;
        int msgqId = msgget(key, 0666 | IPC_CREAT);
        if (msgqId == -1)
            exit(P_ERROR);

        msgQueueIds[i] = msgqId;
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

    exit(OK);
}