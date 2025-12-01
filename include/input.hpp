#include <GLFW/glfw3.h>
#include <unordered_map>

struct InputState
{
    GLFWwindow* window;

    std::unordered_map<int, bool> keyCurrent;
    std::unordered_map<int, bool> keyPrevious;
    std::unordered_map<int, bool> mouseCurrent;
    std::unordered_map<int, bool> mousePrevious;

    void SetContext(GLFWwindow* w)
    {
        window = w;
    }

    void Update()
    {
        // store previous state
        keyPrevious = keyCurrent;
        mousePrevious = mouseCurrent;

        // update current state
        for (auto& [key, state] : keyPrevious) keyCurrent[key] = glfwGetKey(window, key) == GLFW_PRESS;
        for (auto& [button, state] : mousePrevious) mouseCurrent[button] = glfwGetMouseButton(window, button) == GLFW_PRESS;
    }

    bool IsKeyDownThisFrame(int key)
    {
        return keyCurrent[key] && !keyPrevious[key];
    }

    bool IsKeyHeldDown(int key)
    {
        return keyCurrent[key];
    }

    bool IsMouseButtonHeldDown(int button)
    {
        return mouseCurrent[button];
    }

    glm::vec2 GetMousePosition()
    {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        return glm::vec2(float(xpos), float(ypos));
    }
};