#include "g_common.h"

#include "light.h"

#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include "game_info.h"
#include "u_math.h"
#include "utility.h"

//#define NO_MONSTERS

#define DOOM_VERTEX_SHIFT 1
#define DOOM_Z_SHIFT 1

typedef struct
{
    short		width;		// bounding box size 
    short		height;
    short		leftoffset;	// pixels to the left of origin 
    short		topoffset;	// pixels below the origin 
    int			columnofs[8];	// only [width] used
    // the [0] is &columnofs[width] 
} patch_t;

// posts are runs of non masked source pixels
typedef struct
{
   unsigned char		topdelta;	// -1 is the last post in a column
   unsigned char		length; 	// length data bytes follows
} post_t;

// column_t is a list of 0 or more post_t, (byte)-1 terminated
typedef post_t	column_t;
//
// Texture definition.
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
// The lumps are referenced by number, and patched
// into the rectangular texture space using origin
// and possibly other attributes.
//
typedef struct
{
    short	originx;
    short	originy;
    short	patch;
    short	stepdir;
    short	colormap;
} mappatch_t;


//
// Texture definition.
// A DOOM wall texture is a list of patches
// which are to be combined in a predefined order.
//

typedef struct
{
    char		name[8];
    int		masked;
    short		width;
    short		height;
    unsigned columndirectory;	// OBSOLETE
    short		patchcount;
    mappatch_t	patches[1];
} maptexture_t;


// A single patch from a texture definition,
//  basically a rectangular area within
//  the texture rectangle.
typedef struct
{
    // Block origin (allways UL),
    // which has allready accounted
    // for the internal origin of the patch.
    int		originx;
    int		originy;
    int		patch;
} texpatch_t;


// A maptexturedef_t describes a rectangular texture,
//  which is composed of one or more mappatch_t structures
//  that arrange graphic patches.
typedef struct
{
    // Keep name for switch changing, etc.
    char	name[8];
    short	width;
    short	height;

    // All the patches[patchcount]
    //  are drawn back to front into the cached texture.
    short	patchcount;
    texpatch_t	patches[1];

} texture_t;

typedef struct
{
    // Should be "IWAD" or "PWAD".
    char		identification[4];
    int			numlumps;
    int			infotableofs;
} wadinfo_t;

typedef struct
{
    int			filepos;
    int			size;
    char		name[8];

} filelump_t;

//
// WADFILE I/O related stuff.
//
typedef struct
{
    char	name[8];
    int		handle;
    int		position;
    int		size;
} lumpinfo_t;


enum
{
    ML_LABEL,		// A separator, name, ExMx or MAPxx
    ML_THINGS,		// Monsters, items..
    ML_LINEDEFS,		// LineDefs, from editing
    ML_SIDEDEFS,		// SideDefs, from editing
    ML_VERTEXES,		// Vertices, edited and BSP splits generated
    ML_SEGS,		// LineSegs, from LineDefs split by BSP
    ML_SSECTORS,		// SubSectors, list of LineSegs
    ML_NODES,		// BSP nodes
    ML_SECTORS,		// Sectors, from editing
    ML_REJECT,		// LUT, sector-sector visibility	
    ML_BLOCKMAP,		// LUT, motion clipping, walls/grid element,
    ML_LIGHTGRID, //LIGHT GRID
};


// A single Vertex.
typedef struct
{
    short		x;
    short		y;
} mapvertex_t;


// A SideDef, defining the visual appearance of a wall,
// by setting textures and offsets.
typedef struct
{
    short		textureoffset;
    short		rowoffset;
    char		toptexture[8];
    char		bottomtexture[8];
    char		midtexture[8];
    // Front sector, towards viewer.
    short		sector;
} mapsidedef_t;



// A LineDef, as used for editing, and as input
// to the BSP builder.
typedef struct
{
    short		v1;
    short		v2;
    short		flags;
    short		special;
    short		tag;
    // sidenum[1] will be -1 if one sided
    short		sidenum[2];
} maplinedef_t;

// Skill flags.
#define	MTF_EASY		1
#define	MTF_NORMAL		2
#define	MTF_HARD		4

// Deaf monsters/do not react to sound.
#define	MTF_AMBUSH		8


//
// LineDef attributes.
//

// Solid, is an obstacle.
#define ML_BLOCKING		1

// Blocks monsters only.
#define ML_BLOCKMONSTERS	2

// Backside will not be present at all
//  if not two sided.
#define ML_TWOSIDED		4

// If a texture is pegged, the texture will have
// the end exposed to air held constant at the
// top or bottom of the texture (stairs or pulled
// down things) and will move with a height change
// of one of the neighbor sectors.
// Unpegged textures allways have the first row of
// the texture at the top pixel of the line for both
// top and bottom textures (use next to windows).

// upper texture unpegged
#define ML_DONTPEGTOP		8

// lower texture unpegged
#define ML_DONTPEGBOTTOM	16	

// In AutoMap: don't map as two sided: IT'S A SECRET!
#define ML_SECRET		32

// Sound rendering: don't let sound cross two of these.
#define ML_SOUNDBLOCK		64

// Don't draw on the automap at all.
#define ML_DONTDRAW		128

// Set if already seen, thus drawn in automap.
#define ML_MAPPED		256


// Sector definition, from editing.
typedef	struct
{
    short		floorheight;
    short		ceilingheight;
    char		floorpic[8];
    char		ceilingpic[8];
    short		lightlevel;
    short		special;
    short		tag;
} mapsector_t;

// SubSector, as generated by BSP.
typedef struct
{
    short		numsegs;
    // Index of first one, segs are stored sequentially.
    short		firstseg;
} mapsubsector_t;


// LineSeg, generated by splitting LineDefs
// using partition lines selected by BSP builder.
typedef struct
{
    short		v1;
    short		v2;
    short		angle;
    short		linedef;
    short		side;
    short		offset;
} mapseg_t;

// BSP node structure.

