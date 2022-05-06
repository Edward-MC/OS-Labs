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

#define MAX_VPAGES 64
#define MAX_FRAMES 128


typedef struct {
    unsigned valid: 1;
    unsigned referenced: 1;
    unsigned modified: 1;
    unsigned writeProtect: 1;
    unsigned pageout: 1;
    unsigned frameNum: 7;
    unsigned fileMapped: 1;
//    unsigned ifVma: 1;  //check if vpage is in vma
    unsigned rest: 19;
} pte_t;


typedef struct {
    int pid;
    int vpage;
    int ifMapped;
    int frameIndex;
    unsigned int age: 32;
    unsigned long lastUse;
} frame_t;

typedef struct {
    int start;
    int end;
    int write_protected;
    int file_mapped;
} vma;

//Summary info
class Pstats{
public:
    unsigned long unmaps;
    unsigned long maps;
    unsigned long ins;
    unsigned long outs;
    unsigned long fins;
    unsigned long fouts;
    unsigned long zeros;
    unsigned long segv;
    unsigned long segprot;
    
    Pstats()
    {
        unmaps = 0;
        maps = 0;
        ins = 0;
        outs = 0;
        fins = 0;
        fouts = 0;
        zeros = 0;
        segv = 0;
        segprot = 0;
    }
};

class Process{
public:
    int pid;
    vector<vma> vmaList;
    pte_t page_table[MAX_VPAGES];
    Pstats* pstats;
    
    Process()
    {
        for(int p = 0; p < MAX_VPAGES; ++p)
        {
            pte_t* pte = &this->page_table[p];
            pte->valid = 0;
            pte->referenced = 0;
            pte->modified = 0;
            pte->writeProtect = 0;
            pte->pageout = 0;
            pte->pageout = 0;
            pte->frameNum = 0;
//            pte->ifVma = 0;
            pte->fileMapped = 0;
        }
        this->pstats = new Pstats();
    }
};

vector<Process> procList;
queue<pair<char, unsigned long> > instrList;
queue<int> freePool;
int frameSize = 16;
frame_t* frameTable;
Process* currentProcess;
unsigned long instCount = 0;
int setO, setP, setF, setS = 0;

//Used for summary info
unsigned long ctxSwithes = 0;
unsigned long processExits = 0;
unsigned long long cost = 0;

unsigned long instAfterReset = 0;   //record the inst number after reset in NRU
const int resetNum = 50;    //instruction number needed for reset

//For Random Alg
int ofs = 0;
vector<int> randvals;



class Pager {
public:
    virtual frame_t* select_victim_frame() = 0; // virtual base class
    virtual void resetAgeValue(frame_t* frame) = 0;   //reset the value of age to 0
};

class FIFO: public Pager{
public:
    int startIndex;
    
    FIFO(){
        this->startIndex = 0;
    }
    
    frame_t* select_victim_frame()
    {
        frame_t* selectedFrame = &frameTable[startIndex];
        this->startIndex++;
        this->startIndex = this->startIndex % frameSize;
        return selectedFrame;
    }
    
    void resetAgeValue(frame_t* frame)
    {
        //leave empty
    }
};

int myrandom(int frameSize)
{
    int rand = (randvals[ofs] % frameSize);
    ofs = (ofs + 1) % randvals.size();   //if reach the end, come again from the start
    return rand;
}

class Random: public Pager{
public:
    frame_t* select_victim_frame()
    {
        int rand = myrandom(frameSize);
        frame_t* selectedFrame = &frameTable[rand];
        return selectedFrame;
    }
    
    void resetAgeValue(frame_t* frame)
    {
        //leave empty
    }
};

class Clock: public Pager{
public:
    int startIndex;
    int roundSize;
    
    Clock()
    {
        startIndex = 0;
        roundSize = frameSize;
    }
    
    frame_t* select_victim_frame()
    {
        bool ifFound = false;   //control the loop to avoid while(true)
        int roundDist = 0; //make a copy with the hand so that we can loop later
        frame_t* selectedFrame = &frameTable[startIndex];
        while(!ifFound)
        {
            selectedFrame = &frameTable[(startIndex+roundDist) % frameSize];
            roundDist++;
            roundDist = roundDist % roundSize;
            
            
            pte_t* pte = &procList[selectedFrame->pid].page_table[selectedFrame->vpage];
            if(pte->referenced)
            {
                pte->referenced = 0;
            }else{
                ifFound = true;
                startIndex = (startIndex + roundDist) % frameSize;
            }
        }
        return selectedFrame;
    }
    
