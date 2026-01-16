#ifndef INPUT_STATE_H
#define INPUT_STATE_H

#include <GLFW/glfw3.h>
#include <unordered_map>
#include <glm/glm.hpp>

struct Input
{
    GLFWwindow* window;

    std::unordered_map<int, bool> keyCurrent;
    std::unordered_map<int, bool> keyPrevious;
    std::unordered_map<int, bool> mouseCurrent;
    std::unordered_map<int, bool> mousePrevious;

    void SetContext(GLFWwindow* win);
    void Update();
    bool IsKeyDownThisFrame(int key);
    bool IsKeyHeldDown(int key);
    bool IsMouseButtonHeldDown(int button);
    glm::vec2 GetMousePosition();
};

#endif // INPUT_STATE_H