// Indicate a leaf.
#define	NF_SUBSECTOR	0x8000

typedef struct
{
    // Partition line from (x,y) to x+dx,y+dy)
    short		x;
    short		y;
    short		dx;
    short		dy;

    // Bounding box for each child,
    // clip against view frustum.
    short		bbox[2][4];

    // If NF_SUBSECTOR its a subsector,
    // else it's a node of another subtree.
    unsigned short	children[2];

} mapnode_t;

// Thing definition, position, orientation and type,
// plus skill/visibility flags and attributes.
typedef struct
{
    short		x;
    short		y;
    short		angle;
    short		type;
    short		options;
} mapthing_t;


typedef enum
{
    THING__BLUE_TORCH = 44,
    THING__GREEN_TORCH = 45,
    THING__RED_TORCH = 46,
    THING__BLUE_LAMP = 55,
    THING__GREEN_LAMP = 56,
    THING__RED_LAMP = 57,
    THING__SHOTGUN = 2001,
    THING__SMALL_HP = 2011,
    THING__BIG_HP = 2012,
    THING__LAMP = 2028,
    THING__ROCKETS = 2046,
    THING__AMMO = 2048,
    THING__IMP = 3001,
    THING__PINKY = 3002,
    THING__BRUISER = 3003,
    THING__SUN = 10003,
    THING__RED_COLLUMN = 10004,
    THING__BLUE_COLLUMN = 10005,
    THING__RED_FLAG = 10006,
    THING__BLUE_FLAG = 10007,
    THING__CACTUS0 = 10008,
    THING__CACTUS1 = 10009,
    THING__CACTUS2 = 10010,
    THING__DEADTREE0 = 10011,
    THING__DEADTREE1 = 10012,
    THING__DEADTREE2 = 10013,
    THING__DEADTREE3 = 10014,
    THING__SPINNING_PYARMID = 10015,
    THING__BUSH0 = 10016,
    THING__BUSH1 = 10017,
    THING__BUSH2 = 10018,
    THING__TREE0 = 10019,
    THING__TREE1 = 10020,
    THING__TREE2 = 10021,
    THING__TREE3 = 10022,
    THING__SFX_DESERT_WIND = 10023,
    THING__SFX_TEMPLE_AMBIENCE = 10024,
    THING__SFX_JUNGLE_AMBIENCE = 10025,
    THING__FLAME_URN = 10026,
    THING__DEVASTATOR = 10027,
    THING__MACHINE_GUN = 10028
} thingtypes;


static void* MallocLump(FILE* file, wadinfo_t* header, filelump_t* lumps, int lump_start, int lump_num, int size, int* r_num)
{
    if (lump_num + lump_start < 0 || lump_start + lump_num >= header->numlumps)
    {
        return NULL;
    }

    filelump_t* lump = &lumps[lump_start + lump_num];

    int lump_size = lump->size;
    int lump_pos = lump->filepos;
    
    if (lump_size <= 0)
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

    if(r_num)
        *r_num = lump_size / size;

    return data;
}

static void Load_Sectors(mapsector_t* msectors, int num, Texture* sky_texture, Map* map)
{
    if (num <= 0) return;

    map->sectors = calloc(num, sizeof(Sector));

    if (!map->sectors)
    {
        return;
    }

    map->num_sectors = num;

    for (int i = 0; i < num; i++)
    {
        mapsector_t* ms = &msectors[i];
        Sector* os = &map->sectors[i];

        os->ceil = (int)ms->ceilingheight << DOOM_Z_SHIFT;
        os->floor = (int)ms->floorheight << DOOM_Z_SHIFT;

        os->base_ceil = os->ceil;
        os->base_floor = os->floor;

        os->r_ceil = os->ceil;
        os->r_floor = os->floor;

        os->sector_tag = ms->tag;
        os->special = ms->special;
        os->sector_object = -1;

        os->light_level = ms->lightlevel;

        if (os->light_level > 255)
        {
            os->light_level = 255;
        }
        else if (os->light_level < 0)
        {
            os->light_level = 0;
        }
       
#ifndef DISABLE_LIGHTMAPS
        if (os->special == 0)
        {
            os->light_level = 1;
        }
#endif
        os->ceil_texture = Game_FindTextureByName(ms->ceilingpic);
        os->floor_texture = Game_FindTextureByName(ms->floorpic);

        if (os->ceil_texture)
        {
            //check if it is a sky texture
            char name[12];
            memset(name, 0, sizeof(name));
            strncpy(name, ms->ceilingpic, sizeof(ms->ceilingpic));

            if (!strcmp(name, "F_SKY1"))
            {
                os->ceil_texture = sky_texture;
                os->is_sky = true;
            }
        }
    }
}
static void Load_SubSectors(mapsubsector_t* mssectors, int num, Map* map)
{
    if (num <= 0) return;

    map->sub_sectors = calloc(num, sizeof(Subsector));

    if (!map->sub_sectors)
    {
        return;
    }

    map->num_sub_sectors = num;

    for (int i = 0; i < num; i++)
    {
        mapsubsector_t* ms = &mssectors[i];
        Subsector* os = &map->sub_sectors[i];

        os->line_offset = ms->firstseg;
        os->num_lines = ms->numsegs;
    }
}
static void Load_LineSegs(mapseg_t* msegs, int num, mapvertex_t* vertices, mapsidedef_t* msides, maplinedef_t* mlinedefs, Map* map)
{
    if (num <= 0) return;

    map->line_segs = calloc(num, sizeof(Line));

    if (!map->line_segs)
    {
        return;
    }

    map->num_line_segs = num;

    for (int i = 0; i < num; i++)
    {
        mapseg_t* ms = &msegs[i];
        Line* os = &map->line_segs[i];
  
        os->x0 = (int)vertices[ms->v2].x >> DOOM_VERTEX_SHIFT;
        os->y0 = (int)vertices[ms->v2].y >> DOOM_VERTEX_SHIFT;

        os->x1 = (int)vertices[ms->v1].x >> DOOM_VERTEX_SHIFT;
        os->y1 = (int)vertices[ms->v1].y >> DOOM_VERTEX_SHIFT;

        os->offset = ms->offset;

        int linedef_index = ms->linedef;
        maplinedef_t* mldef = &mlinedefs[linedef_index];
        int side = ms->side;
        
        
        float side_x0 = (int)vertices[mldef->v2].x >> DOOM_VERTEX_SHIFT;
        float side_y0 = (int)vertices[mldef->v2].y >> DOOM_VERTEX_SHIFT;
        
        float side_x1 = (int)vertices[mldef->v1].x >> DOOM_VERTEX_SHIFT;
        float side_y1 = (int)vertices[mldef->v1].y >> DOOM_VERTEX_SHIFT;

        float side_dx = side_x1 - side_x0;
        float side_dy = side_y1 - side_y0;

        
        os->front_sector = msides[mldef->sidenum[side]].sector;

        os->sidedef = &map->sidedefs[mldef->sidenum[side]];
        os->linedef = &map->linedefs[linedef_index];

        if (mldef->flags & ML_TWOSIDED)
        {
            os->back_sector = msides[mldef->sidenum[side ^ 1]].sector;
        }
        else
        {
            os->back_sector = -1;
        }
       

        os->width_scale = sqrtf(side_dx * side_dx + side_dy * side_dy);

        Sector* frontsector = &map->sectors[os->front_sector];
        Sector* backsector = NULL;

        if (os->back_sector >= 0)
        {
            backsector = &map->sectors[os->back_sector];
        }
    }

}

