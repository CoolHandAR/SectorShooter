#include "g_common.h"

#include <string.h>
#include <math.h>

#include "sound.h"
#include "game_info.h"
#include "main.h"
#include "u_math.h"

#define TRACE_DIST 1024
#define HIT_TIME 0.1
#define USE_TIME 0.7
#define SAVE_TIME 2
#define PLAYER_MAX_HP 100
#define PLAYER_MAX_AMMO 100
#define PLAYER_OVERHEAL_HP_TICK_TIME 0.25
#define PLAYER_SPEED 3
#define MOUSE_SENS_DIVISOR 1000
#define MIN_SENS 0.5
#define MAX_SENS 16
#define Z_VIEW_LERP 25
#define CAMERA_SHAKE_SCALE 1
#define CAMERA_SHAKE_TIME_SCALE 1
#define GUN_FLASH_SCALE 0.55

static PlayerData player;

static void Player_GiveAll()
{
	memset(player.gun_check, true, sizeof(player.gun_check));

	player.buck_ammo = PLAYER_MAX_AMMO;
	player.bullet_ammo = PLAYER_MAX_AMMO;
	player.rocket_ammo = PLAYER_MAX_AMMO;
}

static void Player_TraceBloodSplat(Object* hit_obj, float hit_x, float hit_y, float hit_z, float p_dir_x, float p_dir_y, float dir_z)
{
	Decal_BloodTrace(hit_obj, hit_x, hit_y, hit_z, p_dir_x, p_dir_y, dir_z);
}

static void Player_TraceBullet(float p_x, float p_y, float p_dirX, float p_dirY)
{
	float frac = 0;
	float inter_x = 0;
	float inter_y = 0;
	float inter_z = 0;

	p_dirX *= TRACE_DIST;
	p_dirY *= TRACE_DIST;

	int hit = Trace_AttackLine(player.obj, p_x, p_y, p_x + p_dirX, p_y + p_dirY, player.obj->z + player.obj->height, TRACE_DIST, &inter_x, &inter_y, &inter_z, &frac);

	if (hit == TRACE_NO_HIT)
	{
		return;
	}

	float origin_x = inter_x;
	float origin_y = inter_y;
	float origin_z = inter_z;

	float dx = (p_x + p_dirX) - p_x;
	float dy = (p_y + p_dirY) - p_y;

	frac -= 1.0 / 127.0;

	inter_x = p_x + dx * frac;
	inter_y = p_y + dy * frac;
	
	//hit a an object
	if (hit >= 0)
	{
		Object* hit_obj = Map_GetObject(hit);
		GunInfo* gun_info = player.gun_info;

		int dmg = gun_info->damage;
		float dist = Math_XY_Distance(p_x, p_y, hit_obj->x, hit_obj->y);

		//way too far away
		if (dist >= TRACE_DIST)
		{
			return;
		}

		//modify damage based on distance
		if (dist <= 5)
		{
			dmg *= 4;
		}
		else if (dist <= 10)
		{
			dmg *= 2;
		}

		//add some rng
		dmg += Math_rand() % 10;

		//check for quad
		if (player.quad_timer > 0)
		{
			dmg *= 4;
		}

		//spawn bullet impact particle
		if (rand() % 100 > 25)
		{
			Object_Spawn(OT__PARTICLE, SUB__PARTICLE_BULLET_IMPACT, (inter_x)+(Math_randf() * 0.5), (inter_y)+(Math_randf() * 0.5), inter_z + (Math_randf() * 0.5));
		}

		//spawn blood particles
		if (rand() % 100 > 25)
		{
			Object_Spawn(OT__PARTICLE, SUB__PARTICLE_BLOOD, (inter_x) + (Math_randf() * 0.5), (inter_y) + (Math_randf() * 0.5), inter_z + (Math_randf() * 0.5));
		}

		//spawn blood decal behing the hit object
		if (rand() % 100 > 25)
		{
			Player_TraceBloodSplat(hit_obj, inter_x, inter_y, inter_z, p_dirX, p_dirY, inter_z - (player.obj->z + player.obj->height));
		}

		if (hit_obj->hp > 0)
		{
			player.hit_timer = HIT_TIME;
		}

		Object_Hurt(hit_obj, player.obj, dmg, false);
	}
	else
	{
		//hit a wall
		//spawn wall dust
		Object_Spawn(OT__PARTICLE, SUB__PARTICLE_WALL_HIT, inter_x, inter_y, inter_z);

		//spawn bullet hole decal
		Object* decal = Object_Spawn(OT__DECAL, SUB__DECAL_WALL_HIT, origin_x, origin_y, origin_z);

		if(decal) decal->sprite.decal_line_index = -(hit + 1);
	}

	Monster_WakeAll(player.obj);
}
static void Player_ShootMissile(float p_x, float p_y, float p_dirX, float p_dirY)
{
	float frac = 0;
	float inter_x = 0;
	float inter_y = 0;
	float inter_z = 0;

	p_dirX *= TRACE_DIST;
	p_dirY *= TRACE_DIST;

	int hit = TRACE_NO_HIT;

	static int PREV_HIT = TRACE_NO_HIT;

	for (int i = -5; i < 5; i++)
	{
		float angle = player.angle - Math_DegToRad(2) * i;

		hit = Trace_AttackLine(player.obj, p_x, p_y, p_x + (cos(angle) * TRACE_DIST), p_y + (sin(angle) * TRACE_DIST), player.obj->z + player.obj->height, TRACE_DIST, &inter_x, &inter_y, &inter_z, &frac);

		if (hit != TRACE_NO_HIT && hit != PREV_HIT && hit >= 0)
		{
			Object* trace_obj = Map_GetObject(hit);

			if (trace_obj->type == OT__MONSTER && trace_obj->hp > 0)
			{
				break;
			}
		}
	}
	if (hit == TRACE_NO_HIT || hit < 0)
	{
		Object* missile = Object_Missile(player.obj, NULL, SUB__MISSILE_MEGASHOT);
	}
	else
	{
		Object* trace_obj = Map_GetObject(hit);

		if (trace_obj->type == OT__MONSTER)
		{
			Object* missile = Object_Missile(player.obj, trace_obj, SUB__MISSILE_MEGASHOT);

			PREV_HIT = hit;
		}
		
	}

	Monster_WakeAll(player.obj);
}

