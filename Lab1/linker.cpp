//
//  tokenizer.cpp
//  Lab
//
//  Created by Meng Chen on 2/6/22.
//

/*****************************************************************************
 For the comment, generally if the comment is in the right of the code,
 it is comment on the current line of code, but if it is on the top of the
 code, it should comment on the next several lines.
 *****************************************************************************/




#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include <sstream>
#include <unordered_map>
#include <string.h>
#include <stdlib.h>
//#include <bits/stdc++.h>
using namespace std;

//Error code defination
#define NUM_EXPECTED 0
#define SYM_EXPECTED 1
#define ADDR_EXPECTED 2
#define SYM_TOO_LONG 3
#define TOO_MANY_DEF_IN_MODULE 4
#define TOO_MANY_USE_IN_MODULE 5
#define TOO_MANY_INSTR 6

const int maxLineNum = 100000;
int lineNum = 0;
int offset = 0;
char line[maxLineNum];
char* curToken = NULL;
char* beginLine = NULL;
size_t endOfLine = 0;
ifstream fin;

//Symbol table in Pass 1
vector<pair<string, int>> symboltable;



void __parseerror(int errcode) {
    static vector<string> errstr = {
        "NUM_EXPECTED",                             // Number expect, anything >= 2^30 is not a number either
        "SYM_EXPECTED",                             // Symbol Expected
        "ADDR_EXPECTED",                            // Addressing Expected which is A/E/I/R
        "SYM_TOO_LONG",                             // Symbol Name is too long
        "TOO_MANY_DEF_IN_MODULE",                   // > 16
        "TOO_MANY_USE_IN_MODULE",                   // > 16
        "TOO_MANY_INSTR"                           // total num_instr exceeds memory size (512)
};
printf("Parse Error line %d offset %d: %s\n", lineNum, offset, errstr[errcode].c_str());
//exit(-1);
}


char* getToken()
{
    //Deal with continuous tokens in one line
    if(curToken != NULL && (curToken = strtok(NULL, " \t")) != NULL)
    {
//        cout << "continue:  " << curToken << endl;
        offset = curToken - beginLine + 1;
        return curToken;
    }else{
        fin.getline(line, maxLineNum);
        
        //Deal with empty lines
        while(!fin.eof() && strlen(line) == 0)
        {
            endOfLine = strlen(line) + 1;
            fin.getline(line, maxLineNum);
            lineNum++;
        }
        
        //Deal with new lines
        if(!fin.eof())
        {
            lineNum++;
            beginLine = line;                                   //Record the beginning pos of the line, or it will be changed by strtok
            endOfLine = strlen(line) + 1;                       // Used to record the end of file
            curToken = strtok(line, " \t");
//            cout << "new line:  " << curToken << endl;
            offset = curToken - beginLine + 1;
            return curToken;
        }else{
            offset = endOfLine;
            return NULL;
        }
    }
}

int readInt()
{
    char* token = getToken();
    
    //If returns null, but still readInt, then NUM_EXPECTED error
    if(token == NULL)
    {
        __parseerror(NUM_EXPECTED);
        exit(-1);
    }
    //Judge if it's a number
    int len = strlen(token);
    
    //Judge if the token is an integer
    for(int i = 0; i < len; ++i)
    {
        if(!isdigit(token[i]))
        {
            __parseerror(NUM_EXPECTED);
            exit(-1);
        }
    }
    
    return atoi(token);
}

string readSymbol()
{
    char* token = getToken();
    if(token == NULL)
    {
        __parseerror(SYM_EXPECTED);
        exit(-1);
    }
    size_t len = strlen(token);

    string tok(token, token+len);                           //Transfer to String
    
    //length > 16
    if(len > 16)
    {
        __parseerror(SYM_TOO_LONG);
        exit(-1);
    }

    //Judge if the first character is alphabetical
    if(!isalpha(token[0]))                                  //First char to be alpha
    {
        __parseerror(SYM_EXPECTED);
        exit(-1);
    }

    //Judge if the rest are alphanumerical
    for(int i = 1; i < len; ++i)
    {
        if(!isalnum(token[i]))
        {
            __parseerror(SYM_EXPECTED);
            exit(-1);
        }
    }
    return tok;
}

char readIAER()
{
    char* token = getToken();
    
    //If a token is expected, but get numm, raise ADDR_EXPECTED error
    if(token == NULL)
    {
        __parseerror(ADDR_EXPECTED);
        exit(-1);
    }
    size_t len = strlen(token);                             //Judge if it's IAER
    
    //Judge the length and if it is IAER
    if(len != 1)
    {
        __parseerror(ADDR_EXPECTED);
        exit(-1);
    }
    if(token[0] != 'I' && token[0] != 'A' && token[0] != 'E' && token[0] != 'R')
    {
        __parseerror(ADDR_EXPECTED);
        exit(-1);
    }
    return token[0];
}