static void Load_Nodes(mapnode_t* mnodes, int num, Map* map)
{
    if (num <= 0) return;

    map->bsp_nodes = calloc(num, sizeof(BSPNode));

    if (!map->bsp_nodes)
    {
        return;
    }

    map->num_nodes = num;

    for (int i = 0; i < num; i++)
    {
        mapnode_t* mn = &mnodes[i];
        BSPNode* on = &map->bsp_nodes[i];

        on->children[0] = mn->children[0];
        on->children[1] = mn->children[1];

        on->line_x = (int)mn->x >> DOOM_VERTEX_SHIFT;
        on->line_y = (int)mn->y >> DOOM_VERTEX_SHIFT;
        on->line_dx = (int)mn->dx >> DOOM_VERTEX_SHIFT;
        on->line_dy = (int)mn->dy >> DOOM_VERTEX_SHIFT;

        for (int k = 0; k < 2; k++)
        {
            for (int j = 0; j < 4; j++)
            {
                on->bbox[k][j] = (int)mn->bbox[k][j] >> DOOM_VERTEX_SHIFT;
            }
        }
    }
}

static void Load_Sidedefs(mapsidedef_t* msidedefs, int num, Map* map)
{
    if (num <= 0) return;

    map->sidedefs= calloc(num, sizeof(Sidedef));

    if (!map->sidedefs)
    {
        return;
    }

    map->num_sidedefs = num;

    for (int i = 0; i < num; i++)
    {
        mapsidedef_t* ms = &msidedefs[i];
        Sidedef* os = &map->sidedefs[i];

        os->x_offset = ms->textureoffset;
        os->y_offset = ms->rowoffset;

        os->bottom_texture = Game_FindTextureByName(ms->bottomtexture);
        os->middle_texture = Game_FindTextureByName(ms->midtexture);
        os->top_texture = Game_FindTextureByName(ms->toptexture);
    }
}
static void Load_Linedefs(maplinedef_t* mlinedefs, int num, mapvertex_t* vertices, mapsidedef_t* msides, Map* map)
{
    if (num <= 0) return;

    map->linedefs = calloc(num, sizeof(Linedef));

    if (!map->linedefs)
    {
        return;
    }

    map->num_linedefs = num;

    for (int i = 0; i < num; i++)
    {
        maplinedef_t* ms = &mlinedefs[i];
        Linedef* os = &map->linedefs[i];

        os->x0 = (int)vertices[ms->v2].x >> DOOM_VERTEX_SHIFT;
        os->y0 = (int)vertices[ms->v2].y >> DOOM_VERTEX_SHIFT;

        os->x1 = (int)vertices[ms->v1].x >> DOOM_VERTEX_SHIFT;
        os->y1 = (int)vertices[ms->v1].y >> DOOM_VERTEX_SHIFT;

        os->dx = os->x1 - os->x0;
        os->dy = os->y1 - os->y0;

        os->dot = os->dx * os->dx + os->dy * os->dy;

        os->sides[0] = ms->sidenum[0];
        os->sides[1] = ms->sidenum[1];

        os->front_sector = msides[ms->sidenum[0]].sector;

        if (ms->flags & ML_TWOSIDED && os->sides[1] != -1)
        {
            os->back_sector = msides[ms->sidenum[1]].sector;
        }
        else
        {
            os->back_sector = -1;
        }

        os->flags = ms->flags;
        os->sector_tag = ms->tag;
        os->special = ms->special;
        os->bbox[0][0] = min(os->x0, os->x1);
        os->bbox[0][1] = min(os->y0, os->y1);
        os->bbox[1][0] = max(os->x0, os->x1);
        os->bbox[1][1] = max(os->y0, os->y1);

        os->index = i;
    }
}