static void Player_Use()
{
	if (player.use_timer > 0)
	{
		return;
	}

	const float check_range = 45;
	int hit = Trace_FindSpecialLine(player.obj->x, player.obj->y, player.obj->x + (player.obj->dir_x * check_range), player.obj->y + (player.obj->dir_y * check_range), player.obj->z + player.obj->height);

	//no special line was found
	if (hit == TRACE_NO_HIT || hit >= 0)
	{
		return;
	}

	Linedef* special_line = Map_GetLineDef(-(hit + 1));
	
	Event_TriggerSpecialLine(player.obj, 0, special_line, EVENT_TRIGGER__LINE_USE);

	player.use_timer = USE_TIME;
}
static void Player_ShootGun()
{
	if (player.gun_timer > 0)
	{
		return;
	}
	if (player.obj->hp <= 0)
	{
		return;
	}
	if (!player.gun_info)
	{
		return;
	}

	float dir_x = cosf(player.angle);
	float dir_y = sinf(player.angle);

	bool no_ammo = false;

	switch (player.gun)
	{
	case GUN__PISTOL:
	{
		//has infinite ammo
		float randomf = Math_randf();

		randomf = Math_Clamp(randomf, 0.0, 0.01);

		if (rand() % 100 > 50)
		{
			randomf = -randomf;
		}

		Player_TraceBullet(player.obj->x, player.obj->y, dir_x + randomf, dir_y + randomf);

		break;
	}
	case GUN__MACHINEGUN:
	{
		//no ammo
		if (player.bullet_ammo <= 0)
		{
			no_ammo = true;
			break;
		}

		float randomf = Math_randf();

		randomf = Math_Clamp(randomf, 0.0, 0.05);

		if (rand() % 100 > 50)
		{
			randomf = -randomf;
		}

		Player_TraceBullet(player.obj->x, player.obj->y, dir_x + randomf, dir_y + randomf);

		player.bullet_ammo--;

		break;
	}
	case GUN__SHOTGUN:
	{
		//no ammo
		if (player.buck_ammo <= 0)
		{
			no_ammo = true;
			break;
		}

		for (int i = 0; i < 8; i++)
		{
			float randomf = Math_randf();

			randomf = Math_Clamp(randomf, 0.0, 0.05);

			if (rand() % 100 > 50)
			{
				randomf = -randomf;
			}

			Player_TraceBullet(player.obj->x, player.obj->y, dir_x + randomf, dir_y + randomf);
		}

		player.buck_ammo--;
		break;
	}
	case GUN__DEVASTATOR:
	{
		//no ammo
		if (player.rocket_ammo <= 0)
		{
			no_ammo = true;
			break;
		}

		Player_ShootMissile(player.obj->x, player.obj->y, dir_x, dir_y);
		
		player.rocket_ammo--;
		break;
	}
	default:
		break;
	}

	if (no_ammo)
	{
		Sound_Emit(SOUND__NO_AMMO, 0.025);
		player.gun_timer = 0.5;
		return;
	}


	GunInfo* gun_info = player.gun_info;
	player.obj->flags |= OBJ_FLAG__JUST_ATTACKED;
	player.gun_timer = gun_info->cooldown;
	player.shake_timer = gun_info->shake_time * CAMERA_SHAKE_TIME_SCALE;
	player.shake_scale = gun_info->shake_scale * CAMERA_SHAKE_SCALE;

	//PLAY SOUND
	int sound_index = -1;
	float volume = 1.0;

	switch (player.gun)
	{
	case GUN__PISTOL:
	{
		volume = 0.004;
		sound_index = SOUND__PISTOL_SHOOT;
		break;
	}
	case GUN__SHOTGUN:
	{
		volume = 0.015;
		sound_index = SOUND__SHOTGUN_SHOOT;
		break;
	}
	case GUN__MACHINEGUN:
	{
		volume = 0.01;
		sound_index = SOUND__MACHINEGUN_SHOOT;
		break;
	}
	case GUN__DEVASTATOR:
	{
		volume = 0.012;
		sound_index = SOUND__DEVASTATOR_SHOOT;
		break;
	}
	default:
		break;
	}

	if (sound_index >= 0)
	{
		Sound_Emit(sound_index, volume);
	}
}

