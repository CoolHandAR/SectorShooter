#ifndef SOUND_H
#define SOUND_H
#pragma once

#include <stdbool.h>
#include "miniaudio/miniaudio.h"

#define MAX_SOUNDS 1024

typedef struct
{
	bool alive;
	bool fire_and_forget;
	ma_sound snd;
	int type;
} Sound;

int Sound_Init();
void Sound_Shutdown();

ma_engine* Sound_GetEngine();
bool Sound_DeleteSound(int id);
bool Sound_load(const char* p_filePath, unsigned p_flags, ma_sound* r_sound);
bool Sound_createGroup(unsigned p_flags, ma_sound_group* r_group);
void Sound_setMasterVolume(float volume);
float Sound_GetMasterVolume();
int Sound_Preload(int type);
void Sound_Emit(int type, float volume);
void Sound_EmitWorldTemp(int type, float x, float y, float dir_x, float dir_y);
int Sound_EmitWorld(int type, float x, float y, float dir_x, float dir_y);
void Sound_Set(int id, float x, float y, float dir_x, float dir_y);
void Sound_Play(int id);
void Sound_Stop(int id);
void Sound_Stream(int type);
void Sound_SetAsMusic(int type);

#endif