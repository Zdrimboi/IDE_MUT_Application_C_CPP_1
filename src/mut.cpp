#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <shellscalingapi.h>
#include "platform_window.h"
#include "dpi_manager.h"
#include "gui_layer.h"
#include <imgui.h>

int main()
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (!glfwInit()) return -1;

    PlatformWindow window(1280, 720, "ImGui DPI Demo");
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    GuiLayer    gui;
    gui.init(window.glfw());

    ImGui::GetIO().UserData = new DpiManager(window.glfw()); // own the manager
    auto* dpi = static_cast<DpiManager*>(ImGui::GetIO().UserData);

    while (!window.shouldClose())
    {
        window.pollEvents();
        dpi->newFrame(ImGui::GetIO());                       // rebuild if needed
        int fbw, fbh; window.getFramebufferSize(fbw, fbh);
        ImGui::GetIO().DisplaySize = { fbw / dpi->scale(), fbh / dpi->scale() };

        gui.begin();
        gui.render();
        gui.end();

        window.swapBuffers();
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    gui.shutdown();
    delete dpi;
    glfwTerminate();
    return 0;
}
