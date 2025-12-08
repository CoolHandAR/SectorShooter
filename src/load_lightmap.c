#include "g_common.h"

#include "utility.h"

#define LIGHTMAP_LUMP 0
#define LIGHTGRID_LUMP 1
#define MAX_LUMPS 2

typedef struct
{
	int offset, size;
} LightLump;

typedef enum
{
	LMT__LINE,
	LMT__FLOOR,
	LMT__CEIL
} LightmapType;

typedef struct
{
	int offset;
	int width;
	int height;
	int index;
	int type;
} LightmapLump;

typedef struct
{
	int x_blocks, y_blocks, z_blocks;
	int offset;
} LightgridLump;

typedef struct
{
	int magic;
	LightLump lumps[MAX_LUMPS];
} LightHeader;

static void* MallocLump(FILE* file, LightHeader* header, int lump_num, int size, int* r_size)
{
	if (lump_num < 0 || lump_num >= MAX_LUMPS)
	{
		return NULL;
	}

	LightLump* lump = &header->lumps[lump_num];

	int lump_size = Num_LittleLong(lump->size);
	int lump_pos = Num_LittleLong(lump->offset);

	if (lump_size <= 0 || lump_size < size)
	{
		return NULL;
	}

	void* data = malloc(lump_size);

	if (!data)
	{
		return NULL;
	}

	fseek(file, lump_pos, SEEK_SET);
	fread(data, lump_size, 1, file);

	if (r_size)
		*r_size = lump_size / size;

	return data;
}
static void* Read_Data(FILE* file, int offset, int size)
{
	if (size <= 0)
	{
		return NULL;
	}

	void* data = malloc(size);

	if (!data)
	{
		return NULL;
	}
	memset(data, 0, size);

	fseek(file, offset, SEEK_SET);
	fread(data, size, 1, file);

	return data;
}

static bool Save_Lump(FILE* file, LightHeader* header, int lump_num, const void* data, int size)
{
	LightLump* lump = &header->lumps[lump_num];

	lump->offset = Num_LittleLong(ftell(file));
	lump->size = Num_LittleLong(size);

	return File_Write(file, data, (size + 3) & ~3);
}
static int Save_Data(FILE* file, const void* data, int size)
{
	int offset = Num_LittleLong(ftell(file));

	File_Write(file, data, size);

	return offset;
}

static void GetProperSuffix(const char* filename, char buffer[256])
{
	memset(buffer, 0, sizeof(buffer));

	strcpy(buffer, filename);

	for (int i = 0; i < strlen(buffer); i++)
	{
		if (buffer[i] == '.')
		{
			buffer[i] = 0;
			break;
		}
	}

	sprintf(buffer, "%s%s", buffer, ".lm");
}

static bool Load_ParseLightmaps(LightHeader* header, FILE* file, Map* map)
{
	int num_lightmaps = 0;
	LightmapLump* lightmaplumps = MallocLump(file, header, LIGHTMAP_LUMP, sizeof(LightmapLump), &num_lightmaps);

	if (!lightmaplumps)
	{
		return false;
	}

	for (int i = 0; i < num_lightmaps; i++)
	{
		LightmapLump* lump = &lightmaplumps[i];

		int type = Num_LittleLong(lump->type);
		int index = Num_LittleLong(lump->index);

		Lightmap* lightmap = NULL;
		switch (type)
		{
		case LMT__LINE:
		{
			Linedef* linedef = Map_GetLineDef(index);

			if (linedef)
			{
				lightmap = &linedef->lightmap;
			}

			break;
		}
		case LMT__FLOOR:
		{
			Sector* sector = Map_GetSector(index);

			if (sector)
			{
				lightmap = &sector->floor_lightmap;
			}

			break;
		}
		case LMT__CEIL:
		{
			Sector* sector = Map_GetSector(index);

			if (sector)
			{
				lightmap = &sector->ceil_lightmap;
			}
			break;
		}
		default:
			break;
		}

		if (!lightmap)
		{
			continue;
		}

		lightmap->width = Num_LittleLong(lump->width);
		lightmap->height = Num_LittleLong(lump->height);
		
		if (lightmap->width * lightmap->height <= 0)
		{
			continue;
		}

		lightmap->data = Read_Data(file, Num_LittleLong(lump->offset), (lightmap->width * lightmap->height) * sizeof(Vec4_u8));
	}

	
	free(lightmaplumps);

	return true;
}
static bool Load_ParseLightgrid(LightHeader* header, FILE* file, Map* map)
{
	LightgridLump* lightgridlump = MallocLump(file, header, LIGHTGRID_LUMP, sizeof(LightgridLump), NULL);

	if (!lightgridlump)
	{
		return false;
	}

	int x_blocks = Num_LittleLong(lightgridlump->x_blocks);
	int y_blocks = Num_LittleLong(lightgridlump->y_blocks);
	int z_blocks = Num_LittleLong(lightgridlump->z_blocks);
	int offset = Num_LittleLong(lightgridlump->offset);

	free(lightgridlump);

	map->lightgrid.blocks = Read_Data(file, offset, sizeof(Lightblock) * x_blocks * y_blocks * z_blocks);
	if (!map->lightgrid.blocks)
	{
		return false;
	}

	Map_SetupLightGrid(x_blocks, y_blocks, z_blocks);

	//need to do this since objects are loaded first and their initial light needs updating
	Map_UpdateObjectsLight();

	return true;
}

