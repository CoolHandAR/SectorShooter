#ifndef MAIN_H
#define MAIN_H
#pragma once

#include <stdint.h>
#include <GLFW/glfw3.h>

uint64_t Engine_GetTicks();
float Engine_GetDeltaTime();
void Engine_SetTimeScale(float scale);
GLFWwindow* Engine_GetWindow();

#endif // !MAIN_H