    void resetAgeValue(frame_t* frame)
    {
        //leave empty
    }
};

class NRU: public Pager{
public:
    int startIndex;
    
    NRU()
    {
        startIndex = 0;
    }
    
    frame_t* select_victim_frame()
    {
        bool ifFound = false;   //control the loop to avoid while(true)
        frame_t* classList[4];
        
        //Init the classList so that we can tell what we get or not for each class.
        for(int i = 0; i < 4; ++i)
        {
            classList[i] = NULL;
        }
        
        frame_t* selectedFrame = NULL;
        
        for(int i = 0; i < frameSize; ++i)
        {
            frame_t* frame = &frameTable[startIndex];
            startIndex = (startIndex + 1) % frameSize;
            pte_t* pte = &procList[frame->pid].page_table[frame->vpage];
            int classNum = pte->referenced * 2 + pte->modified;
            
            //Set only the first found one
            if(classNum == 0 && !ifFound)
            {
                classList[classNum] = frame;
                if(instAfterReset < resetNum) break;
                else{
                    ifFound = true;
                    continue;
                }
            }
            if(classList[classNum] == NULL && !ifFound)
            {
                classList[classNum] = frame;
            }
            
            if(instAfterReset >= resetNum)
            {
                pte->referenced = 0;
            }
        }
        
        //Return the first one with the least class number
        for(int i = 0; i < 4; ++i)
        {
            if(classList[i] != NULL)
            {
                startIndex = (classList[i]->frameIndex + 1) % frameSize;
                selectedFrame = classList[i];
                break;
            }
        }
        
        //update instruction number finally
        if(instAfterReset >= resetNum)
        {
            instAfterReset = 0;
        }
            
        
        return selectedFrame;
    }
    
    void resetAgeValue(frame_t* frame)
    {
        //leave empty
    }
};

class Aging: public Pager
{
public:
    int startIndex;
    
    Aging()
    {
        startIndex = 0;
    }
    
    void resetAgeValue(frame_t* frame)
    {
        frame->age = 0;
    }
    
    frame_t* select_victim_frame()
    {
        frame_t* selectedFrame = &frameTable[startIndex];
        startIndex = (startIndex + 1) % frameSize;
        unsigned int minAge = selectedFrame->age >> 1;
        if(procList[selectedFrame->pid].page_table[selectedFrame->vpage].referenced)
        {
            minAge = selectedFrame->age | 0x80000000;
        }
        
        //age operation for every frame
        for(int i = 0; i < frameSize; ++i)
        {
            frame_t* frame = &frameTable[(startIndex + i) % frameSize];
            pte_t* pte = &procList[frame->pid].page_table[frame->vpage];
            frame->age = frame->age >> 1;
            if(pte->referenced == 1)
            {
                frame->age = frame->age | 0x80000000;
                pte->referenced = 0;    //reset r bit
            }
            
            //compare and get the frame with minimal age
            if (frame->age < minAge) {
                minAge = frame->age;
                selectedFrame = frame;
            }
        }
        startIndex = (selectedFrame->frameIndex + 1) % frameSize;
        return selectedFrame;
    }
};

class WorkingSet: public Pager
{
public:
    int startIndex;
    int tau;
    
    WorkingSet()
    {
        startIndex = 0;
        tau = 50;
    }
    
    frame_t* select_victim_frame()
    {
        frame_t* selectedFrame = NULL;
        
        //Give the initial value to oldest frame and timeDiff, which can make sure they get the correct value
        frame_t* oldestFrame = &frameTable[startIndex];
        //For timeDiff, since we cannot init it to -1, and it have to compare to 0, so we can set it to the value of startIndex
        unsigned long timeDiff = procList[oldestFrame->pid].page_table[oldestFrame->vpage].referenced == 1 ? 0 : instCount - frameTable[startIndex].lastUse;
        
        for(int i = 0; i < frameSize; ++i)
        {
            frame_t* frame = &frameTable[(startIndex + i) % frameSize];
            pte_t* pte = &procList[frame->pid].page_table[frame->vpage];
            
            if(pte->referenced == 1)
            {
                pte->referenced = 0;
                frame->lastUse = instCount;
            }
            //check if there is a frame that satisfy the time condition
            if(instCount - frame->lastUse >= tau && pte->referenced == 0)
            {
                selectedFrame = frame;
                startIndex = (selectedFrame->frameIndex + 1) % frameSize;
                break;
            }
            //record the oldest frame
            if (timeDiff < instCount - frame->lastUse) {
                timeDiff = instCount - frame->lastUse;
                oldestFrame = frame;
            }
//            cout << "frame index:   " << frame->frameIndex << "  "<< "frame last use:  " << frame->lastUse << "  timeDiff:  " << timeDiff << endl;
        }
        
        if(selectedFrame == NULL)
        {
            selectedFrame = oldestFrame;
            startIndex = (selectedFrame->frameIndex + 1) % frameSize;
        }
        
        return selectedFrame;
    }
    
