#ifndef SOUND_H
#define SOUND_H
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "miniaudio/miniaudio.h"

#define MAX_SOUNDS 1024

typedef int16_t SoundID;

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
void Sound_EmitWorldTemp(int type, float x, float y, float z, float dir_x, float dir_y, float dir_z, float vol);
SoundID Sound_EmitWorld(int type, float x, float y, float z, float dir_x, float dir_y, float dir_z, float vol);
void Sound_SetTransform(SoundID id, float x, float y, float z, float dir_x, float dir_y, float dir_z);
void Sound_SetRolloff(SoundID id, float rolloff);
void Sound_SetVolume(SoundID id, float vol);
void Sound_Play(SoundID id);
void Sound_Stop(SoundID id);
void Sound_Stream(int type);
void Sound_SetAsMusic(int type);
void Sound_FlushAll();

#endif