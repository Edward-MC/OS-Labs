#include <iostream>
#include <queue>
#include <vector>
#include <deque>
#include <stack>
#include <fstream>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <string>
using namespace std;

// emunerate all the status
enum TRANS {TRANS_TO_READY, TRANS_TO_RUN, TRANS_TO_BLOCK, TRANS_TO_PREEMPT};
enum STATUS {CREATED, READY, RUN, BLOCKED, PREEMPT};

//do all the initialize
int quantum = 10000;
int maxprio = 4;
int CURRENT_TIME = 0;
bool CALL_SCHEDULER = false;

//used to store process time info
class ProcTime
{
public:
    int finTime;
    int turnaroundTime;
    int IOTime;
    int waitTime;
    
    ProcTime()
    {
        this->finTime = 0;
        this->turnaroundTime = 0;
        this->IOTime = 0;
        this->waitTime = 0;
    }
};

class Process
{
public:
    int arriveTime;
    int totalCpuTime;
    int cpuBurst;
    int randCB; //store the random cpu burst
    int IOBurst;
    int prio;   // dynamic priority
    int status;
    int state_ts;
    int pid;
    int rem;    //remaining cpu time
    int addIndex; //used to compare who is added to the runQueue first
    int static_prio;
    ProcTime* timeInfo;
};

class Event
{
public:
    int timeStamp;
    Process* proc;
    int status;
    int transition;
    int addIndex; //used to compare who is added to the eventQueue first
};

//define the compare function for priority
struct comp
{
    bool operator() (Event* a, Event* b)
    {
        if(a->timeStamp != b->timeStamp)
        {
            return a->timeStamp > b->timeStamp;
        }else{
            //if the timeStamp is the same, compare the time added to the queue
            return a->addIndex > b->addIndex;
        }
    }
};

//define the base class for the schedulers
class Scheduler
{
public:
    queue<Process*> runQueue;

    virtual ~Scheduler() {};
    
    virtual void add_process(Process* p) = 0;
    virtual Process* get_next_process() = 0;
    virtual bool test_preempt() = 0;
};


class FCFS: public Scheduler
{
public:
    queue<Process*> runQueue;
    
    
    void add_process(Process* p)
    {
        runQueue.push(p);
    }

    Process* get_next_process()
    {
        if(!runQueue.empty())
        {
            Process* proc = runQueue.front();
            runQueue.pop();
            return proc;
        }else{
            return NULL;
        }
    }
    
    bool test_preempt()
    {
        return false;
    }
};

class LCFS: public Scheduler
{
public:
    stack<Process*> runQueue;

    void add_process(Process* p)
    {
        runQueue.push(p);
    }
    
    Process* get_next_process()
    {
        if(!runQueue.empty())
        {
            Process* proc = runQueue.top();
            runQueue.pop();
            return proc;
        }else{
            return NULL;
        }
    }
    
    bool test_preempt()
    {
        return false;
    }
};

// used for SRTF priority elements comparison
struct SRTFComp
{
    bool operator() (Process* a, Process* b)
    {
        if(a->rem != b->rem)
        {
            return a->rem > b->rem;
        }else{
            return a->addIndex > b->addIndex;
        }
    }
};

//use priority here to compare remaining cpu time
class SRTF: public Scheduler
{
public:
    priority_queue<Process*, vector<Process*>, SRTFComp > runQueue;
    

    void add_process(Process* p)
    {
        runQueue.push(p);
    }
    
    Process* get_next_process()
    {
        if(!runQueue.empty())
        {
            Process* proc = runQueue.top();
            runQueue.pop();
            return proc;
        }else{
            return NULL;
        }
    }
    
    bool test_preempt()
    {
        return false;
    }
    
};

class RR: public Scheduler
{
public:
    queue<Process*> runQueue;
    
    
    void add_process(Process* p)
    {
        runQueue.push(p);
    }
    
