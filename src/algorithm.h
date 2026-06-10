#ifndef GRUG_ALGORITHM_H
#define GRUG_ALGORITHM_H

#include "search.h"

// A move-selection algorithm. Define one with designated initializers and
// register it in algorithm.c; unset hooks are simply skipped.
//
// chooseMove receives a scratch copy of the position,
// limits that are never NULL, and a pre-initialized result. The engine
// validates bestMove and plays the first legal move if it is missing or bad.
typedef struct
{
    const char* name;
    const char* description;

    void (*init)(void);
    void (*newGame)(void);
    int (*evaluate)(const Board* b);
    void (*chooseMove)(Board* b, const SearchLimits* limits, SearchResult* result);
} Algorithm;

const Algorithm* algorithmDefault(void);
const Algorithm* algorithmFind(const char* name);
void             algorithmPrintUciOptions(void);

#endif
