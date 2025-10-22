#ifndef UTIL_H
#define UTIL_H
#pragma once

#include <stdio.h>
#include <Windows.h>

int File_GetLength(FILE* p_file);
unsigned char* File_Parse(const char* p_filePath, int* r_length);

//src https://web.archive.org/web/20250118080332/http://www.glennslayden.com/code/win32/reader-writer-lock
typedef struct
{
	CRITICAL_SECTION write_mutex;
	CRITICAL_SECTION reader_count_mutex;
	
	HANDLE readers_cleared_event;

	int num_readers;
} ReaderWriterLockMutex;

void ReaderWriterLockMutex_Init(ReaderWriterLockMutex* lock);
void ReaderWriterLockMutex_Destruct(ReaderWriterLockMutex* lock);
void ReaderWriterLockMutex_EnterRead(ReaderWriterLockMutex* lock);
void ReaderWriterLockMutex_ExitRead(ReaderWriterLockMutex* lock);
void ReaderWriterLockMutex_EnterWrite(ReaderWriterLockMutex* lock);
void ReaderWriterLockMutex_ExitWrite(ReaderWriterLockMutex* lock);

int QueryNumLogicalProcessors();

#endif