#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl2.h"

#include <stdio.h>      /* to be removed */
#include <SDL.h>
#include <SDL_opengl.h>


/* constant colors used to present searching states at the main screen */
#define WHITE                   ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
#define GRAY                    ImVec4(0.5f, 0.5f, 0.5f, 1.0f)
#define RED                     ImVec4(1.0f, 0.0f, 0.0f, 1.0f)
#define GREEN                   ImVec4(0.0f, 1.0f, 0.0f, 1.0f)
#define YELLOW                  ImVec4(1.0f, 1.0f, 0.0f, 1.0f)
#define ORANGE                  ImVec4(1.0f, 0.7f, 0.0f, 1.0f)
#define BLUE                    ImVec4(0.0f, 0.0f, 1.0f, 1.0f)
#define LIGHTBLUE               ImVec4(0.0f, 0.6f, 0.8f, 1.0f)
#define BLACK                   ImVec4(0.0f, 0.0f, 0.0f, 1.0f)

#define IM_COL_BLACK            IM_COL32(0, 0, 0, 255)
#define IM_COL_RED              IM_COL32(255, 0, 0, 255)
#define BORDER_THICKNESS        0.3f

#define ABS(x)                  ((x > 0) ? (x) : -(x))
#define MAX2(x, y)              ((x > y) ? (x) : (y))
#define MIN2(x, y)              ((x < y) ? (x) : (y))

#define LINE_THICKNESS          3.0f

/* to avoid including math.h header to calculate square root */
#define SQRT2                   1.41421356f


#define GetBlockByIdx(idx, size)    ImVec2((int) idx % size, \
                                           (int) idx / size)

#define GetIdxByBlock(blk, size) ((int)(blk.x + blk.y * size)) 

#define CHECK_THREAD_EXITED(state, toBeFreed) \
do { \
        if (state == THREAD_EXITED) { \
            free(toBeFreed); \
            return NULL; \
        } \
} while(0);

#define BUSY_DELAY_EXECUTION(state, toBeFreed, timeout) \
do { \
    long prevTime = getCurrentMicroSecs(); \
    while (getCurrentMicroSecs() - prevTime < timeout) \
    { \
        CHECK_THREAD_EXITED(state, toBeFreed); \
        usleep(100); \
    }; \
} while(0); 

#define GetBlockPosition(colIdx, rowIdx, blkSize)       (ImVec2(4  + colIdx * (blkSize + 8) + mainWindowPosition.x - ImGui::GetScrollX(), \
                                                                 24 + rowIdx * (blkSize + 4) + mainWindowPosition.y - ImGui::GetScrollY()))

#define GetBlockCenter(blk, blkSize)                    (ImVec2(blk.x + blkSize/2, blk.y + blkSize/2))

typedef enum BlockLabels
{
    LBL_UNBLOCKED,
    LBL_BLOCKED,
    LBL_VISITED,
    LBL_VISITING,
    LBL_TOBEVISITED
} BlockLabels;

typedef enum ChoosingLabel
{
    CHOOSE_SOURCE,
    CHOOSE_TARGET,
    CHOOSE_BLOCKED_UNBLOCKED
} ChoosingLabel;

typedef enum ThreadState
{
    THREAD_INITIALIZED,         /* Shared information is just initialized, not started*/
    THREAD_RUNNING,             /* The child thread is running */
    THREAD_PAUSED,              /* The child thread is paused */
    THREAD_FINISHED,            /* The child thread finished execution => remain line-drawing status on the main screen */
    THREAD_EXITED               /* The child thread is exited => joined => line-draw of previous run is cleared */
} ThreadState;

typedef struct Cell
{
    ImVec2       block;
    float        f;             /* Total distance (f = g + h)                   */
    float        g;             /* Exact distance from SOURCE to this Cell      */
    float        h;             /* Heuristic distance from this Cell to TARGET  */
    float        f_order;       /* f + (insignificant amount), used to compare
                                   Cell so that all Cell are uniquely stored    */
    Cell        *prev;          /* previous Cell that when go through it
                                   recursively, we will end at SOURCE, and 
                                   archive the total distance `g`               */
} Cell;

typedef struct Grid
{
    int          nrow;
    int          ncol;
} Grid;

typedef struct ThreadSearchingState // TODO: only lock item which might be modified by both thread.  
{
    BlockLabels     *labels;
    Grid             windowSize;
    ThreadState     *state;
    Cell            *listCell;      /* state of nrow*ncol cell in listCell */
} ThreadSearchingState;


void printBlockNotation();
void initLabels(BlockLabels **labels, Grid* windowSize);
void reCalculateBlockSize(Grid* windowSize);
long getCurrentMicroSecs();
void endExec(Cell* listCell, Grid windowSize);
void RandomGrid(BlockLabels** labels, Grid* windowSize, float blockedRatio);
void *execAStar(void* arg);
