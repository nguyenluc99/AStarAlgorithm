#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl2.h"

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
int blockSize = 40;
float fontS = 13;
ImVec2 mainWindowPosition = ImVec2(0.0f, 0.0f);

void reCalculateBlockSize(Grid* windowSize)
{
    int defaultBlockSize = 40;
    int maxBlockWidth = (int) max_width / (windowSize->ncol);
    int maxBlockHeight = (int) max_height / (windowSize->nrow);
    blockSize = MIN2(defaultBlockSize, MIN2(maxBlockHeight, maxBlockWidth));
}

void printBlockNotation()
{
    /* Block notation: */
    ImGui::Dummy(ImVec2(0.0f, 15.0f));
    {
        ImGui::PushStyleColor(ImGuiCol_Header, GREEN);
        ImGui::Selectable("##demoSource", true, ImGuiSelectableFlags_None, ImVec2(fontS, fontS));
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("%s", "Source");
    }
    ImGui::SameLine();
    {
        ImGui::PushStyleColor(ImGuiCol_Header, RED);
        ImGui::Selectable("##demoTarget", true, ImGuiSelectableFlags_None, ImVec2(fontS, fontS));
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("%s", "Target");
    }
    ImGui::SameLine();
    {
        ImGui::PushStyleColor(ImGuiCol_Header, WHITE);
        ImGui::Selectable("##demoBlocked", true, ImGuiSelectableFlags_None, ImVec2(fontS, fontS));
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("%s", "Unblocked");
    }
    ImGui::SameLine();
    {
        ImGui::PushStyleColor(ImGuiCol_Header, GRAY);
        ImGui::Selectable("##demoUnBlocked", true, ImGuiSelectableFlags_None, ImVec2(fontS, fontS));
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("%s", "Blocked");
    }
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    {
        ImGui::PushStyleColor(ImGuiCol_Header, BLUE);
        ImGui::Selectable("##demoVisited", true, ImGuiSelectableFlags_None, ImVec2(fontS, fontS));
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("%s", "Visited");
    }
    ImGui::SameLine();
    {
        ImGui::PushStyleColor(ImGuiCol_Header, LIGHTBLUE);
        ImGui::Selectable("##demoVisited", true, ImGuiSelectableFlags_None, ImVec2(fontS, fontS));
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("%s", "To be visited");
    }
    ImGui::SameLine();
    {
        ImGui::PushStyleColor(ImGuiCol_Header, YELLOW);
        ImGui::Selectable("##demoVisited", true, ImGuiSelectableFlags_None, ImVec2(fontS, fontS));
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("%s", "Visiting");
    }
}

void initLabels(BlockLabels **labels, Grid* windowSize)
{
    windowSize->ncol = 5;
    windowSize->nrow = 5;
    if (*labels)
        free(*labels); // TODO: check: can I free it here?

    *labels = (BlockLabels*) malloc(25 * sizeof(BlockLabels));

    (*labels)[0]  = LBL_UNBLOCKED;  (*labels)[1]  = LBL_UNBLOCKED;
    (*labels)[2]  = LBL_UNBLOCKED;  (*labels)[3]  = LBL_UNBLOCKED;
    (*labels)[4]  = LBL_BLOCKED;    (*labels)[5]  = LBL_BLOCKED;
    (*labels)[6]  = LBL_UNBLOCKED;  (*labels)[7]  = LBL_BLOCKED;
    (*labels)[8]  = LBL_UNBLOCKED;  (*labels)[9]  = LBL_BLOCKED;
    (*labels)[10] = LBL_UNBLOCKED;  (*labels)[11] = LBL_UNBLOCKED;
    (*labels)[12] = LBL_UNBLOCKED;  (*labels)[13] = LBL_UNBLOCKED;
    (*labels)[14] = LBL_BLOCKED;    (*labels)[15] = LBL_UNBLOCKED;
    (*labels)[16] = LBL_BLOCKED;    (*labels)[17] = LBL_UNBLOCKED;
    (*labels)[18] = LBL_BLOCKED;    (*labels)[19] = LBL_UNBLOCKED;
    (*labels)[20] = LBL_UNBLOCKED;    (*labels)[21] = LBL_UNBLOCKED;
    (*labels)[22] = LBL_UNBLOCKED;    (*labels)[23] = LBL_UNBLOCKED;
    (*labels)[24] = LBL_UNBLOCKED;

    sourceIdx = 0; targetIdx = 24;
    assert((*labels)[sourceIdx] != LBL_BLOCKED && (*labels)[targetIdx] != LBL_BLOCKED);
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
    if (padx == 0 && pady == 0)
        return 0.0f;
    if (padx == 0 || pady == 0)
        return 1.0f;
    return (float) SQRT2;
}

long getCurrentMicroSecs()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return (long) now.tv_sec * (int)1e6 + now.tv_usec;
}

ImVec2 blockToPosition(ImVec2 block)
{
    float x = block.x * blockSize + blockSize / 2;
    float y = block.y * blockSize + blockSize / 2;
    return ImVec2(x, y);
}