static void Player_Die()
{

}

static void Player_UpdateTimers(float delta)
{
	if (player.obj->hp > 0) player.alive_timer += delta;
	if (player.gun_timer > 0) player.gun_timer -= delta;
	if (player.hurt_timer > 0) player.hurt_timer -= delta;
	if (player.quad_timer > 0) player.quad_timer -= delta;
	if (player.hp_tick_timer > 0) player.hp_tick_timer -= delta;
	if (player.hit_timer > 0) player.hit_timer -= delta;
	if (player.use_timer > 0) player.use_timer -= delta;
	if (player.save_timer > 0) player.save_timer -= delta;
	if (player.shake_timer > 0) player.shake_timer -= delta;
	if (player.menu_timer > 0) player.menu_timer -= delta;

	if (player.godmode_timer > 0)
	{
		player.godmode_timer -= delta;
		player.obj->flags |= OBJ_FLAG__GODMODE;
	}
	else
	{
		player.obj->flags &= ~OBJ_FLAG__GODMODE;
	}
}
static void Player_UpdateListener()
{
	ma_engine* sound_engine = Sound_GetEngine();

	ma_engine_listener_set_position(sound_engine, 0, player.obj->x, player.obj->y, player.obj->z);
	ma_engine_listener_set_direction(sound_engine, 0, player.obj->dir_x, player.obj->dir_y, 0);
}

