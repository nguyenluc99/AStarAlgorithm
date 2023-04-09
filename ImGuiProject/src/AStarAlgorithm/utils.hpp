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

#define ABS(x)                  ((x > 0) ? (x) : -(x))
#define MAX2(x, y)              ((x > y) ? (x) : (y))
#define MIN2(x, y)              ((x < y) ? (x) : (y))

/* to avoid including math.h header to calculate square root */
#define SQRT2                   1.41421356f


#define GetBlockByIdx(idx, size)    ImVec2((int) idx % size, \
                                           (int) idx / size)

#define GetIdxByBlock(blk, size) ((int)(blk.x + blk.y * size)) 

#define CHECK_THREAD_END(state, toBeFreed) \
do { \
        if (state  == THREAD_END) { \
            free(toBeFreed); \
            return NULL; \
        } \
} while(0);

#define BUSY_DELAY_EXECUTION() \
do { \
    long prevTime = getCurrentMicroSecs(); \
    while (getCurrentMicroSecs() - prevTime < 1/stepPerSecs * 1e6) \
    { \
        CHECK_THREAD_END(*(shared->state), heuDistance); \
        usleep(100); \
    }; \
} while(0); 


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
    CHOOSE_UNBLOCKED,
    CHOOSE_SOURCE,
    CHOOSE_TARGET,
    CHOOSE_BLOCKED
} ChoosingLabel;

typedef enum ThreadState
{
    THREAD_INITIALIZED,
    THREAD_RUNNING,
    THREAD_PAUSED,
    THREAD_END
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

typedef struct ThreadSearchingState
{
    BlockLabels     *labels;
    Grid             windowSize;
    ThreadState     *state;
} ThreadSearchingState;


void reCalculateBlockSize(Grid* windowSize, int* blockSize);
long getCurrentMicroSecs();
void RandomGrid(BlockLabels** labels, Grid* windowSize, float blockedRatio);
void *execAStar(void* arg);