static void Load_PostProcessMap(Texture* sky_texture, Map* map)
{
    //calculate bounding box of subsector
    //and set ptr to front sector
   
    float min_height = FLT_MAX;
    float max_height = -FLT_MAX;

    for (int i = 0; i < map->num_sectors; i++)
    {
        Sector* sector = &map->sectors[i];
        sector->index = i;

        for (int k = 0; k < 2; k++)
        {
            sector->bbox[0][k] = FLT_MAX;
            sector->bbox[1][k] = -FLT_MAX;
        }

        for (int k = 0; k < map->num_sub_sectors; k++)
        {
            Subsector* sub = &map->sub_sectors[k];

            Line* line = &map->line_segs[sub->line_offset];
            sub->sector = &map->sectors[line->front_sector];

            if (sub->sector == sector)
            {
                if (sub->num_lines == 0)
                {
                    continue;
                }

                float sub_box[2][2];

                for (int l = 0; l < 2; l++)
                {
                    sub_box[0][l] = FLT_MAX;
                    sub_box[1][l] = -FLT_MAX;
                }
                for (int l = 0; l < sub->num_lines; l++)
                {
                    Line* line = &map->line_segs[sub->line_offset + l];

                    float line_box[2][2];

                    line_box[0][0] = min(line->x0, line->x1);
                    line_box[0][1] = min(line->y0, line->y1);
                    line_box[1][0] = max(line->x0, line->x1);
                    line_box[1][1] = max(line->y0, line->y1);

                    for (int j = 0; j < 2; j++)
                    {
                        sub_box[0][j] = min(sub_box[0][j], line_box[0][j]);
                        sub_box[1][j] = max(sub_box[1][j], line_box[1][j]);
                    }
                }


                for (int j = 0; j < 2; j++)
                {
                    sector->bbox[0][j] = min(sector->bbox[0][j], sub_box[0][j]);
                    sector->bbox[1][j] = max(sector->bbox[1][j], sub_box[1][j]);
                }
            }
        }

        min_height = min(min_height, min(sector->ceil, sector->floor));
        max_height = max(max_height, max(sector->ceil, sector->floor));
    }

    //create and setup spatial tree
    map->spatial_tree = BVH_Tree_Create(0.5);
    
    int index = -1;
    for (int i = 0; i < map->num_linedefs; i++)
    {
        Linedef* line = &map->linedefs[i];
        BVH_Tree_Insert(&map->spatial_tree, line->bbox, index);

        index--;
    }

    //setup sectors specials
    for (int i = 0; i < map->num_linedefs; i++)
    {
        Linedef* line = &map->linedefs[i];

        int iter_index = 0;

        Sector* sector = NULL;

        if (line->special > 0)
        {
            if (line->sector_tag > 0)
            {
                while (sector = Map_GetNextSectorByTag(&iter_index, line->sector_tag))
                {
                    if (line->special == SPECIAL__USE_DOOR || line->special == SPECIAL__USE_DOOR_NEVER_CLOSE || line->special == SPECIAL__TRIGGER_DOOR_NEVER_CLOSE)
                    {
                        sector->neighbour_sector_value = Sector_FindLowestNeighbourCeilling(sector);
                    }
                    else if (line->special == SPECIAL__USE_LIFT)
                    {
                       sector->neighbour_sector_value = Sector_FindLowestNeighbourFloor(sector);
                    }
                }
            }
            else if(line->back_sector >= 0)
            {
                Sector* frontsector = &map->sectors[line->front_sector];
                Sector* backsector = &map->sectors[line->back_sector];

                if (line->special == SPECIAL__USE_DOOR || line->special == SPECIAL__USE_DOOR_NEVER_CLOSE || line->special == SPECIAL__TRIGGER_DOOR_NEVER_CLOSE)
                {
                    backsector->neighbour_sector_value = Sector_FindLowestNeighbourCeilling(backsector);
                }
                else if (line->special == SPECIAL__USE_LIFT)
                {
                    backsector->neighbour_sector_value = Sector_FindLowestNeighbourFloor(backsector);
                }
            }
        }
    }

    //calc world bounds
    for (int k = 0; k < 2; k++)
    {
        map->world_bounds[0][k] = FLT_MAX;
        map->world_bounds[1][k] = -FLT_MAX;
    }

    for (int i = 0; i < map->num_linedefs; i++)
    {
        Linedef* line = &map->linedefs[i];

        for (int j = 0; j < 2; j++)
        {
            map->world_bounds[0][j] = min(map->world_bounds[0][j], line->bbox[0][j]);
            map->world_bounds[1][j] = max(map->world_bounds[1][j], line->bbox[1][j]);
        }
    }

    for (int k = 0; k < 2; k++)
    {
        map->world_size[k] = map->world_bounds[1][k] - map->world_bounds[0][k];
    }

    map->world_height = max_height - min_height;
    map->world_min_height = min_height;
    map->world_max_height = max_height;

    //calc average sky color
    if (sky_texture)
    {
        Image* img = &sky_texture->img;

        if (img->data)
        {
            float total_color[3] = { 0, 0, 0 };
            int samples = 0;

            for (int x = 0; x < img->width; x++)
            {
                for (int y = 0; y < img->height; y++)
                {
                    unsigned char* data = Image_Get(img, x, y);

                    if (!data)
                    {
                        continue;
                    }

                    total_color[0] += data[0];
                    total_color[1] += data[1];
                    total_color[2] += data[2];

                    samples++;
                }
            }

            if (samples > 0)
            {
                map->sky_color[0] = Math_Clampl(total_color[0] / samples, 0, 255);
                map->sky_color[1] = Math_Clampl(total_color[1] / samples, 0, 255);
                map->sky_color[2] = Math_Clampl(total_color[2] / samples, 0, 255);
            }
           
        }
    }
}

static void Load_SetupSectorSpecials(Map* map)
{
    for (int i = 0; i < map->num_sectors; i++)
    {
        Sector* sector = &map->sectors[i];

        if (sector->special <= 0)
        {
            continue;
        }

        switch (sector->special)
        {
        case SECTOR_SPECIAL__LIGHT_FLICKER:
        {
            Sector_CreateLightStrober(sector, SUB__LIGHT_STROBER_FLICKER_RANDOM);
            break;
        }
        case SECTOR_SPECIAL__LIGHT_GLOW:
        {
            Sector_CreateLightStrober(sector, SUB__LIGHT_STROBER_GLOW);
            break;
        }
        default:
        {
            break;
        }
        }

    }
}