static void Player_ProcessInput(GLFWwindow* window)
{
	int move_x = 0;
	int move_y = 0;
	int slow_move = 0;

	//movement
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{
		move_x = 1;
	}
	else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		move_x = -1;
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		move_y = -1;
	}
	else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		move_y = 1;
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
	{
		slow_move = 1;
	}

	//gun input
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
	{
		if (player.obj->hp > 0)
		{
			Player_ShootGun();
		}
		else if(player.hurt_timer <= 1.0)
		{
			Game_Reset(false);
		}	
	}
	//gun stuff
	if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
	{
		Player_SetGun(GUN__PISTOL);
	}
	else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
	{
		Player_SetGun(GUN__SHOTGUN);
	}
	else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
	{
		Player_SetGun(GUN__MACHINEGUN);
	}
	else if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS)
	{
		Player_SetGun(GUN__DEVASTATOR);
	}
	if (player.save_timer <= 0)
	{
		if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS)
		{
			Game_Save(0, "QUICKSAVE");
			player.save_timer = SAVE_TIME;
		}
		else if (glfwGetKey(window, GLFW_KEY_F9) == GLFW_PRESS)
		{
			Game_Load(0);
			player.save_timer = SAVE_TIME;
		}
	}
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
	{
		Player_Use();
	}

	if (player.menu_timer <= 0)
	{
		if (glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS)
		{
			Render_ToggleFullscreen();
			player.menu_timer = 1;
		}
	}
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		Game_SetState(GS__MENU);
	}

	player.move_x = move_x;
	player.move_y = move_y;
	player.slow_move = slow_move;
}

static void Player_SetupGunSprites()
{
	GameAssets* assets = Game_GetAssets();

	for (int i = 1; i < GUN__MAX; i++)
	{
		Sprite* sprite = &player.gun_sprites[i];
		GunInfo* gun_info = Info_GetGunInfo(i);

		if (!gun_info)
		{
			continue;
		}

		switch (i)
		{
		case GUN__SHOTGUN:
		{
			sprite->img = &assets->shotgun_texture;
			sprite->anim_speed_scale = 2;
			break;
		}
		case GUN__PISTOL:
		{
			sprite->img = &assets->pistol_texture;
			sprite->anim_speed_scale = 1;
			break;
		}
		case GUN__MACHINEGUN:
		{
			sprite->img = &assets->machinegun_texture;
			sprite->anim_speed_scale = 1;
			break;
		}
		case GUN__DEVASTATOR:
		{
			sprite->img = &assets->devastator_texture;
			sprite->anim_speed_scale = 1;
			break;
		}
		default:
			break;
		}

		sprite->frame_count = sprite->img->h_frames * sprite->img->v_frames;
		sprite->scale_x = gun_info->scale;
		sprite->scale_y = gun_info->scale;
		sprite->x = gun_info->screen_x;
		sprite->y = gun_info->screen_y;
		sprite->action_frame = gun_info->light_frame;
	}
}
static void Player_HandleHurt()
{
	if (player.obj->hp <= 0)
	{
		player.hurt_timer = 2;
		Sound_Emit(SOUND__PLAYER_DIE, 0.035);
	}
	else
	{
		if (player.hurt_timer <= 0)
		{
			Sound_Emit(SOUND__PLAYER_PAIN, 0.035);
		}

		player.hurt_timer = 0.5;
	}
}


void Player_Init(int keep)
{
	int hp = 100;
	float sens = player.sensitivity;
	bool guns_check[GUN__MAX];
	int buck_ammo = player.buck_ammo;
	int bullet_ammo = player.bullet_ammo;
	int gun = player.gun;

	if (keep)
	{
		memcpy(guns_check, player.gun_check, sizeof(guns_check));
		hp = player.stored_hp;
	}

	memset(&player, 0, sizeof(player));
	
	int spawn_x, spawn_y, spawn_z, spawn_sector;
	float spawn_rot;
	Map_GetSpawnPoint(&spawn_x, &spawn_y, &spawn_z, &spawn_sector, &spawn_rot);

	if (keep)
	{
		memcpy(player.gun_check, guns_check, sizeof(guns_check));
		player.bullet_ammo = bullet_ammo;
		player.buck_ammo = buck_ammo;
		Player_SetGun(gun);
	}

	player.gun_check[GUN__PISTOL] = true;

	player.obj = Object_Spawn(OT__PLAYER, 0, spawn_x, spawn_y, 0);

	player.obj->hp = hp;
	player.sensitivity = sens;
	player.view_x = player.obj->x;
	player.view_y = player.obj->y;
	player.view_z = player.obj->z;

	player.obj->speed = PLAYER_SPEED;
	player.obj->height = PLAYER_HEIGHT;
	player.obj->step_height = PLAYER_STEP_SIZE;

	Player_SetupGunSprites();

	Player_GiveAll();

	if(!keep)
		Player_SetGun(GUN__PISTOL);

	Player_Rotate(spawn_rot);
}

