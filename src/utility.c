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

void ReaderWriterLockMutex_Init(ReaderWriterLockMutex* lock)
{
	memset(lock, 0, sizeof(ReaderWriterLockMutex));

	lock->num_readers = 0;

	InitializeCriticalSection(&lock->write_mutex);
	InitializeCriticalSection(&lock->reader_count_mutex);

	lock->readers_cleared_event = CreateEvent(NULL, TRUE, TRUE, NULL);
}

void ReaderWriterLockMutex_Destruct(ReaderWriterLockMutex* lock)
{
	WaitForSingleObject(lock->readers_cleared_event, INFINITE);

	CloseHandle(lock->readers_cleared_event);
	DeleteCriticalSection(&lock->write_mutex);
	DeleteCriticalSection(&lock->reader_count_mutex);
}

void ReaderWriterLockMutex_EnterRead(ReaderWriterLockMutex* lock)
{
	EnterCriticalSection(&lock->write_mutex);
	EnterCriticalSection(&lock->reader_count_mutex);

	if (++lock->num_readers == 1)
	{
		ResetEvent(lock->readers_cleared_event);
	}
	
	LeaveCriticalSection(&lock->reader_count_mutex);
	LeaveCriticalSection(&lock->write_mutex);
}
void ReaderWriterLockMutex_ExitRead(ReaderWriterLockMutex* lock)
{
	EnterCriticalSection(&lock->reader_count_mutex);

	if (--lock->num_readers == 0)
	{
		SetEvent(lock->readers_cleared_event);
	}

	LeaveCriticalSection(&lock->reader_count_mutex);
}

void ReaderWriterLockMutex_EnterWrite(ReaderWriterLockMutex* lock)
{
	EnterCriticalSection(&lock->write_mutex);
	WaitForSingleObject(lock->readers_cleared_event, INFINITE);
}

void ReaderWriterLockMutex_ExitWrite(ReaderWriterLockMutex* lock)
{
	LeaveCriticalSection(&lock->write_mutex);
}

int QueryNumLogicalProcessors()
{
	//src https://stackoverflow.com/a/52716113

	int concurrency = 0;
	DWORD length = 0;
	//query buffer size
	if (GetLogicalProcessorInformationEx(RelationAll, NULL, &length) != FALSE)
	{
		return 0;
	}
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{
		return 0;
	}

	unsigned char* buffer = calloc(length, sizeof(void*));

	if (!buffer)
	{
		return 0;
	}

	if (GetLogicalProcessorInformationEx(RelationAll, buffer, &length) == FALSE)
	{
		return 0;
	}

	for (DWORD i = 0; i < length;)
	{
		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX proc = &buffer[i];

		if (proc->Size <= 0)
		{
			break;
		}

		if (proc->Relationship == RelationProcessorCore)
		{
			for (WORD group = 0; group < proc->Processor.GroupCount; group++)
			{
				for (KAFFINITY mask = proc->Processor.GroupMask[group].Mask; mask != 0; mask >>= 1)
				{
					concurrency += mask & 1;
				}
			}
		}

		i += proc->Size;
	}


	free(buffer);

	return concurrency;
}
