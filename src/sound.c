#include "sound.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"

#include "game_info.h"

typedef struct
{
	ma_engine sound_engine;
	Sound sounds[MAX_SOUNDS];
	int free_list[MAX_SOUNDS];
	int num_free_list;
	int num_sounds;
	int max_index;
	float master_volume;

	ma_sound music_snd;
} SoundCore;

static SoundCore sound_core;

static void Sound_FreeListStoreID(int id)
{
	if (sound_core.num_free_list >= MAX_SOUNDS)
	{
		return;
	}

	sound_core.free_list[sound_core.num_free_list++] = id;
}

static int Sound_GetNewIndex()
{
	int id = -1;

	if (sound_core.num_free_list > 0)
	{
		id = sound_core.free_list[sound_core.num_free_list - 1];
		sound_core.num_free_list--;
	}
	else if(sound_core.num_sounds < MAX_SOUNDS)
	{
		id = sound_core.num_sounds++;
	}
	else
	{
		for (int i = 0; i < sound_core.num_sounds; i++)
		{
			Sound* snd = &sound_core.sounds[i];

			if (snd->fire_and_forget && snd->alive)
			{
				if (ma_sound_at_end(&snd->snd) && !ma_sound_is_playing(&snd->snd))
				{
					Sound_DeleteSound(i);
				}
			}
		}
		if (sound_core.num_free_list > 0)
		{
			id = sound_core.free_list[sound_core.num_free_list - 1];
			sound_core.num_free_list--;
		}


	}
	sound_core.max_index = max(sound_core.max_index, id);

	return id;
}


static void Sound_FreeUnusedSounds()
{
	int num_sounds = 0;

	for (int i = 0; i < sound_core.max_index; i++)
	{
		Sound* snd = &sound_core.sounds[i];

		if (snd->fire_and_forget && snd->alive)
		{
			if (!ma_sound_at_end(&snd->snd))
			{
				Sound_DeleteSound(i);
				continue;
			}
		}
		
		if (i > num_sounds)
		{
			num_sounds = i;
		}
	}

	//sound_core.num_sounds = num_sounds;
	
}

static Sound* Sound_NewSound(int* r_index)
{
	int index = Sound_GetNewIndex();

	if (index == -1)
	{
		return NULL;
	}

	Sound* sound = &sound_core.sounds[index];
	
	if (r_index)
	{
		*r_index = index;
	}

	sound->alive = false;
	sound->fire_and_forget = false;
	sound->type = 0;

	return sound;
}

static Sound* Sound_IDToSound(int id)
{
	if (id < 0 || id >= MAX_SOUNDS)
	{
		return NULL;
	}

	Sound* sound = &sound_core.sounds[id];

	return sound;
}

void Sound_DeleteSound(int id)
{
	Sound* sound = &sound_core.sounds[id];

	if (sound->alive)
	{
		ma_sound_uninit(&sound->snd);
	}

	memset(sound, 0, sizeof(Sound));

	Sound_FreeListStoreID(id);
}

ma_engine* Sound_GetEngine()
{
	return &sound_core.sound_engine;
}
bool Sound_load(const char* p_filePath, unsigned p_flags, ma_sound* r_sound)
{
	ma_result result;

	result = ma_sound_init_from_file(&sound_core.sound_engine, p_filePath, p_flags, NULL, NULL, r_sound);

	if (result != MA_SUCCESS)
	{
		printf("Failed to load sound file at path %s \n", p_filePath);
		return false;
	}

	return true;
}

bool Sound_createGroup(unsigned p_flags, ma_sound_group* r_group)
{
	ma_result result;

	result = ma_sound_group_init(&sound_core.sound_engine, p_flags, NULL, r_group);

	if (result != MA_SUCCESS)
	{
		printf("Failed to create sound group");
		return false;
	}

	return true;
}

void Sound_setMasterVolume(float volume)
{
	if (volume < 0)
	{
		volume = 0;
	}

	sound_core.master_volume = volume;
	ma_engine_set_volume(&sound_core.sound_engine, volume);
}

float Sound_GetMasterVolume()
{
	return sound_core.master_volume;
}

