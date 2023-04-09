#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl2.h"

#include <stdio.h>      /* to be removed */
#include <SDL.h>
#include <SDL_opengl.h>
#include <assert.h>
#include <limits.h>
#include <utility>
#include <set>
#include <iostream>
#include <sys/time.h>
#include <unistd.h>
#include <random>

#include <pthread.h>

#include "utils.hpp"


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int sourceIdx = -1, targetIdx = -1;
float stepPerSecs = 1.0f;
int max_width = 1000, max_height = 700;


void reCalculateBlockSize(Grid* windowSize, int* blockSize)
{
    int maxBlockWidth = (int) max_width / (windowSize->ncol);
    int maxBlockHeight = (int) max_height / (windowSize->nrow);
    *blockSize = MIN2(*blockSize, MIN2(maxBlockHeight, maxBlockWidth));
}

/*
 * Initialize a heuristic distance from each node to the target
 * The distance is composed of three kinds of movement: 1. Vertical,
 * 2. Horizontal, 3. Diagonal 45 degrees.
 */
float* initHeuristicDistance(BlockLabels* labels, Grid *windowSize)
{
    assert(sourceIdx >= 0 && targetIdx >= 0); /* To be removed */

    int idx, numElement;
    ImVec2 target;
    float* heuDistance;
    
    numElement = windowSize->nrow * windowSize->ncol;
    target = GetBlockByIdx(targetIdx, windowSize->ncol);
    heuDistance = (float*) malloc(numElement * sizeof(float));
    
    for (idx = 0; idx < numElement; ++idx)
    {
        if (labels[idx] == LBL_BLOCKED)
        {
            heuDistance[idx] = INT_MAX;
            continue;
        }

        ImVec2 currentBlock = GetBlockByIdx(idx, windowSize->ncol);
        int diffX = ABS(target.x - currentBlock.x);
        int diffY = ABS(target.y - currentBlock.y);

        if (diffX > diffY)
            heuDistance[idx] = (diffX - diffY) + diffY * SQRT2;
        else
            heuDistance[idx] = (diffY - diffX) + diffX * SQRT2;
    }

    return heuDistance;
};

/*
 * Init a list cell representing all block in the grid with additional
 * information about heuristic distance from the block to TARGET.
 */
Cell* initListCell(float* heuDistance, Grid* windowSize)
{
    int i, numElement;
    Cell *listCell;

    numElement = windowSize->nrow * windowSize->ncol;
    listCell   = (Cell*) malloc(sizeof(Cell) * numElement);

    for (i = 0; i < numElement; i++)
    {
        listCell[i].block       = GetBlockByIdx(i, windowSize->ncol);
        listCell[i].f           = INT_MAX;
        listCell[i].g           = (i == sourceIdx) ? 0.0f : INT_MAX;
        listCell[i].h           = heuDistance[i];
        listCell[i].f_order     = INT_MAX -  ((float) i / (numElement + 1));
        listCell[i].prev        = NULL;
    }

    return listCell;
}

bool isValidBlock(ImVec2 blk, Grid* windowSize)
{
    return (blk.x >= 0 &&
            blk.x < windowSize->ncol &&
            blk.y >= 0 &&
            blk.y < windowSize->nrow);
}

bool isValidIdx(int idx, int numElement)
{
    return (idx >= 0 &&
            idx < numElement);
}

float adjDistance(int padx, int pady)
{
    if (padx == 0 && pady == 0) return 0.0f;
    if (padx == 0 || pady == 0) return 1.0f;
    return (float) SQRT2;
}

long getCurrentMicroSecs()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return (long) now.tv_sec * (int)1e6 + now.tv_usec;
}

void endExec(Cell* listCell, int ncol)
{
    Cell* runningCell;
    int runningIdx;
    ImVec2 blk; // TODO: This variable seems redundant, can be optimized

    runningCell = &listCell[targetIdx];
    runningIdx = targetIdx;
    std::cout << "Found a way to go from SOURCE to TARGET: " << std::endl;
    while (runningIdx != sourceIdx)
    {
        std::cout << "visit (" << runningCell->block.x << ", " << runningCell->block.y << ")" << std::endl;
        runningCell = runningCell->prev;
        runningIdx = GetIdxByBlock(runningCell->block, ncol);
        usleep(1/stepPerSecs * 1e6);
    }
    /* runningIdx must be sourceIdx now: */
    blk = GetBlockByIdx(runningIdx, ncol);
    std::cout << "visit (" << blk.x << ", " << blk.y << ")" << std::endl;
    // TODO: DRAW LINE
}