    void resetAgeValue(frame_t* frame)
    {
        //leave empty
    }
};

Pager* ThePager;


void readRandomFile(char* path)
{
    ifstream fin;
    fin.open(path, ios::out|ios::in);
    if(!fin.is_open())
    {
        cout << "Fail to open the input file !" << endl;
        exit(-1);
    }
    int randSize;
    fin >> randSize;
    for (int i = 0; i < randSize; ++i) {
        int randNum;
        fin >> randNum;
        randvals.push_back(randNum);
    }
    fin.close();
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
    while(fin.getline(line, 100))
    {
        if (line[0] != '#') break;
    }
    
    //Process Number
    int processNum = atoi(line);
    for(int p = 0; p < processNum; ++p)
    {
        Process proc;
        proc.pid = p;
        
        //VMA Number
        while(fin.getline(line, 100))
        {
            if (line[0] != '#') break;
        }

        int vmaNum = atoi(line);
        for(int v = 0; v < vmaNum; ++v)
        {
            int start, end, writeProtected, fileMapped;
            fin.getline(line, 100);
            sscanf(line, "%d %d %d %d", &start, &end, &writeProtected, &fileMapped);
            
            vma vmaInst;
            vmaInst.start = start;
            vmaInst.end = end;
            vmaInst.write_protected = writeProtected;
            vmaInst.file_mapped = fileMapped;
            proc.vmaList.push_back(vmaInst);
            

        }
        procList.push_back(proc);
    }
    
    
    //Instructions
    while(fin.getline(line, 100))
    {
        if (line[0] == '#') continue;
        char instType;
        int instNum;
        sscanf(line, "%c %d", &instType, &instNum);
        instrList.push(make_pair(instType, instNum));
    }
    
    fin.close();
    
}

void initFreePool(int frameSize)
{
    for(int i = 0; i < frameSize; ++i)
    {
        freePool.push(i);
    }
}

void initFrameTableIndex(int frameSize)
{
    for(int i = 0; i < frameSize; ++i)
    {
        frameTable[i].frameIndex = i;
        frameTable[i].pid = 0;
        frameTable[i].vpage = 0;
        frameTable[i].ifMapped = 0;
        frameTable[i].age = 0;
        frameTable[i].lastUse = 0;
    }
}

frame_t* allocFromFreePool()
{
    if(!freePool.empty())
    {
        int frameIndex = freePool.front();
        freePool.pop();
        return &frameTable[frameIndex];
    }else{
        return NULL;
    }
}

frame_t* get_frame()
{
    frame_t *frame = allocFromFreePool();
    if (frame == NULL)
    {
        frame = ThePager->select_victim_frame();
    }
    return frame;
}

bool getNextInstruction(char& operation, int& vpage)
{
    if (!instrList.empty())
    {
        pair<char, int> inst = instrList.front();
        instrList.pop();
        operation = inst.first;
        vpage = inst.second;
        return true;
    }else{
        return false;
    }
}

int getCurrentVma(int vpage)
{
    for(int i = 0; i < currentProcess->vmaList.size(); ++i)
    {
        int left = currentProcess->vmaList[i].start;
        int right = currentProcess->vmaList[i].end;
        if(vpage >= left && vpage <= right)
        {
            return i;
        }
    }
    return -1;
}