//Get position of the symbol "key" from symbol table
// If cannot find the "key", return -1
int getPosFromSymbolTable(string key)
{
    for(int i = 0; i < symboltable.size(); ++i)
    {
        if(symboltable[i].first == key)
        {
            return symboltable[i].second;
        }
    }
    return -1;
}


void Pass1()
{
    int moduleOffset = 0;
    int accumulateInstNum = 0;
    unordered_map<string, int> tokenRepeat;
    int accumulateDefCount = 0;
    int moduleCount = 1;
    vector<int> defRelativePos;
    while(!fin.eof())
    {
        //define list
        int defCount = readInt();
        if(defCount > 16)
        {
            //Raise error if deflist > 16
            __parseerror(TOO_MANY_DEF_IN_MODULE);
            exit(-1);
        }
        for(int i = 0; i < defCount; ++i)
        {
            string symbol = readSymbol();
            int val = readInt();
            if(tokenRepeat.find(symbol) != tokenRepeat.end())
            {
                //If the token shows many times, just add its value to be > 1
                tokenRepeat[symbol]++;
            }else{
                //If the token is first found, set its value as 1, and append to symbol table
                tokenRepeat[symbol] = 1;
                symboltable.push_back(make_pair(symbol, val + moduleOffset));
                defRelativePos.push_back(val);
            }
        }

        
        //used list
        int useCount = readInt();
        if(useCount > 16)
        {
            //Raise error if uselist > 16
            __parseerror(TOO_MANY_USE_IN_MODULE);
            exit(-1);
        }
        for(int i = 0; i < useCount; i++)
        {
            string sym = readSymbol();
        }

        
        //instruction list
        int instCount = readInt();
        accumulateInstNum += instCount;
        
        //If the instr Num > 512, raise error TOO_MANY_INSTR
        if(accumulateInstNum > 512)
        {
            __parseerror(TOO_MANY_INSTR);
            exit(-1);
        }
        for(int i = 0; i < instCount; i++)
        {
            char addressMode = readIAER();
            int operand = readInt();
        }
        
        
        //Rule 5: the error of relative pos exceed the size of the module
        for(int j = accumulateDefCount; j < symboltable.size(); ++j)
        {
            pair<string, int> item = symboltable[j];
            if(defRelativePos[j] > instCount - 1)
            {
                symboltable[j].second -= defRelativePos[j];                     //Set relative address to zero
                printf("Warning: Module %d: %s too big %d (max=%d) assume zero relative\n", moduleCount, item.first.c_str(), defRelativePos[j], instCount-1);
            }
        }
        
        
        moduleCount++;                              //Update module index
        moduleOffset += instCount;                  //Update the offset of each module
        accumulateDefCount = symboltable.size();             //Update sum of appeared defination count
        fin.peek();                                 //Make sure the end of file is found
    }

    
    //Print out the symbol table
    printf("Symbol Table\n");
    for(int i = 0; i < symboltable.size(); ++i)
    {
        pair<string, int> item = symboltable[i];
        printf("%s=%d", item.first.c_str(), item.second);
        
        //Rule 2: Multiple time defined variables
        //If the value of the token is > 1 in the map, then it is multi-time defined
        if(tokenRepeat[item.first] > 1)
        {
            printf(" Error: This variable is multiple times defined; first value used\n");
        }else{
            printf("\n");
        }
    }

}


