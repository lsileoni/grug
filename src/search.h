#ifndef GRUG_SEARCH_H
#define GRUG_SEARCH_H

#include "board.h"

typedef struct
{
    long long wtime, btime;
    long long winc, binc;
    int       movestogo;
    long long movetime;
    int       depth;
    long long nodes;
    bool      infinite;
} SearchLimits;

typedef struct
{
    Move     bestMove;
    uint64_t nodes;
    bool     hasScore;
    int      score;
} SearchResult;

void        searchInit(void);
void        searchNewGame(void);
bool        searchSetAlgorithm(const char* name);
const char* searchActiveAlgorithmName(void);
bool        searchEvaluate(const Board* b, int* score);
void        searchPrintAlgorithmOptions(void);

void searchPosition(Board* b, const SearchLimits* limits);

#endif