    Process* get_next_process()
    {
        if(!runQueue.empty())
        {
            Process* proc = runQueue.front();
            runQueue.pop();
            return proc;
        }else{
            return NULL;
        }
    }
    
    bool test_preempt()
    {
        return false;
    }
};


class PRIO: public Scheduler
{
public:
    deque<Process*> *activeQ;
    deque<Process*> *expiredQ;
    
    PRIO()
    {
        this->activeQ = new deque<Process*> [maxprio];
        this->expiredQ = new deque<Process*> [maxprio];
    }
    
    ~PRIO()
    {
        delete [] activeQ;
        delete [] expiredQ;
    }
    
    //judge if the queue are empty
    bool judgeIfNull()
    {
        for(int i = 0; i < maxprio; ++i)
        {
            if(!activeQ[i].empty() || !expiredQ[i].empty())
            {
                return false;
            }
        }
        return true;
    }
    
    //judge if the active queue and expired queue need to exchange
    bool judgeIfExchange()
    {
        for(int i = 0; i < maxprio; ++i)
        {
            if(!this->activeQ[i].empty())
            {
                return false;
            }
        }
        return true;
    }
    
    void add_process(Process* p)
    {
        //set prio
        //if transition comes from run, then prio--
        if(p->status == RUN)
        {
            p->prio--;
        }
        //if transition comes from Block, then prio=static prio-1
        if(p->status == BLOCKED)
        {
            p->prio = p->static_prio - 1;
        }
        
        //add to queue according to prio
        //if prio reaches -1, reset it to static prio-1, and add it to expired queue
        if(p->prio == -1)
        {
            p->prio = p->static_prio - 1;
            this->expiredQ[p->prio].push_back(p);
        }else{
            this->activeQ[p->prio].push_back(p);
        }
    }
    
    //print the static of active queue and expried queue
    void printQueues()
    {
        printf("activeQ: \n");
        for(int i = 0; i < maxprio; ++i)
        {
            if(!activeQ[i].empty())
            {
                printf("%d: ", i);
                for(int aq = 0; aq < activeQ[i].size(); ++aq)
                {
                    Process* p = activeQ[i][aq];
                    printf("%d:%d \n", p->pid, p->prio);
                }
            }else{
                printf("%d: \n", i);
            }
        }
        
        printf("expiredQ: \n");
        for(int i = 0; i < maxprio; ++i)
        {
            if(!expiredQ[i].empty())
            {
                printf("%d: ", i);
                for(int aq = 0; aq < expiredQ[i].size(); ++aq)
                {
                    Process* p = expiredQ[i][aq];
                    printf("%d:%d \n", p->pid, p->prio);
                }
            }else{
                printf("%d: \n", i);
            }
        }
    }
    
    Process* get_next_process()
    {
        //get the active queue and expired queue info
//        printQueues();
        Process* result = NULL;

        // have to examine null first, or will put empty situation to exchange situation
        if(this->judgeIfNull())
        {
            return NULL;
        }
    
        // examine if we should change the expiredQueue with runQueue
        if(this->judgeIfExchange())
        {
            deque<Process*> *temp = activeQ;
            activeQ = expiredQ;
            expiredQ = temp;
        }
        for(int i = maxprio-1; i >= 0; --i)
        {
            if(!activeQ[i].empty())
            {
                result = activeQ[i].front();
                activeQ[i].pop_front();
                return result; //should add return here since we should find the first element in highest prio queue
            }
        }
        
        return result;
        
        
    }
    
    bool test_preempt()
    {
        return false;
    }
};

//basically the same as PRIO except test_preempt function
class PREPRIO: public Scheduler
{
public:
    deque<Process*> *activeQ;
    deque<Process*> *expiredQ;
    
    PREPRIO()
    {
        this->activeQ = new deque<Process*> [maxprio];
        this->expiredQ = new deque<Process*> [maxprio];
    }
    
    ~PREPRIO()
    {
        delete [] activeQ;
        delete [] expiredQ;
    }
    
