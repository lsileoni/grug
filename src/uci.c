#define _CRT_SECURE_NO_WARNINGS 1 // NOLINT(bugprone-reserved-identifier) MSVC macro

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uci.h"
#include "board.h"
#include "movegen.h"
#include "search.h"
#include "perft.h"

static bool verb(const char* line, const char* word)
{
    size_t n = strlen(word);
    if (strncmp(line, word, n) != 0)
        return false;
    char c = line[n];
    return c == '\0' || c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static Move parseMove(const Board* b, const char* str)
{
    if (strlen(str) < 4)
        return NO_MOVE;
    int from = makeSquare(str[1] - '1', str[0] - 'a');
    int to = makeSquare(str[3] - '1', str[2] - 'a');
    int promo = -1;
    switch (str[4])
    {
        case 'q':
            promo = QUEEN;
            break;
        case 'r':
            promo = ROOK;
            break;
        case 'b':
            promo = BISHOP;
            break;
        case 'n':
            promo = KNIGHT;
            break;
        default:
            break;
    }

    Move moves[MAX_MOVES];
    int  n = generateAllMoves(b, moves);
    for (int i = 0; i < n; i++)
    {
        if (moveFrom(moves[i]) != from || moveTo(moves[i]) != to)
            continue;
        if (moveType(moves[i]) == PROMOTION)
        {
            if (movePromoPiece(moves[i]) == promo)
                return moves[i];
        }
        else
        {
            return moves[i];
        }
    }
    return NO_MOVE;
}

static void parsePosition(Board* b, char* line)
{
    char* p = line + strlen("position");
    while (*p == ' ')
        p++;

    if (strncmp(p, "startpos", 8) == 0)
    {
        boardSetFen(b, STARTPOS_FEN);
        p += 8;
    }
    else if (strncmp(p, "fen", 3) == 0)
    {
        p += 3;
        while (*p == ' ')
            p++;
        char  fen[128];
        char* moves = strstr(p, "moves");
        if (moves)
        {
            size_t len = (size_t)(moves - p);
            if (len >= sizeof fen)
                len = sizeof fen - 1;
            memcpy(fen, p, len);
            fen[len] = '\0';
        }
        else
        {
            snprintf(fen, sizeof fen, "%s", p);
        }
        boardSetFen(b, fen);
    }
    else
    {
        boardSetFen(b, STARTPOS_FEN);
    }

    char* moves = strstr(p, "moves");
    if (moves)
    {
        char* tok = strtok(moves + strlen("moves"), " \t\r\n");
        while (tok)
        {
            Move m = parseMove(b, tok);
            if (m == NO_MOVE)
                break;
            Undo u;
            applyMove(b, m, &u);
            tok = strtok(NULL, " \t\r\n");
        }
    }
}

static void parseGo(Board* b, char* line)
{
    SearchLimits lim;
    memset(&lim, 0, sizeof lim);

    strtok(line, " \t\r\n"); // prime strtok and skip the "go" verb
    char* tok = strtok(NULL, " \t\r\n");

    if (tok && strcmp(tok, "perft") == 0)
    {
        char* d = strtok(NULL, " \t\r\n");
        perftDivide(b, d ? atoi(d) : 1);
        return;
    }

    while (tok)
    {
        if (strcmp(tok, "infinite") == 0)
        {
            lim.infinite = true;
        }
        else if (strcmp(tok, "ponder") == 0)
        {
        }
        else
        {
            char* val = strtok(NULL, " \t\r\n");
            if (!val)
                break;
            if (strcmp(tok, "wtime") == 0)
                lim.wtime = atoll(val);
            else if (strcmp(tok, "btime") == 0)
                lim.btime = atoll(val);
            else if (strcmp(tok, "winc") == 0)
                lim.winc = atoll(val);
            else if (strcmp(tok, "binc") == 0)
                lim.binc = atoll(val);
            else if (strcmp(tok, "movestogo") == 0)
                lim.movestogo = atoi(val);
            else if (strcmp(tok, "movetime") == 0)
                lim.movetime = atoll(val);
            else if (strcmp(tok, "depth") == 0)
                lim.depth = atoi(val);
            else if (strcmp(tok, "nodes") == 0)
                lim.nodes = atoll(val);
        }
        tok = strtok(NULL, " \t\r\n");
    }

    searchPosition(b, &lim);
}

static void parseSetOption(char* line)
{
    char* name = strstr(line, "name");
    char* value = strstr(line, "value");
    if (!name)
        return;
    name += strlen("name");
    while (*name == ' ')
        name++;
    if (value)
    {
        value += strlen("value");
        while (*value == ' ')
            value++;
    }

    if (strncmp(name, "Algorithm", 9) == 0 && value)
    {
        char* end = value + strcspn(value, "\r\n");
        while (end > value && (end[-1] == ' ' || end[-1] == '\t'))
            end--;
        *end = '\0';
        searchSetAlgorithm(value);
    }
}

void uciLoop(void)
{
    Board board;
    boardSetFen(&board, STARTPOS_FEN);

    char line[1 << 16];
    while (fgets(line, sizeof line, stdin))
    {
        if (verb(line, "uci"))
        {
            printf("id name %s %s\n", ENGINE_NAME, ENGINE_VERSION);
            printf("id author %s\n", ENGINE_AUTHOR);
            searchPrintAlgorithmOptions();
            printf("uciok\n");
        }
        else if (verb(line, "isready"))
            printf("readyok\n");
        else if (verb(line, "ucinewgame"))
        {
            searchNewGame();
            boardSetFen(&board, STARTPOS_FEN);
        }
        else if (verb(line, "position"))
            parsePosition(&board, line);
        else if (verb(line, "go"))
            parseGo(&board, line);
        else if (verb(line, "setoption"))
            parseSetOption(line);
        else if (verb(line, "perft"))
        {
            int d = atoi(line + strlen("perft"));
            perftDivide(&board, d > 0 ? d : 1);
        }
        else if (verb(line, "eval"))
        {
            int score;
            if (searchEvaluate(&board, &score))
                printf("eval: %d cp (side to move)\n", score);
            else
                printf("eval: unavailable for algorithm %s\n", searchActiveAlgorithmName());
        }
        else if (verb(line, "d"))
            boardPrint(&board);
        else if (verb(line, "stop"))
        {
        }
        else if (verb(line, "quit"))
            break;

        fflush(stdout);
    }
}
