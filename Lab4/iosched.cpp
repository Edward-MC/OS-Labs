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

class IORequest
{
public:
    int op;
    int arriveTime;
    int track;
    int startTime;
    int endTime;
    
    //Summary information
    int turnAroundTime;
    int waitTime;
};

int setV = 0;
int curTime = 0;
int curIOTrack = 0;
IORequest* nextIOTrack = NULL;   //Let NULL represents no IO is active now.
queue<IORequest*> IORequestQueue;
queue<IORequest*> IORequestKeep;   //Keep All IO Requests before delete, used for summary output

//Summary information
int totMovement = 0;
double avgTurnAround = 0.0;
double avgWaitTime = 0.0;
int maxWaitTime = 0;

class SchedAlg
{
public:
    virtual bool checkEmpty() = 0;
    virtual IORequest* getTrack() = 0;
    virtual void addToQueue(IORequest* request) = 0;
    
};

class FLOOK: public SchedAlg
{
public:
    int direction;
    queue<IORequest*> activeQueue;
    queue<IORequest*> addQueue;
    
    FLOOK()
    {
        direction = 0;
    }
    
    bool checkEmpty()
    {
        if (activeQueue.empty() && addQueue.empty()) {
            return true;
        }else{
            return false;
        }
    }
    
    void addToQueue(IORequest* request)
    {
        addQueue.push(request);
    }
    
    bool checkIfTransDirect()
    {
        //When goes up, if we find any track >= current track, return false
        //When goes down, if we find any track <= current track, return false
        //In other cases: return true
        size_t size = activeQueue.size();
        for(int i = 0; i < size; ++i)
        {
            IORequest* front = activeQueue.front();
            activeQueue.pop();
            activeQueue.push(front);
            if(direction == 0 && front->track >= curIOTrack)
            {
                return false;
            }else if(direction == 1 && front->track <= curIOTrack)
            {
                return false;
            }
        }
        return true;
    }
    
    //Totally the same with LOOK
    //But deal with active queue and add queue
    //Before do the pick up
    IORequest* getTrack()
    {
        if (checkEmpty()) {
            return NULL;
        }
        
        if (activeQueue.empty())
        {
            swap(activeQueue, addQueue);
        }
            
            
        //Check if we need to change direction
        if(checkIfTransDirect())
        {
            direction = 1 - direction;  //change direction
        }
        
        IORequest* front = activeQueue.front();
        activeQueue.pop();
        
        //Have to keep the init to satisfy the condition
        //When goes up, the init should > current track
        //When goes down, the init should < current track
        while(((direction == 0) && (front->track < curIOTrack)) || ((direction == 1) && (front->track > curIOTrack)))
        {
            activeQueue.push(front);
            front = activeQueue.front();
            activeQueue.pop();
        }
        
        int minDist = abs(front->track - curIOTrack);
        IORequest* request = front;
        size_t size = activeQueue.size();
        
        //put the front to the back if it is not min.
        //if it is the smaller than the hold one, push the hold one back, keep the smaller one
        //This can make sure we find the min with one travesal
        for(int i = 0; i < size; ++i)
        {
            front = activeQueue.front();
            activeQueue.pop();
            
            //Exclude the requests that do not satisfy the requirement
            bool upUnsatisfy = direction == 0 && front->track < curIOTrack;
            bool downUnatisfy = direction == 1 && front->track > curIOTrack;
            if (upUnsatisfy || downUnatisfy)
            {
                activeQueue.push(front);
                continue;
            }
            
            //begin to compare
            int dist = direction == 0 ? front->track - curIOTrack : curIOTrack - front->track;
            if ((dist < minDist) || (dist == minDist && (request->op > front->op)))
            {
                //put the old one back to queue and update
                activeQueue.push(request);
                minDist = dist;
                request = front;
            }else{
                //Directly put the one in the back
                activeQueue.push(front);
            }
        }
        return request;
    }
};

class CLOOK: public SchedAlg
{
public:
    queue<IORequest*> IOQueue;
    
    bool checkEmpty()
    {
        return IOQueue.empty();
    }
    
    void addToQueue(IORequest* request)
    {
        IOQueue.push(request);
    }
    