    bool judgeIfNull()
    {
        for(int i = 0; i < maxprio; ++i)
        {
            if(!activeQ[i].empty() || !expiredQ[i].empty())
            {
                return false;
            }
        }
        return true;
    }
    
    bool judgeIfExchange()
    {
        for(int i = 0; i < maxprio; ++i)
        {
            if(!this->activeQ[i].empty())
            {
                return false;
            }
        }
        return true;
    }
    
    void printQueues()
    {
        printf("activeQ: \n");
        for(int i = 0; i < maxprio; ++i)
        {
            if(!activeQ[i].empty())
            {
                printf("%d: ", i);
                for(int aq = 0; aq < activeQ[i].size(); ++aq)
                {
                    Process* p = activeQ[i][aq];
                    printf("%d:%d \n", p->pid, p->prio);
                }
            }else{
                printf("%d: \n", i);
            }
        }
        
        printf("expiredQ: \n");
        for(int i = 0; i < maxprio; ++i)
        {
            if(!expiredQ[i].empty())
            {
                printf("%d: ", i);
                for(int aq = 0; aq < expiredQ[i].size(); ++aq)
                {
                    Process* p = expiredQ[i][aq];
                    printf("%d:%d \n", p->pid, p->prio);
                }
            }else{
                printf("%d: \n", i);
            }
        }
    }
    
    
    void add_process(Process* p)
    {
        if(p->status == RUN)
        {
            p->prio--;
        }
        if(p->status == BLOCKED)
        {
            p->prio = p->static_prio - 1;
        }
        if(p->prio == -1)
        {
            p->prio = p->static_prio - 1;
            this->expiredQ[p->prio].push_back(p);
        }else{
            this->activeQ[p->prio].push_back(p);
        }
    }
    
    Process* get_next_process()
    {
        //get the active queue and expired queue info
//        printQueues();
        Process* result = NULL;
        // have to examine null first, or will put empty situation to exchange situation
        if(this->judgeIfNull())
        {
            return NULL;
        }
        
        // examine if we should change the expiredQueue with runQueue
        if(this->judgeIfExchange())
        {
            deque<Process*> *temp = activeQ;
            activeQ = expiredQ;
            expiredQ = temp;
            
        }
        for(int i = maxprio-1; i >= 0; --i)
        {
            if(!activeQ[i].empty())
            {
                result = activeQ[i].front();
                activeQ[i].pop_front();
                return result;
            }
        }
        return result;
    }
    
    bool test_preempt()
    {
        return true;
    }
    
};

priority_queue<Event*, vector<Event*>, comp > eventQueue;
vector<Process*> proc_queue;
Scheduler* current_scheduler;
Process* CURRENT_RUNNING_PROCESS;
int ofs = 0;
int lineNums = 0;
int randvals[40000];

//Summary Information
int finTime = 0;
double cpuUtil = 0.0;
double IOUtil = 0.0;
double aveTurnaroundTime = 0.0;
double aveCpuWaitTime = 0.0;
double throughput = 0.0;
vector<pair<int,int> > IOTimeStamp;  //used to store the enter and exit time in IO

//Debug options
int setV = 0;
int setT = 0;
int setE = 0;
int setP = 0;
int setS = 0;

// global event/proc count used for order of same timeStamp/remain time
int eventCount = 0;
int runCount = 0;





int myrandom(int burst)
{
    int rand = 1 + (randvals[ofs] % burst);
    ofs = (ofs + 1) % lineNums;   //if reach the end, come again from the start
    return rand;
}

int getRandCB(int CB, int rem)
{
    int randCB = myrandom(CB);
    //if remmain time is less than randCB, set it to remaining time
    if(randCB > rem)
    {
        randCB = rem;
    }
    return randCB;
}

int getRandIO(int IO)
{
    return myrandom(IO);
}



//DES functions
Event* get_event()
{
    if(!eventQueue.empty())
    {
        Event* evt = eventQueue.top();
        eventQueue.pop();
        return evt;
    }else{
        return NULL;
    }
    
}