void Simulation()
{
    char operation;
    int vpage;
    while (getNextInstruction(operation, vpage)) {
        
        //Add inst number for NRU algorithm
        instAfterReset++;
        
        if(setO)
        {
            printf("%lu: ==> %c %d\n", instCount, operation, vpage);
        }

        //Update inst number
        instCount++;

        // handle special case of “c” and “e” instruction
        if(operation == 'c')
        {
            currentProcess = &procList[vpage];  //change the current running process
            if (setS)
            {
                ctxSwithes++;   //update the context switches
            }
            continue;
        }
        
        //deal with e inst
        if(operation == 'e')
        {
            //Exit Process
            printf("EXIT current process %d\n", vpage);
            
            Process* proc = &procList[vpage];
            for (int p = 0; p < MAX_VPAGES; ++p)
            {
                pte_t* pte = &proc->page_table[p];
                if(pte->valid)
                {
                    
                    //UNMAP
                    printf(" UNMAP %d:%d\n", vpage, p);
                    
                    //Clear Frame State
                    frame_t* frame = &frameTable[pte->frameNum];
                    frame->pid = 0;
                    frame->vpage = 0;
                    frame->ifMapped = 0;
                    ThePager->resetAgeValue(frame);
                    
                    //Add to Free Pool
                    freePool.push(frame->frameIndex);
                    
                    if (setS) {
                        proc->pstats->unmaps++;
                    }
                    
                    //FOUT
                    if(pte->modified && pte->fileMapped)
                    {
                        printf(" FOUT\n");
                        if (setS) {
                            proc->pstats->fouts++;
                        }
                    }
                }
                //Clear Vpage State
                pte->valid = 0;
                pte->referenced = 0;
                pte->modified = 0;
                pte->pageout = 0;
                pte->frameNum = 0;
                
            }
            if (setS)
            {
                processExits++; //update the process exits
            }
            continue;
        }
        
        if(setS)
        {
            if(operation == 'r' || operation == 'w')
            {
                cost += 1;  //add the cost of read and write
            }
        }
        
        // now the real instructions for read and write
        pte_t *pte = &currentProcess->page_table[vpage];
        
        //Deal with Page Fault
        if (!pte->valid)
        {
        // this in reality generates the page fault exception and now you execute
        // verify this is actually a valid page in a vma if not raise error and next inst
            int vmaIndex;
            if((vmaIndex = getCurrentVma(vpage)) == -1)
            {
                printf(" SEGV\n");
                if(setS)
                {
                    currentProcess->pstats->segv++;
                }
                continue;
            }
            vma* currentVma = &currentProcess->vmaList[vmaIndex];
            pte->writeProtect = currentVma->write_protected;
            pte->fileMapped = currentVma->file_mapped;
            
            frame_t* newFrame = get_frame();
    
            
            //If the new frame is already mapped by vpage
            if (newFrame->ifMapped) {
                //deal with old frame
                pte_t* oldPte = &procList[newFrame->pid].page_table[newFrame->vpage];
                
                //UMMap
                if(setO)
                {
                    printf(" UNMAP %d:%d\n", newFrame->pid, newFrame->vpage);
                }
                if(setS)
                {
                    Process* unmapProc = &procList[newFrame->pid];
                    unmapProc->pstats->unmaps++;
                }
                
                //OUT/FOUT
                if(oldPte->modified)
                {
                    if(oldPte->fileMapped)
                    {
                        printf(" FOUT\n");
                        if(setS)
                        {
                            Process* foutProc = &procList[newFrame->pid];
                            foutProc->pstats->fouts++;
                        }
                    }else{
                        oldPte->pageout = 1;
                        printf(" OUT\n");
                        if(setS)
                        {
                            Process* outProc = &procList[newFrame->pid];
                            outProc->pstats->outs++;
                        }
                    }
                }
                
                //clear old page state
                oldPte->valid = 0;
                oldPte->frameNum = 0;
                oldPte->referenced = 0;
                oldPte->modified = 0;
            }
            
            //IN/FIN/ZERO
            if (pte->pageout)
            {
                printf(" IN\n");
                if(setS)
                {
                    currentProcess->pstats->ins++;
                }
            }else if(pte->fileMapped)
            {
                printf(" FIN\n");
                if(setS)
                {
                    currentProcess->pstats->fins++;
                }
            }else{
                printf(" ZERO\n");
                if(setS)
                {
                    currentProcess->pstats->zeros++;
                }
            }
            
            //MAP
            pte->frameNum = newFrame->frameIndex;
            pte->valid = 1;
            if(setO)
            {
                printf(" MAP %d\n", newFrame->frameIndex);
            }
            if(setS)
            {
                currentProcess->pstats->maps++;
            }
            
            //Update Frame Table
            frameTable[newFrame->frameIndex].vpage = vpage;
            frameTable[newFrame->frameIndex].ifMapped = 1;
            frameTable[newFrame->frameIndex].pid = currentProcess->pid;
            frameTable[newFrame->frameIndex].lastUse = instCount;
            ThePager->resetAgeValue(newFrame);
            
        }

        //have to set reference bit for both r and w
        pte->referenced = 1;
        //deal with case w, and can be modified
        if(operation == 'w')
        {
            if(pte->writeProtect)
            {
                printf(" SEGPROT\n");
                if(setS)
                {
                    currentProcess->pstats->segprot++;
                }
            }else{
                pte->modified = 1;
            }
        }
    }
}



