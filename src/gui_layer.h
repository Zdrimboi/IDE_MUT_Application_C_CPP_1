// gui_layer.h
#pragma once
class GuiLayer {
public:
    void init(void* glfwWindow);
    void begin();
    void render();         // draw actual widgets
    void end();            // submit draw data
    void shutdown();
};