void put_event(Event* evt)
{
    eventQueue.push(evt);
}

//peek the time of next event
int get_next_event_time()
{
    if(!eventQueue.empty())
    {
        Event* topP = eventQueue.top();
        return topP->timeStamp;
    }else{
        return -1;
    }
    
}

//used for PREPRIO too see if it should preempt
//if return -1, then not found
int get_next_proc_time(Process* p)
{
    int result = -1;
    priority_queue<Event*, vector<Event*>, comp > eventQueueCopy;
    while(!eventQueue.empty())
    {
        Event* evt = eventQueue.top();
        eventQueue.pop();
        eventQueueCopy.push(evt);   //add all to event queue copy to recover
        
        //detect pid here and get timestamp
        if(evt->proc->pid == p->pid)
        {
            result = evt->timeStamp;
            break;
        }
    }
    
    while(!eventQueueCopy.empty())
    {
        eventQueue.push(eventQueueCopy.top());
        eventQueueCopy.pop();
    }
    return result;
}

//used for PREPRIO to delete future event of p
void delete_proc_event(Process* p)
{
    priority_queue<Event*, vector<Event*>, comp > eventQueueCopy;
    while(!eventQueue.empty())
    {
        Event* evt = eventQueue.top();
        eventQueue.pop();
        
        //the pid that is not equal to p->pid will be add back to original event queue
        if(evt->proc->pid != p->pid)
        {
            eventQueueCopy.push(evt);
        }
    }
    
    //put back to original event queue
    while(!eventQueueCopy.empty())
    {
        eventQueue.push(eventQueueCopy.top());
        eventQueueCopy.pop();
    }
}

//transfer from status(int type) to string
char* transEnumToString(int state)
{
    switch (state) {
        case READY:
            return "READY";
            break;
        case BLOCKED:
            return "BLOCK";
            break;
        case CREATED:
            return "CREATED";
            break;
        case RUN:
            return "RUNNG";
            break;
        case PREEMPT:
            return "PREEMPT";
            break;
            
        default:
            return NULL;
    }
}

//used for -e option to print event queue
void printEventQueue()
{
    priority_queue<Event*, vector<Event*>, comp > eventQueueCopy;
    while(!eventQueue.empty())
    {
        // pop the front event from the queue
        Event* evt = eventQueue.top();
        eventQueue.pop();
        
        // print event
        Process* proc = evt->proc;
        printf("%d:%d:%s  ", evt->timeStamp, proc->pid, transEnumToString(evt->status));
        
        // add the event to copy queue
        eventQueueCopy.push(evt);
    }
    
    // add all of the events back to original event queue
    while(!eventQueueCopy.empty())
    {
        Event* evt = eventQueueCopy.top();
        eventQueueCopy.pop();
        eventQueue.push(evt);
    }
}

