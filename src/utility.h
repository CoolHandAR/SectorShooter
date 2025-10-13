#ifndef UTIL_H
#define UTIL_H
#pragma once

#include <stdio.h>

int File_GetLength(FILE* p_file);
unsigned char* File_Parse(const char* p_filePath, int* r_length);

#endif