    bool checkIfTransDirect()
    {
        //When goes up, if we find any track >= current track, return false
        //When goes down, if we find any track <= current track, return false
        //In other cases: return true
        size_t size = IOQueue.size();
        for(int i = 0; i < size; ++i)
        {
            IORequest* front = IOQueue.front();
            IOQueue.pop();
            IOQueue.push(front);
            if(front->track >= curIOTrack)
            {
                return false;
            }
        }
        return true;
    }
    
    //If goes up to the last request
    //Call this algorithm to go back to the minimal
    IORequest* getMinTrack()
    {
        if(!IOQueue.empty())
        {
            IORequest* request = IOQueue.front();
            IOQueue.pop();
            size_t size = IOQueue.size();   //must get after taking one value
            
            for (int i = 0; i < size; ++i) {
                IORequest* front = IOQueue.front();
                IOQueue.pop();
                
                if(front->track < request->track)
                {
                    IOQueue.push(request);
                    request = front;
                }else{
                    IOQueue.push(front);
                }
            }
            return request;
        }else{
            return NULL;
        }
    }
    
    IORequest* getTrack()
    {
        if(!IOQueue.empty())
        {
            if(checkIfTransDirect())
            {
                return getMinTrack();
            }
            
            //Find a valid init value, where init->track > current track
            IORequest* request = IOQueue.front();
            IOQueue.pop();
            while(request->track < curIOTrack)
            {
                IOQueue.push(request);
                request = IOQueue.front();
                IOQueue.pop();
            }
            int minDist = abs(request->track - curIOTrack);
            
            //put the front to the back if it is not min.
            //if it is the smaller than the hold one, push the hold one back, keep the smaller one
            //This can make sure we find the min with one travesal
            size_t size = IOQueue.size();
            for(int i = 0; i < size; ++i)
            {
                IORequest* front = IOQueue.front();
                IOQueue.pop();
                
                //Check if valid
                if(front->track < curIOTrack)
                {
                    IOQueue.push(front);
                    front = IOQueue.front();
                    continue;
                }
                
                int dist = abs(curIOTrack - front->track);
                if ((dist < minDist) || (dist == minDist && (request->op > front->op)))
                {
                    //put the old one back to queue and update
                    IOQueue.push(request);
                    minDist = dist;
                    request = front;
                }else{
                    //Directly put the one in the back
                    IOQueue.push(front);
                }
            }
            return request;
        }else{
            return NULL;
        }
    }
};

class LOOK: public SchedAlg
{
public:
    queue<IORequest*> IOQueue;
    bool direction;     //0 goes up, and 1 goes down.
    
    LOOK()
    {
        direction = 0;  //In default, we go up.
    }
    
    bool checkEmpty()
    {
        return IOQueue.empty();
    }
    
    void addToQueue(IORequest* request)
    {
        IOQueue.push(request);
    }
    
    bool checkIfTransDirect()
    {
        //When goes up, if we find any track >= current track, return false
        //When goes down, if we find any track <= current track, return false
        //In other cases: return true
        size_t size = IOQueue.size();
        for(int i = 0; i < size; ++i)
        {
            IORequest* front = IOQueue.front();
            IOQueue.pop();
            IOQueue.push(front);
            if(direction == 0 && front->track >= curIOTrack)
            {
                return false;
            }else if(direction == 1 && front->track <= curIOTrack)
            {
                return false;
            }
        }
        return true;
    }
    