//Simulation
void simulation()
{
    Event* evt;
    while((evt = get_event()))
    {
        Process* proc = evt->proc;
        CURRENT_TIME = evt->timeStamp;
        int transition = evt->transition;
        int timeInPrevState = CURRENT_TIME - proc->state_ts;
        delete evt; evt = NULL;
        
        ProcTime* pt = proc->timeInfo;


        switch(transition) {  // which state to transition to?
        case TRANS_TO_READY:
            {
                // must come from BLOCKED or from PREEMPTION
                // must add to run queue
                CALL_SCHEDULER = true; // conditional on whether something is run
                
                
                // judge if trans from RUN
                //should update run info and judge if finished here
                if(proc->status == RUN)
                {
                    CURRENT_RUNNING_PROCESS = NULL;
                    proc->rem -= timeInPrevState;   //update remaining cpu time
                    cpuUtil += timeInPrevState;     //update cpu util time
                    if(proc->randCB > timeInPrevState)  //update remaining cpu burst
                    {
                        proc->randCB -= timeInPrevState;
                    }else{
                        proc->randCB = 0;
                    }
                    if(proc->rem == 0)      //process should finish here
                    {
                        pt->finTime = CURRENT_TIME;
                        pt->turnaroundTime = pt->finTime - proc->arriveTime;
                        
                        // record the finish time of last process
                        finTime = CURRENT_TIME;
                        
                        if(setV)
                            printf("%d %d %d: Done\n", CURRENT_TIME, proc->pid, timeInPrevState);
                        
                        break;
                    }else{
                        if(setV)
                        {
                            printf("%d %d %d: %s -> %s cb=%d rem=%d prio=%d\n", CURRENT_TIME, proc->pid, timeInPrevState, "RUNNG", "READY",
                                                           proc->randCB, proc->rem, proc->prio);
                        }
                    }
                }
                
                
                if(proc->status == BLOCKED)
                {
                    // add the IO time to summary info
                    pt->IOTime += timeInPrevState;
                }
                proc->state_ts = CURRENT_TIME;
                proc->addIndex = runCount++;    // update the count each time a proc is added to runqueue, used for sort
                current_scheduler->add_process(proc);
                
                if(proc->status != RUN)
                {
                    // print out state change
                    if(setV)
                    {
                        printf("%d %d %d: %s -> %s\n", CURRENT_TIME, proc->pid, timeInPrevState, transEnumToString(proc->status), "READY");
                    }
                }
               
         
                //deal with E scheduler preemption
                if(current_scheduler->test_preempt())
                {
                    int cur = CURRENT_TIME;
                    Process* cur_running_proc = CURRENT_RUNNING_PROCESS;
                    if(cur_running_proc != NULL)    //some processes are running
                    {
                        if(proc->prio > cur_running_proc->prio)     //the prio of comming process are larger than the running one
                        {
                            if(cur != get_next_proc_time(cur_running_proc))     //not the same time for finishing running, then start preemption
                            {
                                CURRENT_RUNNING_PROCESS = NULL;
                                
                                if(setE)
                                {
                                    //print the remove event info
                                    printf("RemoveEvent(%d):  ", cur_running_proc->pid);
                                    printEventQueue();
                                    printf("==>  ");
                                }
                                
                                //delete the future event of current process
                                delete_proc_event(cur_running_proc);
                                
                                if(setE)
                                {
                                    printEventQueue();
                                    printf("\n");
                                }
                                
                                //create a new preemption event
                                Event* preempt_evt = new Event();
                                preempt_evt->transition = TRANS_TO_READY;
                                preempt_evt->status = PREEMPT;
                                preempt_evt->proc = cur_running_proc;
                                preempt_evt->timeStamp = cur;
                                preempt_evt->addIndex = eventCount++;
                                
                                if(setE)
                                {
                                    printf("  AddEvent(%d:%d:%s):  ", preempt_evt->timeStamp, cur_running_proc->pid, transEnumToString(preempt_evt->status));
                                    printEventQueue();
                                    printf("==>   ");
                                }
                                
                                put_event(preempt_evt);
                                
                                if(setE)
                                {
                                    printEventQueue();
                                    printf("\n");
                                }
                            }

                        }
                    }
                }
             
                proc->status = READY;   //should update status at last since we need old info before
                break;
            }
        case TRANS_TO_RUN:
            {
                // create event for either preemption or blocking
                
//                set current proc to CURRENT_RUNNING_PRC
                CURRENT_RUNNING_PROCESS = proc;
                
                // add ready state time to cpu wait time
                pt->waitTime += timeInPrevState;
                
                // init the random cpu burst
                if(proc->randCB == 0)
                {
                    proc->randCB = getRandCB(proc->cpuBurst, proc->rem);
                }
                
                
                if(setV)
                {
                    printf("%d %d %d: %s -> %s cb=%d rem=%d prio=%d\n", CURRENT_TIME, proc->pid, timeInPrevState, transEnumToString(proc->status), "RUNNG",
                           proc->randCB, proc->rem, proc->prio);
                }
                
                Event* addEvt = new Event();
                if(proc->randCB > quantum)
                {
                    //create preempt event
                    addEvt->transition = TRANS_TO_READY;
                    addEvt->status = PREEMPT;
                    addEvt->proc = proc;
                    
                    addEvt->timeStamp = CURRENT_TIME + quantum;
                    addEvt->addIndex = eventCount++;
                    
                }else{
                    //create blocking event
                    addEvt->transition = TRANS_TO_BLOCK;
                    addEvt->proc = proc;
                    
                    addEvt->timeStamp = CURRENT_TIME + proc->randCB;
                    addEvt->status = BLOCKED;
                    addEvt->addIndex = eventCount++;
                    
                }
                
                
                if(setE)
                {
                    printf("  AddEvent(%d:%d:%s):  ", addEvt->timeStamp, proc->pid, transEnumToString(addEvt->status));
                    printEventQueue();
                    printf("==>   ");
                }
                
                put_event(addEvt);  //put the behavior here to satisfy the printqueue function
                
                proc->status = RUN; //update status at last
                proc->state_ts = CURRENT_TIME;
                
                if(setE)
                {
                    printEventQueue();
                    printf("\n");
                }
                
                break;
            }
        case TRANS_TO_BLOCK:
            {
                // set current running process to null
                CURRENT_RUNNING_PROCESS = NULL;
                CALL_SCHEDULER = true;
                
                
                //Deal with Run status update
                // update relavent info here
                proc->rem -= timeInPrevState;
                cpuUtil += timeInPrevState;
                //update remaining cpu burst
                if(proc->randCB > timeInPrevState)
                {
                    proc->randCB -= timeInPrevState;
                }else{
                    proc->randCB = 0;
                }
                if(proc->rem == 0)  //judge if process finishes here
                {
                    pt->finTime = CURRENT_TIME;
                    pt->turnaroundTime = pt->finTime - proc->arriveTime;

                    // record the finish time of last process
                    finTime = CURRENT_TIME;

                    if(setV)
                        printf("%d %d %d: Done\n", CURRENT_TIME, proc->pid, timeInPrevState);
                    break;
                }
                
                //Deal with IO itself
                // get the random IO burst
                int randIO = getRandIO(proc->IOBurst);
                
                // record the enter and exit time for IO
                pair<int,int> timeStamp = make_pair(CURRENT_TIME, CURRENT_TIME + randIO);
                IOTimeStamp.push_back(timeStamp);
                
                if(setV)
                {
                    printf("%d %d %d: %s -> %s ib=%d rem=%d\n", CURRENT_TIME, proc->pid, timeInPrevState, transEnumToString(proc->status), "BLOCK",
                           randIO, proc->rem);
                }
                
                if(setE)
                {
                    printf("  AddEvent(%d:%d:%s):  ", CURRENT_TIME + randIO, proc->pid, "READY");
                    printEventQueue();
                    printf("==>   ");
                }
                
                
                //create an event for when process becomes READY again
                Event* readyEvt = new Event();
                readyEvt->transition = TRANS_TO_READY;
                readyEvt->proc = proc;
                readyEvt->timeStamp = CURRENT_TIME + randIO;
                proc->state_ts = CURRENT_TIME;
                readyEvt->status = READY;
                readyEvt->addIndex = eventCount++;
                put_event(readyEvt);
                
                if(setE)
                {
                    printEventQueue();
                    printf("\n");
                }
                
                
                proc->status = BLOCKED;
                break;
            }
        case TRANS_TO_PREEMPT:
            {
                //Not use this status
                // add to runqueue (no event is generated)
                current_scheduler->add_process(proc);
                
                CALL_SCHEDULER = true;
                
                if(setV)
                {
                    printf("%d %d %d: %s -> %s\n", CURRENT_TIME, proc->pid, timeInPrevState, transEnumToString(proc->status), "PREEMPT");
                }
                proc->status = PREEMPT;
                break;
            }
        }
        if(CALL_SCHEDULER)
    {
            if (get_next_event_time() == CURRENT_TIME)
            {
                continue; //process next event from Event queue
            }
            CALL_SCHEDULER = false; // reset global flag
            if (CURRENT_RUNNING_PROCESS == NULL) {
                CURRENT_RUNNING_PROCESS = current_scheduler->get_next_process();    //get next run process from scheduler
                if(CURRENT_RUNNING_PROCESS == NULL)
                {
                    continue;
                }
                if(setE)
                {
                    printf("  AddEvent(%d:%d:%s):  ", CURRENT_TIME, CURRENT_RUNNING_PROCESS->pid, "RUNNG");
                    printEventQueue();
                    printf("==>   ");
                }
                
                //create run event
                Event* runEvt = new Event();
                runEvt->proc = CURRENT_RUNNING_PROCESS;
                runEvt->proc->status = READY;
                runEvt->timeStamp = CURRENT_TIME;
                runEvt->transition = TRANS_TO_RUN;
                runEvt->status = RUN;
                runEvt->addIndex = eventCount++;
                put_event(runEvt);
                
                if(setE)
                {
                    printEventQueue();
                    printf("\n");
                }
            }

        }

    }
    
}


