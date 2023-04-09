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


extern int   sourceIdx, targetIdx;
extern float stepPerSecs;
extern int   max_width, max_height;
extern pthread_mutex_t mutex;

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
    // bool show_another_window = false;
    // ImVec4 clear_color = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);

    // Main loop
    bool done = false;
    ChoosingLabel choosingOpt = CHOOSE_UNBLOCKED;
    // 2. Set window size, archive from user input
    Grid windowSize;
    windowSize.nrow = 5;
    windowSize.ncol = 5;
        // TODO: calulate each box size according to the number of block and total windows size
    int blockSize = 40;
    
    BlockLabels *labels = NULL;
    initLabels(&labels, &windowSize);
    ThreadState t_state = THREAD_INITIALIZED;
    pthread_t thread_id;
    bool show_config_window = false;
    ThreadSearchingState shared;

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

        // ImGui::SetNextWindowSize(ImVec2(1200, 1000));
        ImGui::Begin("A star algo", NULL, ImGuiWindowFlags_AlwaysAutoResize);

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        reCalculateBlockSize(&windowSize, &blockSize);
        // 3. Show a window of size windowSize.
        {
            for (int rowIdx = 0; rowIdx < windowSize.nrow; rowIdx++)
            {
                for (int colIdx = 0; colIdx < windowSize.ncol; colIdx++)
                {
                    char name[16];
                    int idx = windowSize.ncol * rowIdx + colIdx;
                    bool itemSelected = false;
                    ImVec4 color;

                    sprintf(name, "##%d", idx); /* Label of item will be hidden if its name start with '##' .*/
                    if (colIdx > 0) ImGui::SameLine();
                    // TODO: border boxes
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));

                    if (idx == sourceIdx)
                    {
                        color = GREEN;
                        itemSelected = true;
                    }
                    else if (idx == targetIdx)
                    {
                        color = RED;
                        itemSelected = true;
                    }
                    else
                    {
                        switch (labels[idx])
                        {
                            case LBL_UNBLOCKED:
                                itemSelected = true;
                                break;
                            case LBL_BLOCKED:
                                color = GRAY;
                                itemSelected = true;
                                break;
                            case LBL_VISITED:
                                color = BLUE;
                                itemSelected = true;
                                break;
                            case LBL_VISITING:
                                color = YELLOW;
                                itemSelected = true;
                                break;
                            case LBL_TOBEVISITED:
                                color = LIGHTBLUE;
                                itemSelected = true;
                                break;
                            default:
                                break;
                        }
                    }

                    ImGui::PushStyleColor(ImGuiCol_Border, BLACK); /* Color of text */
                    ImGui::PushStyleColor(ImGuiCol_Header, color); /* Color of background */
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 10.0f);

                    // ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
                    ImGui::BeginChild((std::string(name) + "1").c_str(), ImVec2(blockSize, blockSize), true /* add border */, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav);
                    if (ImGui::Selectable(name, itemSelected, ImGuiSelectableFlags_None, ImVec2(blockSize, blockSize)))
                    {
                        /* Select a BLOCKED block unblock it. :) Confused enough? */
                        if (labels[idx] != LBL_UNBLOCKED)
                        {
                            pthread_mutex_lock(&mutex);
                            labels[idx] = LBL_UNBLOCKED;
                            /* If the block is SOURCE or TARGET, unselect it. */
                            if (sourceIdx == idx)
                                sourceIdx = -1;
                            else if (targetIdx == idx)
                                targetIdx = -1;
                            pthread_mutex_unlock(&mutex);
                        }
                        else /* The block is not BLOCKED? => Set the label of the block depending on the choosingOpt value. */
                        // TODO: blocked => target, source: OK
                        // but: target => unblocked => blocked
                        {
                            pthread_mutex_lock(&mutex);
                            switch (choosingOpt)
                            {
                                case CHOOSE_SOURCE:
                                    labels[sourceIdx] = LBL_UNBLOCKED;
                                    sourceIdx = idx;
                                    break;
                                case CHOOSE_TARGET:
                                    labels[targetIdx] = LBL_UNBLOCKED;
                                    targetIdx = idx;
                                    break;
                                case CHOOSE_BLOCKED:
                                    labels[idx] = LBL_BLOCKED;
                                    break;
                                case CHOOSE_UNBLOCKED:
                                    labels[idx] = LBL_UNBLOCKED;
                                    break;
                                default:
                                    break;
                            }   
                            pthread_mutex_unlock(&mutex);
                        }
                    }

                    ImGui::EndChild();
                    ImGui::PopStyleVar();

                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();
                    ImGui::PopStyleColor();
                }
            }
            
            ImGui::PushStyleColor(ImGuiCol_Button, GREEN);
            if (ImGui::Button("Choose Source"))
                choosingOpt = CHOOSE_SOURCE;

            ImGui::SameLine();
            if (ImGui::Button("Choose target"))
                choosingOpt = CHOOSE_TARGET;

            ImGui::SameLine();
            if (ImGui::Button("Choose blocked"))
                choosingOpt = CHOOSE_BLOCKED;
                
            if (ImGui::Button("Exec A Star") &&
                t_state != THREAD_RUNNING)
            {
                shared.labels = labels;
                shared.windowSize.nrow = windowSize.nrow;
                shared.windowSize.ncol = windowSize.ncol;
                t_state = THREAD_RUNNING;
                shared.state = &t_state;
                int err = pthread_create(&thread_id,
                                         NULL,
                                         execAStar,
                                         (void*) &shared);
                if(err != 0)
                {
                    /* Ask the child to stop */
                    t_state = THREAD_END;
                    /* now we can join the child before returning */
                    pthread_join(thread_id, NULL);
                    return 1; // Error occurs.
                }
            }
            if (t_state == THREAD_END)
            {
                pthread_join(thread_id, NULL);
                t_state = THREAD_INITIALIZED;
            }

            if (ImGui::Button("RESET"))
            {
                pthread_mutex_lock(&mutex);
                t_state = THREAD_END;
                initLabels(&labels, &windowSize);
                pthread_mutex_unlock(&mutex);
            }
            ImGui::SameLine();
            if (ImGui::Button("PAUSE/CONTINUE"))
            {
                pthread_mutex_lock(&mutex);
                if (t_state == THREAD_RUNNING)
                    t_state = THREAD_PAUSED;
                else if (t_state == THREAD_PAUSED)
                    t_state = THREAD_RUNNING;
                pthread_mutex_unlock(&mutex);
            }

            if (t_state == THREAD_PAUSED)
            {
                ImGui::SameLine();
                ImGui::Text("\tPAUSED.\t"); 
            }

            if (ImGui::Button("Random grid"))
                show_config_window = true;

            if (show_config_window)
            {
                float blockedRatio = 0.3;

                ImGui::Begin("Another Window", &show_config_window, ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::InputInt("Num row", &(windowSize.nrow));
                ImGui::InputInt("Num col", &(windowSize.ncol));
                ImGui::InputFloat("Blocked ratio", &blockedRatio);
                if (ImGui::Button("Random grid"))
                {
                    show_config_window = false;
                    // TODO: Show warning in case blockedRatio is not in a satisfied range.
                    if (blockedRatio < 1.0f &&
                        blockedRatio >= 0.0f)
                        RandomGrid(&labels, &windowSize, blockedRatio);
                }                
                ImGui::SameLine();
                if (ImGui::Button("Close"))
                    show_config_window = false;

                ImGui::End();
            }

            if (ImGui::Button("QUIT") || /* either the user click QUIT, or `x` on the corner with a `initialized` thread */
                (done &&
                    (t_state == THREAD_PAUSED ||
                     t_state == THREAD_RUNNING)))
            {
                /* Ask the child to stop */
                t_state = THREAD_END;// endThread = true;
                /* now we can join the child before quiting */
                pthread_join(thread_id, NULL);
                break;
            }
               
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