    IORequest* getTrack()
    {
        if (!IOQueue.empty()) {
            //Check if we need to change direction
            if(checkIfTransDirect())
            {
                direction = 1 - direction;  //change direction
            }
            
            IORequest* front = IOQueue.front();
            IOQueue.pop();
            
            //Have to keep the init to satisfy the condition
            //When goes up, the init should > current track
            //When goes down, the init should < current track
            while(((direction == 0) && (front->track < curIOTrack)) || ((direction == 1) && (front->track > curIOTrack)))
            {
                IOQueue.push(front);
                front = IOQueue.front();
                IOQueue.pop();
            }
            
            int minDist = abs(front->track - curIOTrack);
            IORequest* request = front;
            size_t size = IOQueue.size();
            
            //put the front to the back if it is not min.
            //if it is the smaller than the hold one, push the hold one back, keep the smaller one
            //This can make sure we find the min with one travesal
            for(int i = 0; i < size; ++i)
            {
                front = IOQueue.front();
                IOQueue.pop();
                
                //Exclude the requests that do not satisfy the requirement
                bool upUnsatisfy = direction == 0 && front->track < curIOTrack;
                bool downUnatisfy = direction == 1 && front->track > curIOTrack;
                if (upUnsatisfy || downUnatisfy)
                {
                    IOQueue.push(front);
                    continue;
                }
                
                //begin to compare
                int dist = direction == 0 ? front->track - curIOTrack : curIOTrack - front->track;
                if ((dist < minDist) || (dist == minDist && (request->op > front->op)))
                {
                    //put the old one back to queue and update
                    IOQueue.push(request);
                    minDist = dist;
                    request = front;
                }else{
                    //Directly put the one in the back
                    IOQueue.push(front);
                }
            }
            return request;
            
        }
        
        return NULL;
        
    }
    
};


class SSTF: public SchedAlg
{
public:
    queue<IORequest*> IOQueue;
    
    bool checkEmpty()
    {
        return IOQueue.empty();
    }
    
    IORequest* getTrack()
    {
        if (!IOQueue.empty()) {
            IORequest* front = IOQueue.front();
            IOQueue.pop();
            
            int minDist = abs(front->track - curIOTrack);
            IORequest* request = front;
            size_t size = IOQueue.size();
            
            //put the front to the back if it is not min.
            //if it is the smaller than the hold one, push the hold one back, keep the smaller one
            //This can make sure we find the min with one travesal
            for(int i = 0; i < size; ++i)
            {
                front = IOQueue.front();
                IOQueue.pop();
                int dist = abs(front->track - curIOTrack);
                if ((dist < minDist) || (dist == minDist && (request->op > front->op)))
                {
                    //put the old one back to queue and update
                    IOQueue.push(request);
                    minDist = dist;
                    request = front;
                }else{
                    //Directly put the one in the back
                    IOQueue.push(front);
                }
            }
            return request;
        }
        return NULL;
    }
    
    void addToQueue(IORequest* request)
    {
        IOQueue.push(request);
    }
};

class FIFO: public SchedAlg
{
public:
    queue<IORequest*> IOQueue;
    
    void addToQueue(IORequest* request)
    {
        IOQueue.push(request);
    }
    
    bool checkEmpty()
    {
        return IOQueue.empty();
    }
    
    IORequest* getTrack()
    {
        if(!IOQueue.empty())
        {
            IORequest* request = IOQueue.front();
            IOQueue.pop();
            return request;
        }
        return NULL;  //Let NULL represents the queue is empty
    }
};

SchedAlg* sched;

//Peek the next arrive time
int getNextIOTime()
{
    if(!IORequestQueue.empty())
    {
        IORequest* front = IORequestQueue.front();
        return front->arriveTime;
    }
    return -1;  //Let -1 represents the request IO queue is empty
}

//Get the next request from the front
IORequest* getNextIORequest()
{
    if(!IORequestQueue.empty())
    {
        IORequest* front = IORequestQueue.front();
        IORequestQueue.pop();
        return front;
    }
    return NULL;  //Let NULL represents the request IO queue is empty
}