//Read the rand numbers from rfile
void readRandVals(char* filePath)
{
    ifstream fin;
    fin.open(filePath, ios::out | ios::in);
    if(!fin.is_open())
    {
        cout << "Fail to open the rfile !" << endl;
        exit(-1);
    }
    char line[100];
    fin.getline(line, 100);
    lineNums = atoi(line);  //indicates the number of random numbers
    int count = 0;
    while(fin.getline(line, 100))
    {
        int randNum = atoi(line);
        randvals[count++] = randNum;
    }
}

//Read input files and initialize processes
void readProcFromFile(char* path)
{
    ifstream fin;
    int pidCount = 0;
    char* curToken;
    char line[100];
    fin.open(path, ios::out | ios::in);
    if(!fin.is_open())
    {
        cout << "Fail to open the file !" << endl;
        exit(-1);
    }
    while(fin.getline(line, 100))
    {
        curToken = strtok(line, " ");
        int cache[4];   //used to cache one line
        int count = 0;
        while(curToken != NULL)
        {
            cache[count++] = atoi(curToken);
            curToken = strtok(NULL, " ");
        }
        
        //initialze process
        //assign the cached values to proc, and add to process queue
        Process* proc = new Process();
        proc->arriveTime = cache[0];
        proc->totalCpuTime = cache[1];
        proc->cpuBurst = cache[2];
        proc->IOBurst = cache[3];
        proc->status = CREATED;
        proc->static_prio = myrandom(maxprio);  //need to change
        proc->prio = proc->static_prio - 1;
        proc->pid = pidCount;
        proc->state_ts = proc->arriveTime;
        proc->rem = proc->totalCpuTime;
        proc->randCB = 0;
        ProcTime* pt = new ProcTime();
        proc->timeInfo = pt;
        proc_queue.push_back(proc);     //add to process queue

//      add to event queue
        Event* evt = new Event();
        evt->timeStamp = proc->arriveTime;
        evt->proc = proc;
        evt->status = READY;
        evt->transition = TRANS_TO_READY;
        evt->addIndex = eventCount++;
        eventQueue.push(evt);
        
//        update process id info
        pidCount++;
    }
}