void RandomGrid(BlockLabels** labels, Grid* windowSize, float blockedRatio)
{
    int idx, numElement;

    numElement = windowSize->nrow * windowSize->ncol;        

    pthread_mutex_lock(&mutex);
    if(*labels)
        free(*labels);
    *labels = (BlockLabels*) malloc(numElement * sizeof(BlockLabels));
    pthread_mutex_unlock(&mutex);

    /* Random a number between 0 and numElement to be source: */
    sourceIdx = rand() % numElement;
    targetIdx = -1;
    do {
        targetIdx = rand() % numElement;
    }
    while (targetIdx == sourceIdx);

    (*labels)[sourceIdx] = LBL_UNBLOCKED;
    (*labels)[targetIdx] = LBL_UNBLOCKED;

    for (idx = 0; idx < numElement; idx++)
    {
        if (idx == sourceIdx ||
            idx == targetIdx)
            continue;
        /* Random a number to be BLOCKED (blockedRatio) or UNBLOCKED (1-blockedRatio - the rest) */
        int ranNum = rand() % 1000;
        if (ranNum < 1000 * blockedRatio)
            (*labels)[idx] = LBL_BLOCKED;
        else
            (*labels)[idx] = LBL_UNBLOCKED;
    }
}

/*
 * DESIGN:
 *  Calculate unblocked-target heuristic from each point to target
 *  Blocked-target = infinity
 * 
 * n: one of 8 neighbor
 * g(n): exact cost from SOURCE to n. 
 * h(n): estimated cost from n to TARGET
 * f(n) = g(n) + h(h)
 *
 * Coordinator is as below:
 * O -------------> x
 * |
 * |
 * |
 * y
 */