void Sound_EmitWorldTemp(int type, float x, float y, float dir_x, float dir_y)
{
	if (type < 0 || type >= SOUND__MAX)
	{
		return;
	}

	const char* sound_file = SOUND_INFO[type];

	if (!sound_file)
	{
		return;
	}

	Sound* snd = Sound_NewSound(NULL);

	if (!snd)
	{
		return;
	}

	snd->type = type;
	snd->fire_and_forget = true;
	snd->alive = true;
	
	Sound_load(sound_file, MA_SOUND_FLAG_NO_PITCH, &snd->snd);
	
	ma_sound_set_position(&snd->snd, x, 0, y);
	ma_sound_set_direction(&snd->snd, dir_x, 0, dir_y);
	ma_sound_start(&snd->snd);
}

int Sound_EmitWorld(int type, float x, float y, float dir_x, float dir_y)
{
	if (type < 0 || type >= SOUND__MAX)
	{
		return - 1;
	}

	const char* sound_file = SOUND_INFO[type];

	if (!sound_file)
	{
		return -1;
	}

	int id = 0;

	Sound* snd = Sound_NewSound(&id);

	if (!snd)
	{
		return -1;
	}

	snd->alive = true;
	snd->type = type;

	Sound_load(sound_file, MA_SOUND_FLAG_NO_PITCH, &snd->snd);

	ma_sound_set_position(&snd->snd, x, 0, y);
	ma_sound_set_direction(&snd->snd, dir_x, 0, dir_y);
	ma_sound_start(&snd->snd);

	return id;
}

int Sound_Preload(int type)
{
	if (type < 0 || type >= SOUND__MAX)
	{
		return -1;
	}

	const char* sound_file = SOUND_INFO[type];

	if (!sound_file)
	{
		return -1;
	}

	int id = 0;

	Sound* snd = Sound_NewSound(&id);

	if (!snd)
	{
		return -1;
	}

	Sound_load(sound_file, 0, &snd->snd);

	return id;
}

void Sound_Emit(int type, float volume)
{

	if (type < 0 || type >= SOUND__MAX)
	{
		return;
	}

	const char* sound_file = SOUND_INFO[type];

	if (!sound_file)
	{
		return;
	}

	Sound* snd = Sound_NewSound(NULL);

	if (!snd)
	{
		return;
	}

	snd->type = type;
	snd->fire_and_forget = true;
	snd->alive = true;

	Sound_load(sound_file, MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH, &snd->snd);
	
	ma_sound_set_volume(&snd->snd, volume);
	ma_sound_start(&snd->snd);
}

void Sound_Set(int id, float x, float y, float dir_x, float dir_y)
{
	Sound* snd = Sound_IDToSound(id);

	if (!snd)
	{
		return;
	}

	ma_sound_set_position(&snd->snd, x, 0, y);
	ma_sound_set_direction(&snd->snd, dir_x, 0, dir_y);
}

void Sound_Play(int id)
{
	Sound* snd = Sound_IDToSound(id);

	if (!snd)
	{
		return;
	}

	ma_sound_start(&snd->snd);
}

void Sound_Stop(int id)
{
	Sound* snd = Sound_IDToSound(id);

	if (!snd)
	{
		return;
	}

	ma_sound_stop(&snd->snd);
}

void Sound_Stream(int type)
{
	if (type < 0 || type >= SOUND__MAX)
	{
		return;
	}

	const char* sound_file = SOUND_INFO[type];

	if (!sound_file)
	{
		return;
	}

	Sound* snd = Sound_NewSound(NULL);

	if (!snd)
	{
		return;
	}

	snd->type = type;
	snd->alive = true;

	Sound_load(sound_file, MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_STREAM, &snd->snd);

	ma_sound_set_looping(&snd->snd, true);
	ma_sound_start(&snd->snd);
}

void Sound_SetAsMusic(int type)
{
	if (type < 0 || type >= SOUND__MAX)
	{
		return;
	}

	const char* sound_file = SOUND_INFO[type];

	if (!sound_file)
	{
		return;
	}

	Sound_load(sound_file, MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH, &sound_core.music_snd);

	ma_sound_set_looping(&sound_core.music_snd, true);
	ma_sound_start(&sound_core.music_snd);
}

int Sound_Init()
{
	memset(&sound_core, 0, sizeof(sound_core));

	ma_result result;

	result = ma_engine_init(NULL, &sound_core.sound_engine);

	if (result != MA_SUCCESS)
	{
		printf("Failed to init MA sound engine \n");
		return 0;
	}

	return 1;
}

void Sound_Shutdown()
{
	ma_engine_uninit(&sound_core.sound_engine);
}
