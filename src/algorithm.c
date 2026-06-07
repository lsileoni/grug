#include <stdio.h>
#include <string.h>

#include "algorithm.h"
#include "algorithms/basic_search.h"
#include "algorithms/e2e4.h"
#include "algorithms/first_generated.h"
#include "algorithms/first_legal.h"
#include "algorithms/no_move.h"
#include "algorithms/square_maximization.h"
#include "algorithms/threat_aware.h"

static const Algorithm* Algorithms[] = {
    &BasicSearchAlgorithm,    &FirstLegalAlgorithm,         &NoMoveAlgorithm,      &E2E4Algorithm,
    &FirstGeneratedAlgorithm, &SquareMaximizationAlgorithm, &ThreatAwareAlgorithm,
};

static const size_t AlgorithmCount = sizeof Algorithms / sizeof Algorithms[0];

const Algorithm* algorithmDefault(void)
{
    return &BasicSearchAlgorithm;
}

const Algorithm* algorithmFind(const char* name)
{
    if (!name)
        return NULL;
    for (size_t i = 0; i < AlgorithmCount; i++)
        if (strcmp(Algorithms[i]->name, name) == 0)
            return Algorithms[i];
    return NULL;
}

void algorithmPrintUciOptions(void)
{
    const Algorithm* def = algorithmDefault();
    printf("option name Algorithm type combo default %s", def->name);
    for (size_t i = 0; i < AlgorithmCount; i++)
        printf(" var %s", Algorithms[i]->name);
    printf("\n");
}