float getBottomX(Grid windowSize){return mainWindowPosition.x + max_width + fontS + windowSize.ncol * 8;}
float getBottomY(Grid windowSize){return mainWindowPosition.y + max_height + fontS + windowSize.nrow * 4;}
/* Check whether the point is out of the main window, with mainWindowPosition variable */
bool outOfBox(ImVec2 point, Grid windowSize)
{
    return (point.x < mainWindowPosition.x + fontS + 4 ||
            point.y < mainWindowPosition.y - fontS ||
            point.x > getBottomX(windowSize) ||
            point.y > getBottomY(windowSize));
}


void getDrawablePos(ImVec2& outside, ImVec2 inside, Grid windowSize)
{
    float topX = mainWindowPosition.x + fontS + 4; float bottomX = getBottomX(windowSize);  /* topX < bottomX */
    float topY = mainWindowPosition.y - fontS; float bottomY = getBottomY(windowSize); /* topY < bottomY */
    if (outside.y < topY) /* higher than the main window */
    {
        float d1y  = topY - outside.y;
        float dy   = inside.y - outside.y;
        float dx   = inside.x - outside.x;
        float d1x  = (float) dx/dy * d1y;
        outside.x = outside.x + d1x;
        outside.y = outside.y + d1y; /* Handy enough? :( */
    }
    if (outside.x < topX)
    {
        float d1x  = topX - outside.x;
        float dx   = inside.x - outside.x;
        float dy   = inside.y - outside.y;
        float d1y  = (float) dy/dx * d1x;
        outside.x = outside.x + d1x;
        outside.y = outside.y + d1y;
    }
    if (outside.y > bottomY)
    {
        float d1y = outside.y - bottomY;
        float dy = outside.y - inside.y;
        float dx = outside.x - inside.x;
        float d1x = (float) dx/dy * d1y;
        outside.x -= d1x;
        outside.y -= d1y;
    }
    if (outside.x > bottomX)
    {
        float d1x = outside.x - bottomX;
        float dx = outside.x - inside.x;
        float dy = outside.y - inside.y;
        float d1y = (float) dy/dx * d1x;
        outside.x -= d1x;
        outside.y -= d1y;
    }
}

void drawLine(ImDrawList* draw_list, Cell* from, Cell* to, Grid windowSize) /* TODO: draw arrow on a side of a line. */
{
    ImVec2 fromPos = GetBlockCenter(GetBlockPosition(from->block.x, from->block.y, blockSize), blockSize); 
    ImVec2 toPos   = GetBlockCenter(GetBlockPosition(to->block.x, to->block.y, blockSize), blockSize); 
    if (outOfBox(fromPos, windowSize) && outOfBox(toPos, windowSize))
        return;

    /* at least one of the two points is in the box now. */
    if (outOfBox(fromPos, windowSize))
        getDrawablePos(fromPos, toPos, windowSize);
    else if (outOfBox(toPos, windowSize))
        getDrawablePos(toPos, fromPos, windowSize);

    draw_list->AddLine(fromPos, toPos, IM_COL_RED, LINE_THICKNESS);
}

void endExec(Cell* listCell, Grid windowSize)
{
    Cell* runningCell;
    int runningIdx;
    int ncol = windowSize.ncol;
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();

    runningCell = &listCell[targetIdx];
    runningIdx = targetIdx;

    while (runningIdx != sourceIdx)
    {
        drawLine(draw_list, runningCell->prev, runningCell, windowSize);
        runningCell = runningCell->prev;
        runningIdx = GetIdxByBlock(runningCell->block, ncol);
    }
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

    /* Random two unique numbers between 0 and numElement to be source and target: */
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
        CHECK_THREAD_EXITED(*(shared->state), heuDistance);

        int padx, pady;
        Cell* mainCell;
        int mainCellIdx;

        mainCell = *openList.begin();
        openList.erase(openList.begin());
        mainCellIdx = GetIdxByBlock(mainCell->block, windowSize->ncol);

        /* Finally reach the TARGET? */
        if (mainCellIdx == targetIdx)
        {
            shared->listCell = listCell;

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
            pthread_mutex_lock(&mutex);
            *(shared->state) = THREAD_EXITED; /* do we need this? Am I sure that the solution is founded?*/
            pthread_mutex_unlock(&mutex);
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
                CHECK_THREAD_EXITED(*(shared->state), heuDistance);

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
                BUSY_DELAY_EXECUTION(*(shared->state), heuDistance, 1/stepPerSecs * 1e6);

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

        BUSY_DELAY_EXECUTION(*(shared->state), heuDistance, 1/stepPerSecs * 1e6);
        /* Un-macro version: */
        // prevTime = getCurrentMicroSecs();
        // while (getCurrentMicroSecs() - prevTime < 1/stepPerSecs * 1e6)// ~ waitingTimeInterval
        // {
        //     CHECK_THREAD_EXITED(*(shared->state), heuDistance);
        //     usleep(100); /* just waiting */
        // };

        pthread_mutex_lock(&mutex);
        if (mainCellIdx != sourceIdx)
            labels[mainCellIdx] = LBL_VISITED;
        pthread_mutex_unlock(&mutex);
    }

    pthread_mutex_lock(&mutex);
    *(shared->state) = THREAD_FINISHED;
    pthread_mutex_unlock(&mutex);

    return NULL;
}
