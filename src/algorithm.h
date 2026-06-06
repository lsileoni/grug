#ifndef GRUG_ALGORITHM_H
#define GRUG_ALGORITHM_H

#include "search.h"

typedef struct
{
    const char* name;
    const char* description;

    void (*init)(void);
    void (*newGame)(void);
    bool (*evaluate)(const Board* b, int* score);
    bool (*chooseMove)(Board* b, const SearchLimits* limits, SearchResult* result);
} Algorithm;

const Algorithm* algorithmDefault(void);
const Algorithm* algorithmFind(const char* name);
void             algorithmPrintUciOptions(void);

#endif