static void Load_Things(mapthing_t* mthings, int num, Map* map)
{
    mapthing_t* player_thing = NULL;

    for (int i = 0; i < num; i++)
    {
        mapthing_t* mt = &mthings[i];

        if (mt->type <= 4)
        {
            player_thing = mt;
            continue;
        }

        float object_x = (int)mt->x >> DOOM_VERTEX_SHIFT;
        float object_y = (int)mt->y >> DOOM_VERTEX_SHIFT;

        if (((int)mt->options & 1))
        {
           // continue;
        }

        int flags = 0;

        if ((int)mt->options & MTF_AMBUSH)
        {
            flags |= OBJ_FLAG__IGNORE_SOUND;
        }

        int type = OT__NONE;
        int sub_type = SUB__NONE;

        bool clamp_to_ceil = false;

        switch (mt->type)
        {
        case THING__SHOTGUN:
        {
            type = OT__PICKUP;
            sub_type = SUB__PICKUP_SHOTGUN;
            break;
        }
        case THING__AMMO:
        {
            type = OT__PICKUP;
            sub_type = SUB__PICKUP_AMMO;
            break;
        }
        case THING__ROCKETS:
        {
            type = OT__PICKUP;
            sub_type = SUB__PICKUP_ROCKETS;
            break;
        }
        case THING__BIG_HP:
        {
            type = OT__PICKUP;
            sub_type = SUB__PICKUP_BIGHP;
            break;
        }
        case THING__SMALL_HP:
        {
            type = OT__PICKUP;
            sub_type = SUB__PICKUP_SMALLHP;
            break;
        }
        case THING__IMP:
        {
            type = OT__MONSTER;
            sub_type = SUB__MOB_IMP;
            break;
        }
        case THING__PINKY:
        {
            type = OT__MONSTER;
            sub_type = SUB__MOB_PINKY;
            break;
        }
        case THING__BRUISER:
        {
            type = OT__MONSTER;
            sub_type = SUB__MOB_BRUISER;
            break;
        }
        case THING__LAMP:
        {
            type = OT__LIGHT;
            sub_type = SUB__LIGHT_TORCH;
            break;
        }
        case THING__SUN:
        {
            map->sun_angle = (float)mt->angle - 180;
            map->sun_color[0] = 255;
            map->sun_color[1] = 204;
            map->sun_color[2] = 51;
            map->sun_position[0] = object_x;
            map->sun_position[1] = object_y;
            break;
        }
        case THING__BLUE_TORCH:
        {
            type = OT__LIGHT;
            sub_type = SUB__LIGHT_BLUE_TORCH;
            break;
        }
        case THING__GREEN_TORCH:
        {
            type = OT__LIGHT;
            sub_type = SUB__LIGHT_GREEN_TORCH;
            break;
        }
        case THING__RED_TORCH:
        {
            type = OT__LIGHT;
            sub_type = SUB__LIGHT_TORCH;
            break;
        }
        case THING__RED_LAMP:
        {
            type = OT__LIGHT;
            sub_type = SUB__LIGHT_LAMP;
            clamp_to_ceil = true;
            break;
        }
        case THING__BLUE_LAMP:
        {
            type = OT__LIGHT;
            sub_type = SUB__LIGHT_BLUE_LAMP;
            clamp_to_ceil = true;
            break;
        }
        case THING__GREEN_LAMP:
        {
            type = OT__LIGHT;
            sub_type = SUB__LIGHT_GREEN_LAMP;
            clamp_to_ceil = true;
            break;
        }
        case THING__RED_COLLUMN:
        {
            type = OT__THING;
            sub_type = SUB__THING_RED_COLLUMN;
            break;
        }
        case THING__BLUE_COLLUMN:
        {
            type = OT__THING;
            sub_type = SUB__THING_BLUE_COLLUMN;
            break;
        }
        case THING__RED_FLAG:
        {
            type = OT__THING;
            sub_type = SUB__THING_RED_FLAG;
            break;
        }
        case THING__BLUE_FLAG:
        {
            type = OT__THING;
            sub_type = SUB__THING_BLUE_FLAG;
            break;
        }
        case THING__CACTUS0:
        {
            type = OT__THING;
            sub_type = SUB__THING_CACTUS0;
            break;
        }
        case THING__CACTUS1:
        {
            type = OT__THING;
            sub_type = SUB__THING_CACTUS1;
            break;
        }
        case THING__CACTUS2:
        {
            type = OT__THING;
            sub_type = SUB__THING_CACTUS2;
            break;
        }
        case THING__DEADTREE0:
        {
            type = OT__THING;
            sub_type = SUB__THING_DEAD_TREE0;
            break;
        }
        case THING__DEADTREE1:
        {
            type = OT__THING;
            sub_type = SUB__THING_DEAD_TREE1;
            break;
        }
        case THING__DEADTREE2:
        {
            type = OT__THING;
            sub_type = SUB__THING_DEAD_TREE2;
            break;
        }
        case THING__DEADTREE3:
        {
            type = OT__THING;
            sub_type = SUB__THING_DEAD_TREE3;
            break;
        }
        case THING__SPINNING_PYARMID:
        {
            type = OT__THING;
            sub_type = SUB__THING_SPINNING_PYRAMID;
            break;
        }
        case THING__BUSH0:
        {
            type = OT__THING;
            sub_type = SUB__THING_BUSH0;
            break;
        }
        case THING__BUSH1:
        {
            type = OT__THING;
            sub_type = SUB__THING_BUSH1;
            break;
        }
        case THING__BUSH2:
        {
            type = OT__THING;
            sub_type = SUB__THING_BUSH2;
            break;
        }
        case THING__TREE0:
        {
            type = OT__THING;
            sub_type = SUB__THING_TREE0;
            break;
        }
        case THING__TREE1:
        {
            type = OT__THING;
            sub_type = SUB__THING_TREE1;
            break;
        }
        case THING__TREE2:
        {
            type = OT__THING;
            sub_type = SUB__THING_TREE2;
            break;
        }
        case THING__TREE3:
        {
            type = OT__THING;
            sub_type = SUB__THING_TREE3;
            break;
        }
        case THING__SFX_DESERT_WIND:
        {
            type = OT__SFX_EMITTER;
            sub_type = SUB__SFX_DESERT_WIND;
            break;
        }
        case THING__SFX_TEMPLE_AMBIENCE:
        {
            type = OT__SFX_EMITTER;
            sub_type = SUB__SFX_TEMPLE_AMBIENCE;
            break;
        }
        case THING__SFX_JUNGLE_AMBIENCE:
        {
            type = OT__SFX_EMITTER;
            sub_type = SUB__SFX_JUNGLE_AMBIENCE;
            break;
        }
        case THING__FLAME_URN:
        {
            type = OT__LIGHT;
            sub_type = SUB__LIGHT_FLAME_URN;
            break;
        }
        case THING__DEVASTATOR:
        {
            type = OT__PICKUP;
            sub_type = SUB__PICKUP_DEVASTATOR;
            break;
        }
        case THING__MACHINE_GUN:
        {
            type = OT__PICKUP;
            sub_type = SUB__PICKUP_MACHINEGUN;
            break;
        }
        default:
            break;
        }

#ifdef NO_MONSTERS
        if (type == OT__MONSTER)
            type = OT__NONE;
#endif // NO_MONSTERS


        if (type != OT__NONE)
        {
            Object* obj = Object_Spawn(type, sub_type, object_x, object_y, 0);

            float rad_angle = Math_DegToRad(mt->angle);
            obj->dir_x = cosf(rad_angle);
            obj->dir_y = sinf(rad_angle);

            obj->flags |= flags;

            if (clamp_to_ceil)
            {
                obj->z = obj->ceil_clamp - obj->height;
                obj->sprite.z = obj->z;
            }
        }
    }

    if (player_thing)
    {
        map->player_spawn_point_x = (int)player_thing->x >> DOOM_VERTEX_SHIFT;
        map->player_spawn_point_y = (int)player_thing->y >> DOOM_VERTEX_SHIFT;
        map->player_spawn_rot = Math_DegToRad(player_thing->angle);
    }
    else
    {
        float center_x = 0, center_y = 0;
        Math_GetBoxCenter(map->world_bounds, &center_x, &center_y);

        map->player_spawn_point_x = center_x;
        map->player_spawn_point_y = center_y;
        map->player_spawn_rot = 0;
    }
}