void Pass2()
{
    //Print out the Memory Map
    printf("Memory Map\n");
    vector<pair<string, int>> tokenUnused;
    vector<string> usedtable;
    
    //Init for vector tokenUnused, making the value of all symbols to be 0
    for(int i = 0; i < symboltable.size(); ++i)
    {
        pair<string, int> item = symboltable[i];
        tokenUnused.push_back(make_pair(item.first, 0));
    }
    
    int moduleOffset = 0;               //Module Base Offset
    int moduleCount = 1;                //Module Indexing
    
    //Start second pass of the file
    while(!fin.eof())
    {
        //define list
        int defCount = readInt();
        for(int i = 0; i < defCount; ++i)
        {
            string symbol = readSymbol();
            int val = readInt();
            
            //If a token is defined, we set its value as the index of the module it belongs
            for(int i = 0; i < tokenUnused.size(); ++i)
            {
                if(tokenUnused[i].first == symbol && tokenUnused[i].second != -1)
                {
                    tokenUnused[i].second = moduleCount;
                }
            }
        }

        //used list
        int useCount = readInt();
        for(int i = 0; i < useCount; i++)
        {
            string sym = readSymbol();
            usedtable.push_back(sym);
            
            //If a symbol is used, mark its value as -1
            for(int i = 0; i < tokenUnused.size(); ++i)
            {
                if(tokenUnused[i].first == sym)
                {
                    tokenUnused[i].second = -1;
                }
            }
        }

        
        //instruction list
        int instCount = readInt();
        
        //Init to record used tokens by E-Instr
        int eInstUsed[useCount];
        for(int var = 0; var < useCount; ++var)
        {
            eInstUsed[var] = 0;
        }
        
        for(int i = 0; i < instCount; i++)
        {
            char addressMode = readIAER();
            int operand = readInt();
            
            int opCode = operand / 1000;
            
            //Print out the first half of each item in memory map
            printf("%03d: ", i+moduleOffset);
            
            //Rule 11: Illegal opcode is encountered
            if(addressMode != 'I' && opCode >= 10)
            {
                operand = 9999;
                printf("%04d", operand);
                printf(" Error: Illegal opcode; treated as 9999\n");
                continue;
            }

            if(addressMode == 'A')
            {
                int absAddr = operand % 1000;
                
                //Rule 8: Absolute address exceeds machine size 512
                if(absAddr >= 512)
                {
                    //Set absolute address to 0
                    operand = operand - absAddr;
                    printf("%04d", operand);
                    printf(" Error: Absolute address exceeds machine size; zero used\n");
                }else{
                    printf("%04d\n", operand);
                }
            }else if(addressMode == 'I')
            {
                //Rule 10: Illegal immediate value (I) is encountered
                if(operand >= 10000)
                {
                    operand = 9999;
                    printf("%04d", operand);
                    printf(" Error: Illegal immediate value; treated as 9999\n");
                }else{
                    printf("%04d\n", operand);
                }
            }else if(addressMode == 'E')
            {
                int indexOfUsedList = operand % 1000;
                
                //Rule 6: External address exceeds the len of uselist
                if(indexOfUsedList > usedtable.size() - 1)
                {
                    printf("%04d", operand);
                    printf(" Error: External address exceeds length of uselist; treated as immediate\n");
                    continue;
                }
                
                //Record the used var by E-instr
                eInstUsed[indexOfUsedList] = 1;
                
                int pos = getPosFromSymbolTable(usedtable[indexOfUsedList]);
                
                //Rule 3: used in E-inst, but not defined
                //Cannot find in Func getPosFromSymbolTable, return -1
                if(pos == -1)
                {
                    //Make variable to be zero
                    pos = 0;
                    printf("%04d", operand - indexOfUsedList + pos);
                    printf(" Error: %s is not defined; zero used\n", usedtable[indexOfUsedList].c_str());
                }else{
                    printf("%04d\n", operand - indexOfUsedList + pos);                  // Replace: First clear the numbers behind opCode, then add position
                }
            
            //addressMode == 'R'
            }else{
                int relativeAddr = operand % 1000;
                
                //Rule 9: Relative address exceeds the size of the module
                if(relativeAddr > instCount - 1)
                {
                    operand = operand - relativeAddr;                                   //Set relative address to zero
                    printf("%04d", operand + moduleOffset);
                    printf(" Error: Relative address exceeds module size; zero used\n");
                }else{
                    printf("%04d\n", operand + moduleOffset);
                }
            }
        }
        
        //Rule 7: Symbol appears in use list, but not used
        for(int j = 0; j < useCount; ++j)
        {
            if(eInstUsed[j] == 0)
            {
                printf("Warning: Module %d: %s appeared in the uselist but was not actually used\n", moduleCount, usedtable[j].c_str());
            }
        }
        
        moduleOffset += instCount;                  //Update the offset of each module
        moduleCount++;                              //Update the count of module
        usedtable.clear();                          //Clear the usedTable, ready for the next module
        fin.peek();                                 //Check for the end of the file
    }
    
    
    //Rule 4: Symbol is defined, but not used
    printf("\n");
    for(int i = 0; i < tokenUnused.size(); ++i)
    {
        pair<string, int> item = tokenUnused[i];
        if(item.second != -1)
        {
            printf("Warning: Module %d: %s was defined but never used\n", item.second, item.first.c_str());
        }
    }
}


int main(int argc, char* argv[])
{
    
//    string fileDir = "/Users/chenmeng/Desktop/NYU_Courses/Term2/Operating System/Lab1/lab1_assign/";
//    string filename = "test1";
    string fileDir = "./";
    string filename = argv[1];
    string filePath = fileDir + filename;
    fin.open(filePath);
    if(!fin.is_open())
    {
        cout << "Fail to open the file !" << endl;
        exit(-1);
    }
    
    //Move the file pointer to the beginning
    fin.clear();
    fin.seekg(0);

    Pass1();
    
    printf("\n");
    
    //Move the file pointer to the beginning
    fin.clear();
    fin.seekg(0);

    Pass2();

    fin.close();
    
    return 0;
}