Object* Player_GetObj()
{
	return player.obj;
}

PlayerData* Player_GetPlayerData()
{
	return &player;
}

void Player_Rotate(float rot)
{
	player.angle = rot;

	player.obj->dir_x = cosf(rot);
	player.obj->dir_y = sinf(rot);
}

void Player_Hurt(float dir_x, float dir_y)
{
	player.obj->flags |= OBJ_FLAG__JUST_HIT;
}

void Player_HandlePickup(Object* obj)
{
	switch (obj->sub_type)
	{
	case SUB__PICKUP_SMALLHP:
	{
		Sound_Emit(SOUND__PICKUP_HP, 0.025);
		Game_SetStatusMessage("SMALL HP PACK PICKED UP!");

		player.obj->hp += PICKUP_SMALLHP_HEAL;
		break;
	}
	case SUB__PICKUP_BIGHP:
	{
		Sound_Emit(SOUND__PICKUP_HP, 0.025);
		Game_SetStatusMessage("BIG HP PACK PICKED UP!");

		player.obj->hp += PICKUP_BIGHP_HEAL;
		break;
	}
	case SUB__PICKUP_AMMO:
	{
		Sound_Emit(SOUND__PICKUP_AMMO, 0.025);
		Game_SetStatusMessage("AMMO PACK PICKED UP!");

		player.buck_ammo += PICKUP_AMMO_GIVE / 2;
		player.bullet_ammo += PICKUP_AMMO_GIVE;
		break;
	}
	case SUB__PICKUP_ROCKETS:
	{
		Sound_Emit(SOUND__PICKUP_AMMO, 0.025);
		Game_SetStatusMessage("ROCKETS PICKED UP!");

		player.rocket_ammo += PICKUP_ROCKETS_GIVE;
		break;
	}
	case SUB__PICKUP_SHOTGUN:
	{
		Sound_Emit(SOUND__PICKUP_AMMO, 0.025);
		Game_SetStatusMessage("SHOTGUN PICKED UP!");

		bool just_picked = false;

		if (!player.gun_check[GUN__SHOTGUN])
		{
			just_picked = true;
		}
		
		player.gun_check[GUN__SHOTGUN] = true;
		player.buck_ammo += PICKUP_SHOTGUN_AMMO_GIVE;

		if (just_picked)
		{
			player.gun_timer = 0;
			Player_SetGun(GUN__SHOTGUN);
		}

		break;
	}
	case SUB__PICKUP_MACHINEGUN:
	{
		Sound_Emit(SOUND__PICKUP_AMMO, 0.025);
		Game_SetStatusMessage("MACHINE GUN PICKED UP!");

		bool just_picked = false;

		if (!player.gun_check[GUN__MACHINEGUN])
		{
			just_picked = true;
		}

		player.gun_check[GUN__MACHINEGUN] = true;
		player.bullet_ammo += PICKUP_MACHINEGUN_AMMO_GIVE;

		if (just_picked)
		{
			player.gun_timer = 0;
			Player_SetGun(GUN__MACHINEGUN);
		}

		break;
	}
	case SUB__PICKUP_DEVASTATOR:
	{
		Sound_Emit(SOUND__PICKUP_AMMO, 0.025);
		Game_SetStatusMessage("DEVASTATOR PICKED UP!");

		bool just_picked = false;

		if (!player.gun_check[GUN__DEVASTATOR])
		{
			just_picked = true;
		}

		player.gun_check[GUN__DEVASTATOR] = true;
		player.rocket_ammo += PICKUP_DEVASTATOR_AMMO_GIVE;

		if (just_picked)
		{
			player.gun_timer = 0;
			Player_SetGun(GUN__DEVASTATOR);
		}

		break;
	}
	case SUB__PICKUP_INVUNERABILITY:
	{
		Sound_Emit(SOUND__PICKUP_SPECIAL, 0.05);
		Game_SetStatusMessage("INVUNERABILITY ORB PICKED UP!");

		player.godmode_timer = PICKUP_INVUNERABILITY_TIME;
		break;
	}
	case SUB__PICKUP_QUAD_DAMAGE:
	{
		Sound_Emit(SOUND__PICKUP_SPECIAL, 0.05);
		Game_SetStatusMessage("QUAD DAMAGE ORB PICKED UP!");

		player.quad_timer = PICKUP_QUAD_TIME;
		break;
	}
	default:
	{
		break;
	}
	}

	//clamp
	if (player.bullet_ammo >= PLAYER_MAX_AMMO) player.bullet_ammo = PLAYER_MAX_AMMO;
	if (player.buck_ammo >= PLAYER_MAX_AMMO) player.buck_ammo = PLAYER_MAX_AMMO;
	if (player.rocket_ammo >= PLAYER_MAX_AMMO) player.rocket_ammo = PLAYER_MAX_AMMO;

	//"delete" the object
	Object_ConsumePickup(obj);
}