static Texture* Load_FindSkyTexture(const char* skyname)
{
    return Game_FindTextureByName(skyname);
}

static int Load_FindLumpNum(const char* name, wadinfo_t* header, filelump_t* file_infos)
{
    char name_buff[12];
    for (int i = 0; i < header->numlumps; i++)
    {
        filelump_t* l = &file_infos[i];
        memset(name_buff, 0, sizeof(name_buff));

        strncpy(name_buff, l->name, 8);

        if (!_stricmp(name_buff, name))
        {
            return i;
        }
    }

    return -1;
}

bool Load_Doommap(const char* filename, const char* skyname, Map* map)
{
    bool result = true;

    FILE* file = fopen(filename, "rb");

    if (!file)
    {
        return false;
    }
    wadinfo_t header;
    memset(&header, 0, sizeof(header));

    fread(&header, sizeof(header), 1, file);

    filelump_t* file_infos = calloc(header.numlumps, sizeof(filelump_t));

    if (!file_infos)
    {
        result = false;
        goto cleanup;
    }
    int lump_start = 0;

    fseek(file, header.infotableofs, SEEK_SET);
    fread(file_infos, sizeof(filelump_t) * header.numlumps, 1, file);
    
    int num_sectors = 0;
    int num_vertices = 0;
    int num_subsectors = 0;
    int num_segs = 0;
    int num_nodes = 0;
    int num_linedefs = 0;
    int num_sidedefs = 0;
    int num_things = 0;

    //load lumps
    mapsector_t* sector_lump = MallocLump(file, &header, file_infos, lump_start, ML_SECTORS, sizeof(mapsector_t), &num_sectors);
    mapsubsector_t* subsector_lump = MallocLump(file, &header, file_infos, lump_start, ML_SSECTORS, sizeof(mapsubsector_t), &num_subsectors);
    mapseg_t* seg_lump = MallocLump(file, &header, file_infos, lump_start, ML_SEGS, sizeof(mapseg_t), &num_segs);
    mapvertex_t* vertex_lump = MallocLump(file, &header, file_infos, lump_start, ML_VERTEXES, sizeof(mapvertex_t), &num_vertices);
    mapnode_t* node_lump = MallocLump(file, &header, file_infos, lump_start, ML_NODES, sizeof(mapnode_t), &num_nodes);
    maplinedef_t* linedef_lump = MallocLump(file, &header, file_infos, lump_start, ML_LINEDEFS, sizeof(maplinedef_t), &num_linedefs);
    mapsidedef_t* sidedef_lump = MallocLump(file, &header, file_infos, lump_start, ML_SIDEDEFS, sizeof(mapsidedef_t), &num_sidedefs);
    mapthing_t* thing_lump = MallocLump(file, &header, file_infos, lump_start, ML_THINGS, sizeof(mapthing_t), &num_things);

    //find sky texture
    Texture* sky_texture = Load_FindSkyTexture(skyname);

    //setup into our format
    //order is important
    Load_Sectors(sector_lump, num_sectors, sky_texture, map);
    Load_SubSectors(subsector_lump, num_subsectors, map);
    Load_Nodes(node_lump, num_nodes, map);
    Load_Sidedefs(sidedef_lump, num_sidedefs, map);
    Load_Linedefs(linedef_lump, num_linedefs, vertex_lump, sidedef_lump, map);
    Load_LineSegs(seg_lump, num_segs, vertex_lump, sidedef_lump, linedef_lump, map);

    //load rejectblock
    int reject_size = 0;
    map->reject_matrix = MallocLump(file, &header, file_infos, lump_start, ML_REJECT, sizeof(unsigned char), &reject_size);
    map->reject_size = reject_size;

    //post processing step
    Load_PostProcessMap(sky_texture, map);

    //setup sector specials
    Load_SetupSectorSpecials(map);
    
    //load objects last
    Load_Things(thing_lump, num_things, map);

    //also save map name
    strncpy(map->name, filename, strlen(filename));

#ifndef DISABLE_LIGHTMAPS

    //check for lightmaps
    if(!Load_Lightmap(filename, map))
    {
        Map_SetupLightGrid(NULL);

        //create new lightmaps
        LightGlobal light_global;
        LightGlobal_Setup(&light_global);

        Lightmap_Create(&light_global, map);
        Lightblocks_Create(&light_global, map);

        LightGlobal_Destruct(&light_global);

        Save_Lightmap(filename, map);
    }
#endif // !DISABLE_LIGHTMAPS
   
    if (sector_lump) free(sector_lump);
    if (subsector_lump) free(subsector_lump);
    if (seg_lump)free(seg_lump);
    if (vertex_lump)free(vertex_lump);
    if (node_lump)free(node_lump);
    if (linedef_lump) free(linedef_lump);
    if (sidedef_lump) free(sidedef_lump);
    if (thing_lump) free(thing_lump);
    if (file_infos) free(file_infos);
cleanup:
    fclose(file);

	return result;
}

