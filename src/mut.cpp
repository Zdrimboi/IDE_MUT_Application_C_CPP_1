// mut.cpp : Defines the entry point for the application.

#include <iostream>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <glfw3.h>
#include <imgui_impl_glfw.h>
#include <GL/gl.h>

#define WINDOW_WIDTH		1280
#define WINDOW_HEIGHT		720

//glfw window object pointer
static GLFWwindow* window = nullptr;

// used to report errors
static void GLFWErrorCallback(int error, const char* description) 
{
	std::cout << "GLFW Error " << description << "code: " << error << "\n";
}

int main() 
{
	//Initialize glfw window
	if (!glfwInit()) 
	{
		std::cout << "Failed to initialize GLFW!\n";
		std::cin.get();
	}

	//appoints the function used to report errors
	glfwSetErrorCallback(GLFWErrorCallback);

	//creating and appointing window
	window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "ImGui", nullptr, nullptr);

	//making the current glfw context the created window
	glfwMakeContextCurrent(window);

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable docking


	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	ImVec4 clearcolor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Hello World");
		ImGui::Text("text");
		ImGui::End();

		ImGui::Begin("Hello World2");
		ImGui::Button("button");
		ImGui::Text("text2");
		ImGui::End();

		ImGui::Render();
		int display_width, display_height;
		glfwGetFramebufferSize(window, &display_width, &display_height);
		glViewport(0, 0, display_width, display_height);
		glClearColor(clearcolor.x * clearcolor.w, clearcolor.y * clearcolor.w, clearcolor.z * clearcolor.w, clearcolor.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);

	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	if (window) 
	{
		glfwDestroyWindow(window);
	}
	glfwTerminate();

}