static void Player_ApplyGravity(float delta)
{
	if (player.obj->z > player.obj->floor_clamp)
	{
		player.z_vel += (GRAVITY_SCALE * PLAYER_GRAVITY_SCALE) * delta;
		player.z_vel *= PLAYER_FRICTION;
	}
	else
	{
		//apply some gravity
		player.z_vel = GRAVITY_SCALE * delta;
	}
}

static void Player_Move(float dirx, float diry, float delta)
{
	player.obj->speed = PLAYER_SPEED;
	if (player.slow_move == 1) player.obj->speed *= 0.5;

	player.prev_x = player.obj->x;
	player.prev_y = player.obj->y;

	Move_Object(player.obj, dirx, diry, delta, true);

	Player_ApplyGravity(delta);
	Move_ZMove(player.obj, player.z_vel);
}

void Player_Update(GLFWwindow* window, float delta)
{
	//Map* map = Map_GetMap();

	//printf("free list: %i, num_objects: %i , sorted_objects: %i \n", map->num_free_list, map->num_objects, map->num_sorted_objects);

	Player_ProcessInput(window);
	Player_UpdateTimers(delta);
	
	//tick down if overhealed
	if (player.obj->hp > PLAYER_MAX_HP)
	{
		if (player.hp_tick_timer <= 0)
		{
			player.obj->hp--;
			player.hp_tick_timer = PLAYER_OVERHEAL_HP_TICK_TIME;
		}
	}
	
	//update direction
	player.obj->dir_x = cosf(player.angle);
	player.obj->dir_y = sinf(player.angle);

	if (player.obj->hp > 0)
	{
		//move player
		float dir_x = (player.move_x * cosf(player.angle) + (player.move_y * cosf(player.angle - Math_DegToRad(90.0))));
		float dir_y = (player.move_x * sinf(player.angle) + (player.move_y * sinf(player.angle - Math_DegToRad(90.0))));

		Player_Move(dir_x, dir_y, delta);
	}
	
	//hacky way to store hp, since we reset the map objects on map change 
	player.stored_hp = player.obj->hp;
}