static unsigned Pack_Color(unsigned char c[4])
{
    unsigned color = 0;

    color |= ((unsigned)c[0] << 24u);
    color |= ((unsigned)c[1] << 16u);
    color |= ((unsigned)c[2] << 8u);
    color |= ((unsigned)c[3]);

    return color;
}

static void Unpack_Color(unsigned color, unsigned char r_c[4])
{
    r_c[0] = (color & 0xff000000u) >> 24u;
    r_c[1] = (color & 0xff0000u) >> 16u;
    r_c[2] = (color & 0xff00u) >> 8u;
    r_c[3] = (color & 0xffu);
}

static unsigned* Load_Pallete(FILE* file, wadinfo_t* header, filelump_t* file_infos, int lump_index)
{
    int pallete_size = 0;
    unsigned char* palette = MallocLump(file, header, file_infos, 0, lump_index, 1, &pallete_size);

    if (!palette)
    {
        return NULL;
    }

    int pal_index = 0;

    unsigned* lut = calloc(256, sizeof(unsigned));

    if (!lut)
    {
        if (palette) free(palette);
        return NULL;
    }

    for (int i = 0; i < 256; i++)
    {
        unsigned char c[4];
        c[0] = 255;
        c[1] = palette[pal_index + 2];
        c[2] = palette[pal_index + 1];
        c[3] = palette[pal_index + 0];

        pal_index += 3;

        lut[i] = Pack_Color(c);
    }

    if (palette) free(palette);

    return lut;
}

