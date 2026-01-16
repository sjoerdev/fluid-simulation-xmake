#include "input.h"

void Input::SetContext(GLFWwindow* win)
{
    window = win;
}

void Input::Update()
{
    keyPrevious = keyCurrent;
    mousePrevious = mouseCurrent;
    for (auto& [key, state] : keyPrevious) keyCurrent[key] = glfwGetKey(window, key) == GLFW_PRESS;
    for (auto& [button, state] : mousePrevious) mouseCurrent[button] = glfwGetMouseButton(window, button) == GLFW_PRESS;
}

bool Input::IsKeyDownThisFrame(int key)
{
    return keyCurrent[key] && !keyPrevious[key];
}

bool Input::IsKeyHeldDown(int key)
{
    return keyCurrent[key];
}

bool Input::IsMouseButtonHeldDown(int button)
{
    return mouseCurrent[button];
}

glm::vec2 Input::GetMousePosition()
{
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    return glm::vec2(float(xpos), float(ypos));
}