void Player_LerpUpdate(double lerp, double delta)
{
	if (!player.obj)
	{
		return;
	}

	if (player.obj->flags & OBJ_FLAG__JUST_HIT)
	{
		Player_HandleHurt();
		player.obj->flags &= ~OBJ_FLAG__JUST_HIT;
	}

	Sprite* gun_sprite = &player.gun_sprites[player.gun];
	GunInfo* gun_info = player.gun_info;

	if (player.obj->flags & OBJ_FLAG__JUST_ATTACKED)
	{
		gun_sprite->frame = 0;
		gun_sprite->playing = true;

		player.obj->flags &= ~OBJ_FLAG__JUST_ATTACKED;
	}
	//make sure to reset the frame
	if (!gun_sprite->playing)
	{
		gun_sprite->frame = 0;
	}

	Player_UpdateListener();
	Sprite_UpdateAnimation(gun_sprite, delta);

	//check for extra light
	if (gun_sprite->frame == gun_sprite->action_frame)
	{
		Vec3_u16 extra_light;
		extra_light.r = gun_info->light_color[0] * GUN_FLASH_SCALE;
		extra_light.g = gun_info->light_color[1] * GUN_FLASH_SCALE;
		extra_light.b = gun_info->light_color[2] * GUN_FLASH_SCALE;
		
		Render_SetExtraLightFrame(extra_light);
	}

	//bob gun
	if (player.move_x != 0 || player.move_y != 0 || player.shake_timer > 0)
	{
		const float bob_amp = 0.003;
		float bob_freq = 16;

		if (player.slow_move == 1) bob_freq *= 0.5;
		
		if (player.shake_timer > 0)
		{
			const float SHAKE_SCALE = 32;
			bob_freq *= SHAKE_SCALE * player.shake_scale;
		}

		player.bob += delta;

		player.gun_offset_x = cosf(player.bob * bob_freq / bob_freq) * bob_amp;
		player.gun_offset_y = sinf(player.bob * bob_freq) * bob_amp;
	}

	//smooth view lerp
	player.view_x = Math_lerpFraction(player.prev_x, player.obj->x, lerp);
	player.view_y = Math_lerpFraction(player.prev_y, player.obj->y, lerp);
	float z = Math_lerpFraction(player.obj->prev_z + player.obj->height, player.obj->z + player.obj->height, lerp);

	player.view_z = Math_lerpClamped(player.view_z, z, delta * Z_VIEW_LERP);


	//do a fake, but cheap camera shake effect
	if (player.shake_timer > 0 && player.shake_scale > 0)
	{
		player.view_x += (Math_randf() * 2.0 - 1.0) * player.shake_scale;
		player.view_y += (Math_randf() * 2.0 - 1.0) * player.shake_scale;
	}
}

void Player_GetView(float* r_x, float* r_y, float* r_z, float* r_yaw, float* r_angle)
{
	if (!player.obj)
	{
		return;
	}

	if (r_x) *r_x = player.view_x;
	if (r_y) *r_y = player.view_y;
	if (r_z) *r_z = player.view_z;
	if (r_yaw) *r_yaw = 0;
	if (r_angle) *r_angle = player.angle;

}
void Player_MouseCallback(float x, float y)
{
	if (!player.obj || player.obj->hp <= 0 || Game_GetState() != GS__LEVEL)
	{
		return;
	}

	static bool s_FirstMouse = true;

	static float last_x = 0.0f;
	static float last_y = 0.0f;

	if (s_FirstMouse)
	{
		x = 0;
		y = 0;
		s_FirstMouse = false;
	}

	float xOffset = x - last_x;
	float yOffset = last_y - y;

	last_x = x;
	last_y = y;

	float rotSpeed = xOffset * (player.sensitivity / MOUSE_SENS_DIVISOR);

	static float yaw = 0;

	player.angle += -rotSpeed;

	if (player.angle > 360)
	{
		player.angle = 0;
	}
	else if (player.angle < -360)
	{
		player.angle = 0;
	}
}

