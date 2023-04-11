// Dear ImGui: standalone example application for SDL2 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

// **DO NOT USE THIS CODE IF YOUR CODE/ENGINE IS USING MODERN OPENGL (SHADERS, VBO, VAO, etc.)**
// **Prefer using the code in the example_sdl2_opengl3/ folder**
// See imgui_impl_sdl2.cpp for details.

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl2.h"
#include <SDL.h>
#include <SDL_opengl.h>

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include "utils.hpp"


extern int   sourceIdx, targetIdx;
extern float stepPerSecs;
extern int   max_width, max_height;
extern pthread_mutex_t mutex;
extern int blockSize;
extern ImVec2 mainWindowPosition;
extern float fontS;


int main(int argc, char** argv)
{
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Setup window
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("A Star algorithm demonstration program", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);

    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    // ImGui::StyleColorsDark();
    ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL2_Init();


    // Main loop
    bool done = false;
    ChoosingLabel choosingOpt = CHOOSE_BLOCKED_UNBLOCKED;

    Grid windowSize;
    int nrow = 5,
        ncol = 5;

    BlockLabels *labels = NULL;
    initLabels(&labels, &windowSize);
    ThreadState t_state = THREAD_INITIALIZED;
    pthread_t thread_id;
    bool show_config_window = false;
    ThreadSearchingState shared;
    char resultMsg[50];
    bool show_warning_init_new_state = false;
    float blockedRatio = 0.3;
    resultMsg[0] = 0;

    windowSize.nrow = nrow;
    windowSize.ncol = ncol;

    while (!done)
    {
        long startTime = getCurrentMicroSecs();
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        fontS = ImGui::GetFont()->FontSize;

        /*
         * Get size of the main window and assign it to the subframe
         * so that the subframe can cover the whole window.
         * It is a little bit handy, though.
         */
        SDL_GetWindowSize(window, &max_width, &max_height);
        ImGui::SetNextWindowSize(ImVec2(max_width, max_height));
        ImGui::SetWindowPos("A star algo", ImVec2(0.0f, 0.0f), ImGuiCond_FirstUseEver) ;

        ImGui::Begin("A star algo", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
        ImGui::Dummy(ImVec2(0.0f, 2*fontS));
        mainWindowPosition = ImGui::GetWindowPos();
        mainWindowPosition.y += 1*fontS -2; /* Magic number, as we are hiding the frame name: "A star algo"*/

        reCalculateBlockSize(&windowSize);
        {
            for (int rowIdx = 0; rowIdx < windowSize.nrow; rowIdx++)
            {
                for (int colIdx = 0; colIdx < windowSize.ncol; colIdx++)
                {
                    char name[16];
                    int idx = windowSize.ncol * rowIdx + colIdx;
                    ImVec4 color;

                    sprintf(name, "##%d", idx); /* Label of item will be hidden if its name start with '##' .*/
                    if (colIdx > 0)
                        ImGui::SameLine();

                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));

                    if (idx == sourceIdx)
                    {
                        color = GREEN;
                    }
                    else if (idx == targetIdx)
                    {
                        color = RED;
                    }
                    else
                    {
                        switch (labels[idx])
                        {
                            case LBL_UNBLOCKED:
                                color = WHITE;
                                break;
                            case LBL_BLOCKED:
                                color = GRAY;
                                break;
                            case LBL_VISITED:
                                color = BLUE;
                                break;
                            case LBL_VISITING:
                                color = YELLOW;
                                break;
                            case LBL_TOBEVISITED:
                                color = LIGHTBLUE;
                                break;
                            default:
                                break;
                        }
                    }

                    ImGui::PushStyleColor(ImGuiCol_Header, color);      /* Color of background  */
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);

                    if (ImGui::Selectable(name, true, ImGuiSelectableFlags_None, ImVec2(blockSize, blockSize)))
                    {
                        /* Once the previous execution finish, we forbid modify block state by clicking on them */
                        if (t_state == THREAD_FINISHED ||
                            t_state == THREAD_EXITED)
                            show_warning_init_new_state = true;
                        else
                        {
                            pthread_mutex_lock(&mutex);
                            switch (choosingOpt)
                            {
                                case CHOOSE_SOURCE:
                                    labels[sourceIdx] = LBL_UNBLOCKED;
                                    if (idx == targetIdx)
                                        targetIdx = -1;
                                    sourceIdx = idx;
                                    break;
                                case CHOOSE_TARGET:
                                    labels[targetIdx] = LBL_UNBLOCKED;
                                    if (idx == sourceIdx)
                                        sourceIdx = -1;
                                    targetIdx = idx;
                                    break;
                                case CHOOSE_BLOCKED_UNBLOCKED:
                                    labels[idx] = (BlockLabels) (LBL_BLOCKED + LBL_UNBLOCKED - (int)labels[idx]);
                                    if (idx == sourceIdx)
                                        sourceIdx = -1;
                                    else if (idx == targetIdx)
                                        targetIdx = -1;
                                    break;
                                default:
                                    break;
                            }
                            pthread_mutex_unlock(&mutex);
                        }
                    }
                    /* draw a rectangle around this `selectable` manually: tired enough? */
                    {
                        ImVec2 topleft = GetBlockPosition(colIdx, rowIdx, blockSize);
                        ImVec2 bottomright = ImVec2(topleft.x + blockSize + 8, topleft.y + blockSize + 5);
                        ImGui::GetWindowDrawList()->AddRect(topleft, bottomright, IM_COL_BLACK, 0.0f, ImDrawFlags_None, BORDER_THICKNESS);
                    }

                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleVar();
                }
            }

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 24.f);

            printBlockNotation();
            /* dummy space to separate buttons and grid */
            ImGui::Dummy(ImVec2(0.0f, 15.0f));

            /* Default WHITE */
            ImGui::PushStyleColor(ImGuiCol_Button, (choosingOpt == CHOOSE_SOURCE) ? YELLOW : WHITE);
            if (ImGui::Button("Choose Source", BUTTON_SIZE))
                choosingOpt = CHOOSE_SOURCE;
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, (choosingOpt == CHOOSE_TARGET) ? YELLOW : WHITE);
            if (ImGui::Button("Choose target", BUTTON_SIZE))
                choosingOpt = CHOOSE_TARGET;
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, (choosingOpt == CHOOSE_BLOCKED_UNBLOCKED) ? YELLOW : WHITE);
            if (ImGui::Button("Choose blocked/unblocked", BUTTON_SIZE_LARGE))
                choosingOpt = CHOOSE_BLOCKED_UNBLOCKED;
            ImGui::PopStyleColor();

            /* Exit the thread after finishing */
            if (t_state == THREAD_FINISHED)
            {
                pthread_mutex_lock(&mutex);
                pthread_join(thread_id, NULL);
                t_state = THREAD_EXITED;
                pthread_mutex_unlock(&mutex);
            }

            /* Thread finished finding, now print the result: */
            if (t_state == THREAD_FINISHED ||
                t_state == THREAD_EXITED)
            {
                if (shared.listCell != NULL)
                {
                    /* print the path from TARGET to SOURCE */
                    endExec(shared.listCell, windowSize);
                    sprintf(resultMsg, "\tEXECUTION DONE.\t");
                }
                else
                    sprintf(resultMsg, "\tNOT FOUND ANY DIRECTION.\t");
            }
            ImGui::SameLine();
            ImGui::Text("%s", resultMsg);

            if (ImGui::Button("Random grid", BUTTON_SIZE))
                show_config_window = true;
            ImGui::SameLine();
            if (ImGui::Button("RESET", BUTTON_SIZE))
            {
                pthread_mutex_lock(&mutex);
                t_state = THREAD_INITIALIZED;
                initLabels(&labels, &windowSize);
                pthread_mutex_unlock(&mutex);
                resultMsg[0] = 0;
                show_warning_init_new_state = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("PAUSE/CONTINUE", BUTTON_SIZE))
            {
                pthread_mutex_lock(&mutex);
                if (t_state == THREAD_RUNNING)
                    t_state = THREAD_PAUSED;
                else if (t_state == THREAD_PAUSED)
                    t_state = THREAD_RUNNING;
                pthread_mutex_unlock(&mutex);
            }
            if (show_warning_init_new_state)
            {
                ImGui::SameLine();
                ImGui::Text("%s", "Please choose Reset or Random Grid to init new state!");
            }

            if (t_state == THREAD_EXITED)
            {
                /* We do nothing this case.
                 * Just wait for user to RESET or Random new Grid to initialize thread */
            }

            if (t_state == THREAD_PAUSED)
            {
                ImGui::SameLine();
                ImGui::Text("\tPAUSED.\t");
            }

            if (show_config_window)
            {
                ImGui::Begin("Another Window", &show_config_window, ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::InputInt("Num row", &nrow);
                ImGui::InputInt("Num col", &ncol);
                ImGui::InputFloat("Blocked ratio", &blockedRatio);

                if (blockedRatio < 0.0f ||
                    blockedRatio > 1.0f)
                {
                    ImGui::SameLine();
                    ImGui::Text("\tPlease input `Blocked ratio` in range of (0.0,1.0).\t");
                }

                if (ImGui::Button("Random grid", BUTTON_SIZE))
                {
                    windowSize.nrow = nrow;
                    windowSize.ncol = ncol;
                    if (blockedRatio <= 1.0f &&
                        blockedRatio >= 0.0f)
                        {
                            if (t_state != THREAD_INITIALIZED)
                                {
                                    /* Force end the currently-running thread. */
                                    pthread_mutex_lock(&mutex);
                                    /* Ask child to exit */
                                    t_state = THREAD_EXITED;
                                    pthread_join(thread_id, NULL);
                                    t_state = THREAD_INITIALIZED;
                                    pthread_mutex_unlock(&mutex);
                                }
                            RandomGrid(&labels, &windowSize, blockedRatio);
                        show_config_window = false;
                        }
                    show_warning_init_new_state = false;
                }
                ImGui::SameLine();
                if (ImGui::Button("Close", BUTTON_SIZE))
                    show_config_window = false;

                ImGui::End();
            }



            ImGui::PushStyleColor(ImGuiCol_Button, GREEN);
            if (ImGui::Button("EXECUTE", BUTTON_SIZE) &&
                t_state == THREAD_INITIALIZED)
            {
                shared.labels = labels;
                shared.windowSize.nrow = windowSize.nrow;
                shared.windowSize.ncol = windowSize.ncol;
                t_state = THREAD_RUNNING;
                shared.state = &t_state;
                shared.listCell = NULL;
                int err = pthread_create(&thread_id,
                                         NULL,
                                         execAStar,
                                         (void*) &shared);
                resultMsg[0] = 0;
                if (err != 0)
                {
                    /* Ask the child to stop */
                    pthread_mutex_lock(&mutex);
                    t_state = THREAD_EXITED;
                    pthread_mutex_unlock(&mutex);

                    /* now we can join the child before returning */
                    pthread_join(thread_id, NULL); // deadlock should not occur here.
                    return 1; // Error occurs.
                }
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, RED);
            if (ImGui::Button("QUIT", BUTTON_SIZE) || /* either the user click QUIT, or `x` on the corner with a `initialized` thread */
                (done &&
                    (t_state == THREAD_PAUSED ||
                     t_state == THREAD_RUNNING)))
            {
                /* Ask the child to stop */
                pthread_mutex_unlock(&mutex);
                t_state = THREAD_EXITED;
                pthread_mutex_lock(&mutex);
                /* now we can join the child before quiting */
                pthread_join(thread_id, NULL);
                break;
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();

            ImGui::PopStyleColor();

        }
        /* Continuously parse a float from slider in range of 0.1f to 500.0f */
        ImGui::SliderFloat("Steps/sec", &stepPerSecs, 0.1f, 500.0f);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::End();

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        // glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        //glUseProgram(0); // You may want this if using this code in an OpenGL 3+ context where shaders may be bound
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);

        /*
         * As we do not know how to set refresh rate of the main screen at the moment,
         * we are using busy waiting to force the main screen to wait for a specific
         * amount of time enough for the FPS. This will enforce a maximum FPS rate on
         * the main screen.
         */
        while(getCurrentMicroSecs() - startTime <  1e6 / MAIN_SCREEN_FPS)
            /*
             * 60 FPS ~ 16667 microseconds/frame.
             * The variances about 500/16667 ~ 3% should be enough.
             * Ideally, the expected refresh rate is about 60*0.985 = 59.1 (FPS)
             */
            usleep(500);

    }

    // Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
