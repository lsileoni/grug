#include <stdio.h>
#include <string.h>

#include "search.h"
#include "algorithm.h"
#include "movegen.h"

static const Algorithm* ActiveAlgorithm = NULL;

static const Algorithm* activeAlgorithm(void)
{
    if (!ActiveAlgorithm)
        ActiveAlgorithm = algorithmDefault();
    return ActiveAlgorithm;
}

static bool generatedLegalMove(Board* b, Move move)
{
    if (move == NO_MOVE)
        return false;

    Move moves[MAX_MOVES];
    int  n = generateAllMoves(b, moves);
    for (int i = 0; i < n; i++)
        if (moves[i] == move)
            return moveIsLegal(b, move);

    return false;
}

static Move firstLegalMove(Board* b)
{
    Move moves[MAX_MOVES];
    int  n = generateAllMoves(b, moves);
    for (int i = 0; i < n; i++)
        if (moveIsLegal(b, moves[i]))
            return moves[i];

    return NO_MOVE;
}

void searchInit(void)
{
    ActiveAlgorithm = algorithmDefault();
    if (ActiveAlgorithm->init)
        ActiveAlgorithm->init();
}

void searchNewGame(void)
{
    const Algorithm* algorithm = activeAlgorithm();
    if (algorithm->newGame)
        algorithm->newGame();
}

bool searchSetAlgorithm(const char* name)
{
    const Algorithm* algorithm = algorithmFind(name);
    if (!algorithm)
    {
        printf(
            "info string unknown algorithm '%s'; keeping '%s'\n", name ? name : "",
            activeAlgorithm()->name
        );
        return false;
    }

    ActiveAlgorithm = algorithm;
    if (ActiveAlgorithm->init)
        ActiveAlgorithm->init();
    printf("info string algorithm set to %s\n", ActiveAlgorithm->name);
    return true;
}

const char* searchActiveAlgorithmName(void)
{
    return activeAlgorithm()->name;
}

bool searchEvaluate(const Board* b, int* score)
{
    const Algorithm* algorithm = activeAlgorithm();
    if (!algorithm->evaluate)
        return false;
    return algorithm->evaluate(b, score);
}

void searchPrintAlgorithmOptions(void)
{
    algorithmPrintUciOptions();
}

void searchPosition(Board* b, const SearchLimits* limits)
{
    const Algorithm* algorithm = activeAlgorithm();
    SearchResult     result;
    memset(&result, 0, sizeof result);
    result.bestMove = NO_MOVE;

    if (!algorithm->chooseMove || !algorithm->chooseMove(b, limits, &result))
    {
        printf("info string algorithm '%s' failed to choose a move\n", algorithm->name);
        result.bestMove = NO_MOVE;
    }

    Move best = result.bestMove;
    if (!generatedLegalMove(b, best))
    {
        if (best != NO_MOVE)
            printf(
                "info string algorithm '%s' returned an illegal move; using fallback\n",
                algorithm->name
            );
        best = firstLegalMove(b);
    }

    if (result.hasScore)
    {
        if (result.score >= VALUE_MATE_IN_MAX || result.score <= -VALUE_MATE_IN_MAX)
        {
            int plies = result.score > 0 ? VALUE_MATE - result.score : VALUE_MATE + result.score;
            int mate = (plies + 1) / 2;
            printf(
                "info score mate %d nodes %llu\n", result.score > 0 ? mate : -mate,
                (unsigned long long)result.nodes
            );
        }
        else
        {
            printf("info score cp %d nodes %llu\n", result.score, (unsigned long long)result.nodes);
        }
    }
    else if (result.nodes)
    {
        printf("info nodes %llu\n", (unsigned long long)result.nodes);
    }

    char str[8];
    moveToString(best, str);
    printf("bestmove %s\n", best == NO_MOVE ? "0000" : str);
    fflush(stdout);
}