static void Load_FlatTextures(FILE* file, wadinfo_t* header, filelump_t* file_infos, int flat_start, int flat_end, unsigned* paletteLut)
{
    GameAssets* assets = Game_GetAssets();

    int num_flats = flat_end - flat_start;

    assets->flat_textures = calloc(num_flats, sizeof(Texture));

    if (!assets->flat_textures)
    {
        return;
    }

    int tex_index = 0;
    for (int i = 0; i < num_flats; i++)
    {
        int size = 0;
        unsigned char* flat = MallocLump(file, header, file_infos, 0, i + flat_start, 1, &size);
        filelump_t* lump = &file_infos[i + flat_start];
        if (!flat)
        {
            continue;
        }

        //invalid texture size
        if (size != 4096)
        {
            free(flat);
            continue;
        }

        Texture* texture = &assets->flat_textures[tex_index++];
        Image* img = &texture->img;

        //all flats are 64 x 64
        Image_Create(img, 64, 64, 4);

        for (int x = 0; x < 64; x++)
        {
            for (int y = 0; y < 64; y++)
            {
                unsigned* pix = Image_Get(img, x, y);

                int index = x + (y * 64);
                int pallete_index = flat[index];
                unsigned color = paletteLut[pallete_index];
                *pix = color;
            }
        }

        strncpy(texture->name, lump->name, 8);
        texture->width_mask = 63;
        texture->height_mask = 63;

        free(flat);
    }
    assets->num_flat_textures = tex_index;
}
static void Load_PatchyTextures(FILE* file, wadinfo_t* header, filelump_t* file_infos, int patchnames_index, int texture1_index, int texture2_index, unsigned* paletteLut)
{
    GameAssets* assets = Game_GetAssets();

    char* pnames = MallocLump(file, header, file_infos, 0, patchnames_index, 1, NULL);
    int num_patch_names = *((int*)pnames);

    if (!pnames)
    {
        return;
    }
   
    int* patchlookup = calloc(num_patch_names, sizeof(int));

    if (!patchlookup)
    {
        free(pnames);
        return;
    }

    char* name_p = pnames + 4;
    char name[12];
    for (int i = 0; i < num_patch_names; i++)
    {
        memset(name, 0, sizeof(name));
        strncpy(name, name_p + i * 8, 8);
        patchlookup[i] = Load_FindLumpNum(name, header, file_infos);
    }

    int* map_texture1 = MallocLump(file, header, file_infos, 0, texture1_index, 4, NULL);
    int num_textures1 = (long)*map_texture1;

    int* directory = map_texture1 + 1;

    int* map_texture2 = NULL;
    int num_textures2 = 0;

    if (texture2_index >= 0)
    {
        map_texture2 = MallocLump(file, header, file_infos, 0, texture2_index, 4, NULL);
        num_textures2 = (long)*map_texture2;
    }

    int num_textures = num_textures1 + num_textures2;

    int* map_texture = map_texture1;


    assets->patchy_textures = calloc(num_textures, sizeof(Texture));

    if (!assets->patchy_textures)
    {
        return;
    }

    for (int i = 0; i < num_textures; i++, directory++)
    {
        if (i == num_textures1)
        {
            map_texture = map_texture2;
            directory = map_texture + 1;
        }

        int offset = (int)*directory;

        maptexture_t* mtexture = (maptexture_t*)((unsigned char*)map_texture + offset);
        Texture* ourtexture = &assets->patchy_textures[i];

        strncpy(ourtexture->name, mtexture->name, sizeof(mtexture->name));
        
        int texwidth = mtexture->width;
        int texheight = mtexture->height;
        int patchcount = mtexture->patchcount;

        Image_Create(&ourtexture->img, texwidth, texheight, 4);

        ourtexture->img.is_collumn_stored = true;

        mappatch_t* mpatch = &mtexture->patches[0];

        for (int j = 0; j < patchcount; j++, mpatch++)
        {
            size_t tcoll_offset = 0;
            patch_t* realpatch = MallocLump(file, header, file_infos, 0, patchlookup[mpatch->patch], sizeof(patch_t), NULL);

            if (!realpatch)
            {
                continue;
            }
      
            int x = 0;
            int x1 = mpatch->originx;
            int x2 = x1 + realpatch->width;

            if (x1 < 0)
            {
                x = 0;
            }
            else
            {
                x = x1;
            }
            if (x2 > texwidth)
            {
                x2 = texwidth;
            }
            for (; x < x2; x++)
            {
                column_t* patchcol = (column_t*)((unsigned char*)realpatch + (long)realpatch->columnofs[x - x1]);

                while (patchcol->topdelta != 0xff)
                {
                    unsigned char* src = (unsigned char*)patchcol + 3;
                    int count = patchcol->length;
                    int ystart = mpatch->originy;
                    int position = ystart + patchcol->topdelta;

                    if (position < 0)
                    {
                        count += position;
                        position = 0;
                    }
                    if (position + count > texheight)
                    {
                        count = texheight - position;
                    }

                    if (count > 0)
                    {
                        unsigned int* pix = ourtexture->img.data;
                        if (!ourtexture->img.is_collumn_stored)
                        {
                            for (int y = 0; y < count; y++)
                            {
                                unsigned color = paletteLut[*src];

                                pix[x + (y + position) * ourtexture->img.width] = color;

                                src++;
                            }
                        }
                        else
                        {
                            for (int y = 0; y < count; y++)
                            {
                                unsigned color = paletteLut[*src];

                                size_t index = (x * ourtexture->img.height) + (y + position);

                                pix[index] = color;

                                src++;
                            }
                        }

                    }

                    patchcol = (column_t*)((unsigned char*)patchcol + patchcol->length + 4);
                }
            }

            free(realpatch);
        }
        assets->num_patchy_textures++;

        int j = 1;
        while (j * 2 <= texwidth)
        {
            j <<= 1;
        }
        if (j > texwidth)
        {
            j = texwidth;
        }

        ourtexture->width_mask = j - 1;

        j = 1;
        
        while (j * 2 <= texheight)
        {
            j <<= 1;
        }


     

        ourtexture->height_mask = j - 1;

        if (!strcmp(ourtexture->name, "NUKE24"))
        {
            ourtexture->height_mask = 127;
        }

        //ourtexture->height_mask = texheight - 1;

        //ourtexture->height_mask = texheight - 1;

        //ourtexture->height_mask = 127;
    }


    if (map_texture1) free(map_texture1);
    if (map_texture2) free(map_texture2);
    if (pnames) free(pnames);
    if (patchlookup) free(patchlookup);
}


bool Load_DoomIWAD(const char* filename)
{
    GameAssets* assets = Game_GetAssets();

    bool result = true;

    FILE* file = fopen(filename, "rb");

    if (!file)
    {
        return false;
    }

    wadinfo_t header;
    memset(&header, 0, sizeof(header));

    fread(&header, sizeof(header), 1, file);

    filelump_t* file_infos = calloc(header.numlumps, sizeof(filelump_t));

    if (!file_infos)
    {
        result = false;
        goto cleanup;
    }
    int lump_start = 0;

    fseek(file, header.infotableofs, SEEK_SET);
    fread(file_infos, sizeof(filelump_t) * header.numlumps, 1, file);

    int playpal_index = -1;
    int patch_name_lump_index = -1;
    int texture1_lump_index = -1;
    int texture2_lump_index = -1;
    int flat_start_index = -1;
    int flat_end_index = -1;
    int num_flats = 0;

    char name[12];
    //query indexes of several lumps
    for (int i = 0; i < header.numlumps; i++)
    {
        filelump_t* l = &file_infos[i];
        memset(name, 0, sizeof(name));

        strncpy(name, l->name, 8);

        if (!strcmp(name, "PLAYPAL"))
        {
            playpal_index = i;
        }
        else if (!strcmp(name, "PNAMES"))
        {
            patch_name_lump_index = i;
        }
        else if (!strcmp(name, "TEXTURE1"))
        {
            texture1_lump_index = i;
        }
        else if (!strcmp(name, "TEXTURE2"))
        {
            texture2_lump_index = i;
        }
        else if (!strcmp(name, "F_START"))
        {
            flat_start_index = i + 1;
        }
        else if (!strcmp(name, "F_END"))
        {
            flat_end_index = i - 1;
        }
    }

    unsigned* paletteLut = Load_Pallete(file, &header, file_infos, playpal_index);

    Load_FlatTextures(file, &header, file_infos, flat_start_index, flat_end_index, paletteLut);
    Load_PatchyTextures(file, &header, file_infos, patch_name_lump_index, texture1_lump_index, texture2_lump_index, paletteLut);

    if (file_infos) free(file_infos);
    if (paletteLut) free(paletteLut);
cleanup:
    fclose(file);

    return result;
}