void Simulation()
{
    while(true)
    {
        //New IO arrives at this time
        if(curTime == getNextIOTime())
        {
            IORequest* request = getNextIORequest();
            sched->addToQueue(request);
            if(setV)
            {
                printf("%d:    %d add %d\n", curTime, request->op, request->track);
            }
        }
        
        //An IO is active, and completed at this time
        if(nextIOTrack != NULL && curIOTrack == nextIOTrack->track)
        {
            nextIOTrack->endTime = curTime;
            nextIOTrack->turnAroundTime = curTime - nextIOTrack->arriveTime;
            if(setV)
            {
                printf("%d:    %d finish %d\n", curTime, nextIOTrack->op, nextIOTrack->turnAroundTime);
            }
            nextIOTrack = NULL;
        }
        
        //No IO is active now
        if(nextIOTrack == NULL)
        {
            if (sched->checkEmpty() && IORequestQueue.empty()) {  //The queue is empty, should exit
                break;
            }else if(sched->checkEmpty() && !IORequestQueue.empty()){
                curTime++;
                continue;
            }else{
                nextIOTrack = sched->getTrack();
                
                if(setV)
                {
                    printf("%d:    %d issue %d %d\n", curTime, nextIOTrack->op, nextIOTrack->track, curIOTrack);
                }
                
                //Update the start time
                nextIOTrack->startTime = curTime;
                
                //Update the wait time
                nextIOTrack->waitTime += curTime - nextIOTrack->arriveTime;
                
                //Deal with the condition that once issued, it will finish right away
                if(nextIOTrack->track == curIOTrack)    continue;
            }
        }
        
        //An IO is active
        if(nextIOTrack != NULL && (nextIOTrack->track != curIOTrack))
        {
            if(curIOTrack < nextIOTrack->track)
                curIOTrack++;
            else
                curIOTrack--;
            
            //Update movement
            totMovement++;
        }
        
        //Increase the time by 1
        curTime++;
    }
}


void readFromInput(char* path)
{
    ifstream fin;
    fin.open(path, ios::out | ios::in);
    if(!fin.is_open())
    {
        cout << "Fail to open the input file !" << endl;
        exit(-1);
    }
    
    char line[100];
    //skip #
    int count = 0;
    while(fin.getline(line, 100))
    {
        if (line[0] == '#')
        {
            continue;
        }else{
            int time;
            int track;
            sscanf(line, "%d %d", &time, &track);
            IORequest* request = new IORequest();
            request->op = count++;
            request->arriveTime = time;
            request->track = track;
            request->startTime = 0;
            request->endTime = 0;
            request->turnAroundTime = 0;
            request->waitTime = 0;
            IORequestQueue.push(request);
        }
    }
    
    //Helps to make a copy of IORquests
    size_t size = IORequestQueue.size();
    for(int i = 0; i < size; ++i)
    {
        IORequest* front = IORequestQueue.front();
        IORequestQueue.pop();
        IORequestQueue.push(front);
        IORequestKeep.push(front);
    }
    
    fin.close();
    
}

int main(int argc, char * argv[]) {
    
    //Set default
    setV = 0;
    sched = new FIFO();
    
    int op;
    while((op = getopt(argc, argv, "s:v")) != -1)
    {
        switch (op) {
            case 'v':
                setV = 1;
                break;
            case 's':
            {
                char alg = optarg[0];
                switch (alg) {
                    case 'i':
                    {
                        sched = new FIFO();
                        break;
                    }
                    case 'j':
                    {
                        sched = new SSTF();
                        break;
                    }
                    case 's':
                    {
                        sched = new LOOK();
                        break;
                    }
                    case 'c':
                    {
                        sched = new CLOOK();
                        break;
                    }
                    case 'f':
                    {
                        sched = new FLOOK();
                        break;
                    }
                    default:
                        break;
                }
                break;
            }
            case '?':
            {
                printf("The input contains error: %c !!!\n", optopt);
                exit(-1);
            }
            default:
                break;
        }
    }
    
    //if exceed the argc, report error
    if(optind >= argc)
    {
        printf("Error occurs with input files!\n");
        exit(-1);
    }
    char* inputPath = argv[optind];
    
    //read from input
    readFromInput(inputPath);
    
    Simulation();
    
    
    //Summary Information Print
    int count = 0;  //Used for count the number of IO reuqests
    while (!IORequestKeep.empty()) {
        IORequest* request = IORequestKeep.front();
        
        //Gather Summary Information
        count++;
        avgTurnAround += request->turnAroundTime;
        avgWaitTime += request->waitTime;
        maxWaitTime = max(maxWaitTime, request->waitTime);
        
        IORequestKeep.pop();
        printf("%5d: %5d %5d %5d\n", request->op, request->arriveTime, request->startTime, request->endTime);
    }
    
    avgTurnAround = avgTurnAround / (count * 1.0);
    avgWaitTime = avgWaitTime / (count * 1.0);
    
    printf("SUM: %d %d %.2lf %.2lf %d\n",
    curTime, totMovement, avgTurnAround, avgWaitTime, maxWaitTime);
    
    
    return 0;
}