void *execAStar(void* arg)
{
    ThreadSearchingState *shared = (ThreadSearchingState*) arg;
    BlockLabels* labels = shared->labels;
    Grid *windowSize = &(shared->windowSize);
    int idx;
    int numElement = (int) windowSize->nrow * windowSize->ncol;

    float* heuDistance;
    Cell* listCell;
    auto comp = [](Cell* a, Cell* b) {return (a->f_order < b->f_order);};
    std::set<Cell*, decltype(comp)> openList = std::set<Cell*, decltype(comp)> (comp);

    /* Clear previously run state */
    for (idx = 0; idx < numElement; idx++)
        if (labels[idx] != LBL_BLOCKED) labels[idx] = LBL_UNBLOCKED;

    heuDistance = initHeuristicDistance(labels, windowSize);
    listCell = initListCell(heuDistance, windowSize);
    
    /* Init the openList with the source in order to start traversing */
    openList.insert(&listCell[sourceIdx]);

    while (!openList.empty())
    {
        /*
         * If the main thread says force end, stop immediately.
         * This seems to be redundant as we already check inside
         * two nested for-loop condition below. But we just leave
         * it here, and might remove it later.
         */
        CHECK_THREAD_END(*(shared->state), heuDistance);

        int padx, pady;
        Cell* mainCell;
        int mainCellIdx;

        mainCell = *openList.begin();
        openList.erase(openList.begin());
        mainCellIdx = GetIdxByBlock(mainCell->block, windowSize->ncol);

        if (mainCellIdx == targetIdx)
        {
            endExec(listCell, windowSize->ncol);

            pthread_mutex_lock(&mutex);
            *(shared->state) = THREAD_END;
            pthread_mutex_unlock(&mutex);

            break;
        } 
        
        if (labels[mainCellIdx] == LBL_VISITED)
            /* This block (with the seemingly shorter distance) has been already visited by other path */
            continue;
        /* We might want to visit cell that is ready to be visited */
        else if (labels[mainCellIdx] == LBL_TOBEVISITED) 
        {
            pthread_mutex_lock(&mutex);
            /* First time this block is visited */
            labels[mainCellIdx] = LBL_VISITED; // redundant as we assign it as visited at the end.
            pthread_mutex_unlock(&mutex);
        }
        else if (mainCellIdx != sourceIdx) 
        {
            /* Weird enough? This should not happen */
            printf("ERROR HAPPENED");
            return NULL;
        }

        pthread_mutex_lock(&mutex);
        labels[mainCellIdx] = LBL_VISITING; /* To change the color in main screen */
        pthread_mutex_unlock(&mutex);
        
        for (pady = -1; pady <= 1; pady ++)
        {
            for (padx = -1; padx <= 1; padx ++)
            {
                /* Busy waiting if the main thread says PAUSED */
                while(*(shared->state) == THREAD_PAUSED) {
                    // deadlock might occur if we PAUSE at child, RESET at main and dont check this if-else
                };
                CHECK_THREAD_END(*(shared->state), heuDistance);

                float successor_f, successor_g, successor_h;
                Cell* successorCell;
                ImVec2 blk = ImVec2(mainCell->block.x+padx, mainCell->block.y+pady);
                int successorIdx = GetIdxByBlock(blk, windowSize->ncol);
                
                if (!isValidIdx(successorIdx, numElement))
                    continue;

                successorCell = &listCell[successorIdx];
                successorCell->block.x = mainCell->block.x + padx; /* Are these redundant? */
                successorCell->block.y = mainCell->block.y + pady;

                /* Skip if the successor is invalid (out of grid) */
                if (!isValidBlock(successorCell->block, windowSize))
                    continue;

                /* 
                 * Skip if the successor is BLOCKED, or SOURCE
                 * 
                 * We still process if the successor is VISITED, 
                 * since new shorter distance might be found and 
                 * updated later.
                 */
                if (labels[successorIdx] == LBL_BLOCKED || 
                    labels[successorIdx] == LBL_VISITING ||
                    successorIdx == sourceIdx) 
                {
                    continue;
                }

                /*
                 * Because there might be a RESET signal which interrupts this execution immediately,
                 * we use `busy waiting` so that we can exit the system when `forceEnd` is set
                 */
                BUSY_DELAY_EXECUTION();

                /* process each successor */
                successor_g = mainCell->g + adjDistance(padx, pady);    /* from source to successor */
                successor_h = heuDistance[successorIdx];                /* from successor to target */
                successor_f = successor_g + successor_h;                /* total from source to target */

                /* 
                 * Update shortest path to the boundary of current block accordingly
                 * 
                 * The cell may be visited in next rounds if either the new way with
                 * shorter path is found, or it has not been visited yet.
                 */
                if (successorCell->g > successor_g) 
                {
                    successorCell->f = successor_f;
                    successorCell->g = successor_g;
                    successorCell->h = successor_h;
                    /* 
                     * The set only save one element of a given key; therefore, in order 
                     * for the set to save different block with the same successor_f distance while 
                     * remaining the relative distance with other blocks, we add some 
                     * insignificantly extra `distance` and use it as key to distinguish.
                     */
                    successorCell->f_order = successor_f + ((float)successorIdx / (numElement * 100));
                    successorCell->prev = mainCell;
                    pthread_mutex_lock(&mutex);
                    /* As new path with shorter distance is updated, we might re-visit it again. */
                    if (labels[successorIdx] == LBL_UNBLOCKED)
                        labels[successorIdx] = LBL_TOBEVISITED;
                    pthread_mutex_unlock(&mutex);

                    openList.insert(successorCell);
                }
            }
        }

        BUSY_DELAY_EXECUTION();
        /* Un-macro version: */
        // prevTime = getCurrentMicroSecs();
        // while (getCurrentMicroSecs() - prevTime < 1/stepPerSecs * 1e6)// ~ waitingTimeInterval
        // {
        //     CHECK_THREAD_END(*(shared->state), heuDistance);
        //     usleep(100); /* just waiting */
        // };

        pthread_mutex_lock(&mutex);
        if (mainCellIdx != sourceIdx)
            labels[mainCellIdx] = LBL_VISITED;
        pthread_mutex_unlock(&mutex); // TODO: check this
    }
    
    pthread_mutex_lock(&mutex);
    *(shared->state) = THREAD_END; /* do we need this? Am I sure that the solution is founded?*/
    pthread_mutex_unlock(&mutex);

    return NULL;
}