//Use the enter and exit time to IO to calculate the IO util
double calcuIOUtil()
{
    //the time is added in order, so it is initially sorted according to start time
    //the idea is to merge all the overlapping time period and add them to IOTimeStamp
    if(!IOTimeStamp.empty())
    {
        int start = IOTimeStamp[0].first;
        int end = IOTimeStamp[0].second;
        int count = 1;
        while(count < IOTimeStamp.size())
        {
            pair<int,int> timeItem = IOTimeStamp[count];
            if(end >= timeItem.first)   //judge if two time periods are overlapped
            {
                end = max(end, timeItem.second);
            }else{
                IOUtil += (end - start) * 1.0;
                start = timeItem.first;
                end = timeItem.second;
            }
            count++;
        }
        IOUtil += (end - start);
        IOUtil  = (IOUtil * 100.0) / finTime;
    }
    
    return IOUtil;
}

int main(int argc, char * argv[]) {
    
    string sche_name = "FCFS";
    current_scheduler = new FCFS();
    int op;
    while((op = getopt(argc, argv, "vteps:")) != -1)
    {
        switch(op)
        {
            case 'v':
                setV = 1;
                break;
            case 't':
                break;
            case 'e':
                setE = 1;
                break;
            case 'p':
                break;
            case 's':
            {
                char sche_type = optarg[0];
                //switch the scheduler
                switch(sche_type)
                {
                    case 'F':
                    {
                        quantum = 10000;
                        sche_name = "FCFS";
                        current_scheduler = new FCFS();
                        break;
                    }
                    case 'L':
                    {
                        quantum = 10000;
                        sche_name = "LCFS";
                        current_scheduler = new LCFS();
                        break;
                    }
                    case 'S':
                    {
                        sche_name = "SRTF";
                        quantum = 10000;
                        current_scheduler = new SRTF();
                        break;
                    }
                    case 'R':
                    {
                        sche_name = "RR";
                        sscanf(optarg, "%c%d", &sche_type, &quantum);
                        current_scheduler = new RR();
                        break;
                    }
                    case 'P':
                    {
                        sche_name = "PRIO";
                        sscanf(optarg,"%c %d:%d", &sche_type, &quantum, &maxprio);
                        current_scheduler = new PRIO();
                        break;
                    }
                    case 'E':
                    {
                        sche_name = "PREPRIO";
                        sscanf(optarg, "%c %d:%d", &sche_type, &quantum, &maxprio);
                        current_scheduler = new PREPRIO();
                        break;
                    }
                    default:
                        printf("No scheduler found !\n");
                        exit(-1);
                }
                break;
            }
            case '?':
                break;
        }
    }
    if(setV != 1)
    {
        setE = 0;
    }
    //if exceed the argc, report error
    if(optind >= argc || optind + 1 >= argc)
    {
        printf("Error occurs with input files!\n");
        exit(-1);
    }
    char* inputPath = argv[optind];
    char* rfilePath = argv[optind+1];
    readRandVals(rfilePath);    //read random numbers
    readProcFromFile(inputPath);    //read input files and initialize processes
    if(setE)
    {
        printf("ShowEventQ:  ");
        for(int qi = 0; qi < proc_queue.size(); ++qi)
        {
            Process* p = proc_queue[qi];
            printf("%d:%d  ", p->arriveTime, p->pid);
        }
        printf("\n");
    }
    
    //start simulation
    simulation();

    //print the time info after simulation
    printf("%s",sche_name.c_str());
    if(sche_name == "RR" || sche_name == "PRIO" || sche_name == "PREPRIO")
    {
        printf(" %d", quantum);
    }
    printf("\n");
    for(int i = 0; i < proc_queue.size(); ++i)
    {
        Process* p = proc_queue[i];
        ProcTime* pt = p->timeInfo;
        printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n", p->pid, p->arriveTime, p->totalCpuTime, p->cpuBurst, p->IOBurst, p->static_prio,
               pt->finTime, pt->turnaroundTime, pt->IOTime, pt->waitTime);
        aveTurnaroundTime += pt->turnaroundTime;
        aveCpuWaitTime += pt->waitTime;
    }
    cpuUtil = (cpuUtil * 100.0) / finTime;
    IOUtil = calcuIOUtil();
    
    aveTurnaroundTime = aveTurnaroundTime / proc_queue.size();
    aveCpuWaitTime = aveCpuWaitTime / proc_queue.size();
    throughput = (proc_queue.size() * 100.0) / finTime;
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n", finTime, cpuUtil, IOUtil, aveTurnaroundTime, aveCpuWaitTime, throughput);
    
    
    return 0;
}
