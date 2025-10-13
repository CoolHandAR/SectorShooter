#include "utility.h"

#include <string.h>
#include <stdlib.h>

int File_GetLength(FILE* p_file)
{
	int pos;
	int end;

	pos = ftell(p_file);
	fseek(p_file, 0, SEEK_END);
	end = ftell(p_file);
	fseek(p_file, pos, SEEK_SET);

	return end;
}

unsigned char* File_Parse(const char* p_filePath, int* r_length)
{
	FILE* file = NULL;

	fopen_s(&file, p_filePath, "rb"); //use rb because otherwise it can cause reading issues
	if (!file)
	{
		printf("Failed to open file for parsing at path: %s!\n", p_filePath);
		return NULL;
	}
	int file_length = File_GetLength(file);
	unsigned char* buffer = malloc(file_length + 1);
	if (!buffer)
	{
		return NULL;
	}
	memset(buffer, 0, file_length + 1);
	fread_s(buffer, file_length, 1, file_length, file);

	//CLEAN UP
	fclose(file);

	if (r_length)
	{
		*r_length = file_length;
	}

	return buffer;
}