bool Load_Lightmap(const char* filename, Map* map)
{
	char buffer[256];
	GetProperSuffix(filename, buffer);

	printf("Loading lightmaps at: %s \n", buffer);

	FILE* file = fopen(buffer, "rb");

	if (!file)
	{
		printf("Failed to load lightmaps \n");
		return false;
	}

	LightHeader header;
	memset(&header, 0, sizeof(header));

	if (!File_Read(file, &header, sizeof(header)))
	{
		printf("Failed to load lightmaps \n");
		fclose(file);
		return false;
	}

	if (!Load_ParseLightmaps(&header, file, map))
	{
		printf("Failed to load lightmaps \n");
		fclose(file);
		return false;
	}
	if (!Load_ParseLightgrid(&header, file, map))
	{
		printf("Failed to load lightgrid \n");
		fclose(file);
		return false;
	}

	printf("Loaded lightmaps successfully\n");

	fclose(file);

	return true;
}

bool Save_Lightmap(const char* filename, Map* map)
{
	int num_lightmaps = 0;

	for (int i = 0; i < map->num_linedefs; i++)
	{
		Linedef* line = &map->linedefs[i];

		if (line->lightmap.data)
		{
			num_lightmaps++;
		}
	}
	for (int i = 0; i < map->num_sectors; i++)
	{
		Sector* sector = &map->sectors[i];

		if (sector->floor_lightmap.data)
		{
			num_lightmaps++;
		}
		if (sector->ceil_lightmap.data)
		{
			num_lightmaps++;
		}
	}

	if (num_lightmaps <= 0)
	{
		return true;
	}

	char buffer[256];
	GetProperSuffix(filename, buffer);

	LightHeader header;
	memset(&header, 0, sizeof(header));

	FILE* file = fopen(buffer, "wb");

	if (!file)
	{
		printf("Failed to save lightmap \n");
		return false;
	}

	if (!File_Write(file, &header, sizeof(header)))
	{
		fclose(file);
		return false;
	}

	LightmapLump* lightmaps = calloc(num_lightmaps, sizeof(LightmapLump));

	if (!lightmaps)
	{
		return false;
	}
	LightgridLump lightgrid_lump;
	memset(&lightgrid_lump, 0, sizeof(lightgrid_lump));
	
	Save_Lump(file, &header, LIGHTMAP_LUMP, lightmaps, sizeof(LightmapLump) * num_lightmaps);
	Save_Lump(file, &header, LIGHTGRID_LUMP, &lightgrid_lump, sizeof(LightgridLump));

	//save all lightmaps
	int lightmap_index = 0;
	for (int i = 0; i < map->num_linedefs; i++)
	{
		Linedef* line = &map->linedefs[i];

		if (!line->lightmap.data)
		{
			continue;
		}

		LightmapLump* lump = &lightmaps[lightmap_index++];

		lump->width = Num_LittleLong(line->lightmap.width);
		lump->height = Num_LittleLong(line->lightmap.height);
		lump->index = Num_LittleLong(i);
		lump->type = Num_LittleLong(LMT__LINE);

		lump->offset = Save_Data(file, line->lightmap.data, (lump->width * lump->height) * sizeof(Vec4_u8));
	}
	for (int i = 0; i < map->num_sectors; i++)
	{
		Sector* sector = &map->sectors[i];

		if (sector->floor_lightmap.data)
		{
			LightmapLump* lump = &lightmaps[lightmap_index++];

			lump->width = Num_LittleLong(sector->floor_lightmap.width);
			lump->height = Num_LittleLong(sector->floor_lightmap.height);
			lump->index = Num_LittleLong(i);
			lump->type = Num_LittleLong(LMT__FLOOR);

			lump->offset = Save_Data(file, sector->floor_lightmap.data, (lump->width * lump->height) * sizeof(Vec4_u8));
		}
		if (sector->ceil_lightmap.data)
		{
			LightmapLump* lump = &lightmaps[lightmap_index++];

			lump->width = Num_LittleLong(sector->ceil_lightmap.width);
			lump->height = Num_LittleLong(sector->ceil_lightmap.height);
			lump->index = Num_LittleLong(i);
			lump->type = Num_LittleLong(LMT__CEIL);

			lump->offset = Save_Data(file, sector->ceil_lightmap.data, (lump->width * lump->height) * sizeof(Vec4_u8));
		}
		
	}
	//save lightgrid data
	lightgrid_lump.x_blocks = map->lightgrid.block_size[0];
	lightgrid_lump.y_blocks = map->lightgrid.block_size[1];
	lightgrid_lump.z_blocks = map->lightgrid.block_size[2];

	lightgrid_lump.offset = Save_Data(file, map->lightgrid.blocks, sizeof(Lightblock) * (lightgrid_lump.x_blocks * lightgrid_lump.y_blocks * lightgrid_lump.z_blocks));


	fseek(file, 0, SEEK_SET);

	if (!File_Write(file, &header, sizeof(header)))
	{
		free(lightmaps);
		fclose(file);
		return false;
	}

	Save_Lump(file, &header, LIGHTMAP_LUMP, lightmaps, sizeof(LightmapLump) * num_lightmaps);
	Save_Lump(file, &header, LIGHTGRID_LUMP, &lightgrid_lump, sizeof(LightgridLump));

	free(lightmaps);
	fclose(file);

	return true;
}