int main(int argc, char * argv[]) {

    // set default value
    frameSize = 16;
    
    setO = 0;
    setP = 0;
    setF = 0;
    setS = 0;
    
    
    ThePager = new FIFO();
    int op;
    while((op = getopt(argc, argv, "f:a:o:")) != -1)
    {
        switch (op) {
            case 'f':
            {
                frameSize = atoi(optarg);
                break;
            }
            case 'a':
            {
                char alg = optarg[0];
                switch (alg) {
                    case 'f':
                        ThePager = new FIFO();
                        break;
                    case 'r':
                        ThePager = new Random();
                        break;
                    case 'c':
                        ThePager = new Clock();
                        break;
                    case 'e':
                        ThePager = new NRU();
                        break;
                    case 'a':
                        ThePager = new Aging();
                        break;
                    case 'w':
                        ThePager = new WorkingSet();
                        break;
                    default:
                        break;
                }
                break;
            }
            case 'o':
            {
                string debugOpt = optarg;
                for (int i = 0; i < debugOpt.size(); ++i)
                {
                    if(debugOpt[i] == 'O') setO = 1;
                    if(debugOpt[i] == 'P') setP = 1;
                    if(debugOpt[i] == 'F') setF = 1;
                    if(debugOpt[i] == 'S') setS = 1;
                }
                break;
            }
            case '?':
            {
                printf("The input contains error: %c !!!\n", optopt);
                exit(-1);
            }
                
        }
    }
    
    //if exceed the argc, report error
    if(optind >= argc || optind + 1 >= argc)
    {
        printf("Error occurs with input files!\n");
        exit(-1);
    }
    char* inputPath = argv[optind];
    char* randFilePath = argv[optind+1];
    
    //read from files
    readFromInput(inputPath);
    readRandomFile(randFilePath);
    
    //alloc the memory of frameTable
    frameTable = new frame_t[frameSize];
    
    //Init Free Pool
     initFreePool(frameSize);
     
     //Init FrameTable index
     initFrameTableIndex(frameSize);
    
    Simulation();
    
    if(setP)
    {
        for(int i = 0; i < procList.size(); ++i)
        {
            Process proc = procList[i];
            printf("PT[%d]:", i);
            for(int vp = 0; vp < MAX_VPAGES; ++vp)
            {
                if(proc.page_table[vp].valid)
                {
                    char R = proc.page_table[vp].referenced == 1 ? 'R' : '-';
                    char M = proc.page_table[vp].modified == 1 ? 'M' : '-';
                    char S = proc.page_table[vp].pageout == 1 ? 'S' : '-';
                    printf(" %d:%c%c%c", vp, R, M, S);
                }else if(!proc.page_table[vp].valid && proc.page_table[vp].pageout)
                {
                    printf(" #");
                }else{
                    printf(" *");
                }
                
            }
            printf("\n");
        }
    }
    
    if(setF)
    {
        printf("FT:");
        for(int i = 0; i < frameSize; ++i)
        {
            frame_t frame = frameTable[i];
            if(!frame.ifMapped)
            {
                printf(" *");
            }else{
                printf(" %d:%d", frame.pid, frame.vpage);
            }
        }
        printf("\n");
    }
    
    if (setS) {
        //calculate the cost of each process
        for(int i = 0;  i < procList.size(); ++i)
        {
            Process* cur = &procList[i];
            cost += cur->pstats->maps * 300;
            cost += cur->pstats->unmaps * 400;
            cost += cur->pstats->ins * 3100;
            cost += cur->pstats->outs * 2700;
            cost += cur->pstats->fins * 2800;
            cost += cur->pstats->fouts * 2400;
            cost += cur->pstats->zeros * 140;
            cost += cur->pstats->segv * 340;
            cost += cur->pstats->segprot * 420;
        }
        
        //calculate the cost of total
        cost += ctxSwithes * 130;
        cost += processExits * 1250;
        
        //Per Process Output
        for (int i = 0; i < procList.size(); i++)
        {
            Process* proc = &procList[i];
            Pstats* pstats = proc->pstats;
            
            printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
                                 proc->pid,
                                 pstats->unmaps, pstats->maps, pstats->ins, pstats->outs,
                                 pstats->fins, pstats->fouts, pstats->zeros,
                                 pstats->segv, pstats->segprot);
        }
        
        //Summary Output
        printf("TOTALCOST %lu %lu %lu %llu %lu\n",
                      instCount, ctxSwithes, processExits, cost, sizeof(pte_t));
        
    }
    
    return 0;
}