void Player_DrawHud(Image* image, FontData* font, int start_x, int end_x)
{
	Render_LockObjectMutex(false);

	//keep on stack
	GunType gun = player.gun;
	float hurt_timer = player.hurt_timer;
	float hit_timer = player.hit_timer;
	float god_timer = player.godmode_timer;
	float quad_timer = player.quad_timer;
	int hp = player.obj->hp;
	int visual_map_mode = VisualMap_GetMode();
	Vec3_u16 light = player.obj->sprite.light;
	Sprite gun_sprite = player.gun_sprites[player.gun];
	float gun_offset_x = player.gun_offset_x;
	float gun_offset_y = player.gun_offset_y;
	int bullet_ammo = player.bullet_ammo;
	int buck_ammo = player.buck_ammo;
	int rocket_ammo = player.rocket_ammo;
	int sector_index = player.obj->sector_index;

	Render_UnlockObjectMutex(false);

	//draw gun
	if (hp > 0)
	{
#ifdef DISABLE_LIGHTMAPS

		if (sector_index >= 0)
		{
			Sector* sector = Map_GetSector(sector_index);

			light.r = sector->light_level;
			light.g = sector->light_level;
			light.b = sector->light_level;
		}
#endif

		//check for gunshot light
		if (gun_sprite.frame == gun_sprite.action_frame)
		{
			light.r += 255;
			light.g += 255;
			light.b += 255;
		}

		gun_sprite.light = light;

		gun_sprite.x += gun_offset_x;
		gun_sprite.y += gun_offset_y;

		Video_DrawScreenSprite(image, &gun_sprite, start_x, end_x);
	}
	if (visual_map_mode > 0)
	{
		return;
	}
	//draw hud text
	if (hp > 0)
	{
		//pseudo crosshair
		if (hit_timer > 0)
		{
			Text_DrawColor(image, font, 0.5, 0.49, 0.3, 0.3, start_x, end_x, 255, 0, 0, 255, "+");
		}
		else
		{
			Text_Draw(image, font, 0.5, 0.49, 0.3, 0.3, start_x, end_x, "+");
		}

		//hp
		int hp_color[3] = { 255, 255, 255 };

		if (hp > PLAYER_MAX_HP)
		{
			hp_color[0] = 128;
			hp_color[1] = 128;
		}

		Text_DrawColor(image, font, 0.02, 0.95, 1, 1, start_x, end_x, hp_color[0], hp_color[1], hp_color[2], 255, "HP %i", hp);


		if (gun == GUN__PISTOL)
		{
			Text_Draw(image, font, 0.75, 0.95, 1, 1, start_x, end_x, "AMMO INF");
		}
		else
		{
			int ammo = 0;
			//ammo
			switch (gun)
			{
			case GUN__PISTOL:
			{
				break;
			}
			case GUN__MACHINEGUN:
			{
				ammo = bullet_ammo;
				break;
			}
			case GUN__SHOTGUN:
			{
				ammo = buck_ammo;
				break;
			}
			case GUN__DEVASTATOR:
			{
				ammo = rocket_ammo;
				break;
			}
			default:
				break;
			}

			Text_Draw(image, font, 0.75, 0.95, 1, 1, start_x, end_x, "AMMO %i", ammo);
		}
	}
	else
	{
		Text_Draw(image, font, 0.45, 0.2, 1, 1, start_x, end_x, "DEAD");
		Text_Draw(image, font, 0.2, 0.5, 1, 1, start_x, end_x, "PRESS FIRE TO CONTINUE...");
	}

	//SHADERS
	if (hurt_timer > 0)
	{
		Video_Shade(image, Shader_HurtSimple, start_x, 0, end_x, image->height);
	}
	else if (god_timer > 0)
	{
		Video_Shade(image, Shader_Godmode, start_x, 0, end_x, image->height);
	}
	else if (quad_timer > 0)
	{
		Video_Shade(image, Shader_Quad, start_x, 0, end_x, image->height);
	}
	if (hp <= 0)
	{
		//draw crazy death stuff
		Video_Shade(image, Shader_Hurt, start_x, 0, end_x, image->height);
		Video_Shade(image, Shader_Dead, start_x, 0, end_x, image->height);
	}
}

float Player_GetSensitivity()
{
	return player.sensitivity;
}

void Player_SetSensitivity(float sens)
{
	if(sens < MIN_SENS)
	{
		sens = MIN_SENS;
	}
	else if (sens > MAX_SENS)
	{
		sens = MAX_SENS;
	}

	player.sensitivity = sens;
}
void Player_SetGun(GunType gun_type)
{
	if (player.gun == gun_type)
	{
		return;
	}
	if (!player.gun_check[gun_type])
	{
		return;
	}
	if (player.gun_timer > 0)
	{
		return;
	}

	Sprite_ResetAnimState(&player.gun_sprites[player.gun]);
	Sprite_ResetAnimState(&player.gun_sprites[gun_type]);

	GunInfo* gun_info = Info_GetGunInfo(gun_type);

	player.gun = gun_type;
	player.gun_info = gun_info;
}