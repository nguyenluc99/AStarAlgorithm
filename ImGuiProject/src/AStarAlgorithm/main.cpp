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
#include <stdio.h>
#include <SDL.h>
// #include <SDL_video.h>
#include <SDL_opengl.h>

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include "utils.hpp"

#define MAIN_SCREEN_FPS         60
#define BUTTON_SIZE             ImVec2(120, 30)


extern int   sourceIdx, targetIdx;
extern float stepPerSecs;
extern int   max_width, max_height;
extern pthread_mutex_t mutex;
extern int blockSize;
extern ImVec2 mainWindowPosition;
extern float fontS;


// Main code
int main(int argc, char** argv)
{
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    if (argc == 3) {
        max_width = atoi(argv[1]);
        max_height = atoi(argv[2]);
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
    // SDL_DisplayMode mode;
    // mode.refresh_rate = 10;
    // SDL_SetWindowDisplayMode(window, &mode);
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

    // Our state
    bool show_demo_window = true;

    // Main loop
    bool done = false;
    ChoosingLabel choosingOpt = CHOOSE_BLOCKED_UNBLOCKED;
    // 2. Set window size, archive from user input
    Grid windowSize;
    int nrow = 5,
        ncol = 5;
    windowSize.nrow = nrow;
    windowSize.ncol = ncol;
    
    BlockLabels *labels = NULL;
    initLabels(&labels, &windowSize);
    ThreadState t_state = THREAD_INITIALIZED;
    pthread_t thread_id;
    bool show_config_window = false;
    ThreadSearchingState shared;
    char resultMsg[50];
    resultMsg[0] = 0;

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

        // ImGui::ImFont *fontA = ImGuiOverlay::AddDefaultFont(13);
        fontS = ImGui::GetFont()->FontSize;

        // ImGui::SetNextWindowSize(ImVec2(max_width, max_height));
        ImGui::Begin("A star algo", NULL, ImGuiWindowFlags_AlwaysAutoResize);
        mainWindowPosition = ImGui::GetWindowPos();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        reCalculateBlockSize(&windowSize);
        // 3. Show a window of size windowSize.
        {
            for (int rowIdx = 0; rowIdx < windowSize.nrow; rowIdx++)
            {
                // ImGui::Dummy(ImVec2(0.0f, 5.0f));
                for (int colIdx = 0; colIdx < windowSize.ncol; colIdx++)
                {
                    char name[16];
                    int idx = windowSize.ncol * rowIdx + colIdx;
                    // bool itemSelected = false;
                    ImVec4 color;

                    sprintf(name, "##%d", idx); /* Label of item will be hidden if its name start with '##' .*/
                    if (colIdx > 0)
                    {
                        // ImGui::Dummy(ImVec2(5.0f, 0.0f));
                        ImGui::SameLine();
                    }
                    // TODO: border boxes
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));

                    if (idx == sourceIdx)
                    {
                        color = GREEN;
                        // itemSelected = true;
                    }
                    else if (idx == targetIdx)
                    {
                        color = RED;
                        // itemSelected = true;
                    }
                    else
                    {
                        switch (labels[idx])
                        {
                            case LBL_UNBLOCKED:
                                // itemSelected = true;
                                break;
                            case LBL_BLOCKED:
                                color = GRAY;
                                // itemSelected = true;
                                break;
                            case LBL_VISITED:
                                color = BLUE;
                                // itemSelected = true;
                                break;
                            case LBL_VISITING:
                                color = YELLOW;
                                // itemSelected = true;
                                break;
                            case LBL_TOBEVISITED:
                                color = LIGHTBLUE;
                                // itemSelected = true;
                                break;
                            default:
                                break;
                        }
                    }

                    ImGui::PushStyleColor(ImGuiCol_Text, YELLOW);       /* Color of text        */
                    ImGui::PushStyleColor(ImGuiCol_Header, color);      /* Color of background  */
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
// ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.0f);
                    

                    if (ImGui::Selectable(name, true, ImGuiSelectableFlags_None, ImVec2(blockSize, blockSize)))
                    {
                        // /* Select a BLOCKED block unblock it. :) Confused enough? */
                        // if (labels[idx] != LBL_UNBLOCKED)
                        // {
                        //     pthread_mutex_lock(&mutex);
                        //     labels[idx] = LBL_UNBLOCKED;
                        //     /* If the block is SOURCE or TARGET, unselect it. */
                        //     if (sourceIdx == idx)
                        //         sourceIdx = -1;
                        //     else if (targetIdx == idx)
                        //         targetIdx = -1;
                        //     pthread_mutex_unlock(&mutex);
                        // }
                        // else /* The block is not BLOCKED? => Set the label of the block depending on the choosingOpt value. */
                        // TODO: blocked => target, source: OK
                        // but: target => unblocked => blocked
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
                    // draw a rectangle around this `selectable` manually: tired enough? 
                    {
                        ImVec2 topleft = GetBlockPosition(colIdx, rowIdx, blockSize);
                        ImVec2 bottomright = ImVec2(topleft.x + blockSize + 8, topleft.y + blockSize + 5);
                        ImGui::GetWindowDrawList()->AddRect(topleft, bottomright, IM_COL_BLACK, 0.0f, ImDrawFlags_None, BORDER_THICKNESS);
                    }

                    ImGui::PopStyleColor();
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleVar();
                }
            }
            
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 24.f);

            /* Block notation: */
            ImGui::Dummy(ImVec2(0.0f, 15.0f));
            {
                ImGui::PushStyleColor(ImGuiCol_Header, GREEN);      /* Color of background  */
                ImGui::Selectable("##demoSource", true, ImGuiSelectableFlags_None, ImVec2(fontS, fontS));
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::Text("%s", "Source");
            }
            ImGui::SameLine();
            {
                ImGui::PushStyleColor(ImGuiCol_Header, RED);      /* Color of background  */
                ImGui::Selectable("##demoTarget", true, ImGuiSelectableFlags_None, ImVec2(fontS, fontS));
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::Text("%s", "Target");
            }
            ImGui::SameLine();
            {
                ImGui::PushStyleColor(ImGuiCol_Header, WHITE);      /* Color of background  */
                ImGui::Selectable("##demoBlocked", true, ImGuiSelectableFlags_None, ImVec2(fontS, fontS));
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::Text("%s", "Unblocked");
            }
            ImGui::SameLine();
            {
                ImGui::PushStyleColor(ImGuiCol_Header, GRAY);      /* Color of background  */
                ImGui::Selectable("##demoUnBlocked", true, ImGuiSelectableFlags_None, ImVec2(fontS, fontS));
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::Text("%s", "Blocked");
            }
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            {
                ImGui::PushStyleColor(ImGuiCol_Header, BLUE);      /* Color of background  */
                ImGui::Selectable("##demoVisited", true, ImGuiSelectableFlags_None, ImVec2(fontS, fontS));
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::Text("%s", "Visited");
            }
            ImGui::SameLine();
            {
                ImGui::PushStyleColor(ImGuiCol_Header, LIGHTBLUE);      /* Color of background  */
                ImGui::Selectable("##demoVisited", true, ImGuiSelectableFlags_None, ImVec2(fontS, fontS));
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::Text("%s", "To be visited");
            }
            ImGui::SameLine();
            {
                ImGui::PushStyleColor(ImGuiCol_Header, YELLOW);      /* Color of background  */
                ImGui::Selectable("##demoVisited", true, ImGuiSelectableFlags_None, ImVec2(fontS, fontS));
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::Text("%s", "Visiting");
            }
            /* dummy space to separate buttons and grid */
            ImGui::Dummy(ImVec2(0.0f, 15.0f));
            
            ImGui::PushStyleColor(ImGuiCol_Button, WHITE);
            if (ImGui::Button("Choose Source", BUTTON_SIZE))
                choosingOpt = CHOOSE_SOURCE;

            ImGui::SameLine();
            if (ImGui::Button("Choose target", BUTTON_SIZE))
                choosingOpt = CHOOSE_TARGET;

            ImGui::SameLine();
            if (ImGui::Button("Choose blocked/unblocked", BUTTON_SIZE))
                choosingOpt = CHOOSE_BLOCKED_UNBLOCKED;

            ImGui::PushStyleColor(ImGuiCol_Button, GREEN);
            if (ImGui::Button("Exec A Star", BUTTON_SIZE) &&
                t_state != THREAD_RUNNING)
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
            if (ImGui::Button("Random grid", BUTTON_SIZE))
                show_config_window = true;
            ImGui::PopStyleColor();
            
            /* Thread finished finding, now print the result: */
            if (t_state == THREAD_FINISHED)
            {
                if (shared.listCell != NULL)
                {
                /* print the path from TARGET to SOURCE */     
                endExec(shared.listCell, windowSize);
                sprintf(resultMsg, "\tEXECUTION DONE.\t");
                }
                else
                {
                    sprintf(resultMsg, "\tNOT FOUND ANY DIRECTION.\t");
                }
            }
            ImGui::SameLine();
            ImGui::Text("%s", resultMsg);

            if (ImGui::Button("RESET", BUTTON_SIZE))
            {
                pthread_mutex_lock(&mutex);
                t_state = THREAD_EXITED;
                initLabels(&labels, &windowSize);
                pthread_mutex_unlock(&mutex);
                resultMsg[0] = 0;
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

            if (t_state == THREAD_EXITED)
            {
                pthread_join(thread_id, NULL);
                pthread_mutex_lock(&mutex);
                t_state = THREAD_INITIALIZED;
                pthread_mutex_unlock(&mutex);
            }
            
            if (t_state == THREAD_PAUSED)
            {
                ImGui::SameLine();
                ImGui::Text("\tPAUSED.\t"); 
            }

            if (show_config_window)
            {
                float blockedRatio = 0.3;
                ImGui::Begin("Another Window", &show_config_window, ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::InputInt("Num row", &nrow);
                ImGui::InputInt("Num col", &ncol);
                ImGui::InputFloat("Blocked ratio", &blockedRatio);
                if (ImGui::Button("Random grid", BUTTON_SIZE))
                {
                    windowSize.nrow = nrow;
                    windowSize.ncol = ncol;
                    show_config_window = false;
                    // TODO: Show warning in case blockedRatio is not in a satisfied range.
                    if (blockedRatio < 1.0f &&
                        blockedRatio >= 0.0f)
                        RandomGrid(&labels, &windowSize, blockedRatio);
                }                
                ImGui::SameLine();
                if (ImGui::Button("Close", BUTTON_SIZE))
                    show_config_window = false;

                ImGui::End();
            }



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
            // ImGui::PopStyleVar();
               
// ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();
// ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();
// ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();
// ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();
// ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            /* Continuously parse a float from slider in range of 0.1f to 500.0f */
            ImGui::SliderFloat("Steps/sec", &stepPerSecs, 0.1f, 500.0f);            

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

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
