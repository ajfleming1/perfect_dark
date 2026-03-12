#include <ultra64.h>
#include <string.h>
#include "constants.h"
#include "game/pad.h"
#include "bss.h"
#include "data.h"
#include "types.h"

// Helpers for unaligned memory access (PPC alignment fault fix)
static inline f32 readF32Unaligned(const void *ptr)
{
	f32 val;
	memcpy(&val, ptr, sizeof(f32));
	return val;
}

// Helper to read s16 from potentially unaligned memory
static inline s16 readS16Unaligned(const void *ptr)
{
	s16 val;
	memcpy(&val, ptr, sizeof(s16));
	return val;
}

// Helper to read u32 from potentially unaligned memory
static inline u32 readU32Unaligned(const void *ptr)
{
	u32 val;
	memcpy(&val, ptr, sizeof(u32));
	return val;
}

struct padsfileheader *g_PadsFile;
u16 *g_PadOffsets;
u32 var800a2358;
u32 var800a235c;
u16 *g_CoverFlags;
s32 *g_CoverRooms;
struct covercandidate *g_CoverCandidates;
u16 g_NumSpecialCovers;
u16 *g_SpecialCoverNums;

void padUnpack(s32 padnum, u32 fields, struct pad *pad)
{
	s32 offset;
	u32 headerval;
	u8 *ptr;

	if (pad);

	offset = g_PadOffsets[padnum];
	ptr = (u8 *) &g_StageSetup.padfiledata[offset];

	// Use unaligned read for header - pad data may not be 4-byte aligned
	headerval = readU32Unaligned(ptr);

	// Header format:
	// flags, room and liftnum
	// ffffffff ffffffff ffrrrrrr rrrrllll

	if (fields & PADFIELD_ROOM) {
		pad->room = (s32)(headerval << 18) >> 22;
	}

	if (fields & PADFIELD_LIFT) {
		pad->liftnum = headerval & 0x0000000f;
	}

	ptr += 4;

	if ((headerval >> 14) & PADFLAG_INTPOS) {
		if (fields & PADFIELD_POS) {
			pad->pos.x = readS16Unaligned(ptr + 0);
			pad->pos.y = readS16Unaligned(ptr + 2);
			pad->pos.z = readS16Unaligned(ptr + 4);
		}
		ptr += 8;
	} else {
		if (fields & PADFIELD_POS) {
			pad->pos.x = readF32Unaligned(ptr + 0);
			pad->pos.y = readF32Unaligned(ptr + 4);
			pad->pos.z = readF32Unaligned(ptr + 8);
		}
		ptr += 12;
	}

	if ((headerval >> 14) & (PADFLAG_UPALIGNTOX | PADFLAG_UPALIGNTOY | PADFLAG_UPALIGNTOZ)) {
		if (fields & (PADFIELD_UP | PADFIELD_NORMAL)) {
			if ((headerval >> 14) & PADFLAG_UPALIGNTOX) {
				pad->up.x = ((headerval >> 14) & PADFLAG_UPALIGNINVERT) ? -1 : 1;
				pad->up.y = 0;
				pad->up.z = 0;
			} else if ((headerval >> 14) & PADFLAG_UPALIGNTOY) {
				pad->up.x = 0;
				pad->up.y = ((headerval >> 14) & PADFLAG_UPALIGNINVERT) ? -1 : 1;
				pad->up.z = 0;
			} else {
				pad->up.x = 0;
				pad->up.y = 0;
				pad->up.z = ((headerval >> 14) & PADFLAG_UPALIGNINVERT) ? -1 : 1;
			}
		}
	} else {
		if (fields & (PADFIELD_UP | PADFIELD_NORMAL)) {
			pad->up.x = readF32Unaligned(ptr + 0);
			pad->up.y = readF32Unaligned(ptr + 4);
			pad->up.z = readF32Unaligned(ptr + 8);
		}
		ptr += 12;
	}

	if ((headerval >> 14) & (PADFLAG_LOOKALIGNTOX | PADFLAG_LOOKALIGNTOY | PADFLAG_LOOKALIGNTOZ)) {
		if (fields & (PADFIELD_LOOK | PADFIELD_NORMAL)) {
			if ((headerval >> 14) & PADFLAG_LOOKALIGNTOX) {
				pad->look.x = ((headerval >> 14) & PADFLAG_LOOKALIGNINVERT) ? -1 : 1;
				pad->look.y = 0;
				pad->look.z = 0;
			} else if ((headerval >> 14) & PADFLAG_LOOKALIGNTOY) {
				pad->look.x = 0;
				pad->look.y = ((headerval >> 14) & PADFLAG_LOOKALIGNINVERT) ? -1 : 1;
				pad->look.z = 0;
			} else {
				pad->look.x = 0;
				pad->look.y = 0;
				pad->look.z = ((headerval >> 14) & PADFLAG_LOOKALIGNINVERT) ? -1 : 1;
			}
		}
	} else {
		if (fields & (PADFIELD_LOOK | PADFIELD_NORMAL)) {
			pad->look.x = readF32Unaligned(ptr + 0);
			pad->look.y = readF32Unaligned(ptr + 4);
			pad->look.z = readF32Unaligned(ptr + 8);
		}
		ptr += 12;
	}

	if (fields & PADFIELD_NORMAL) {
		pad->normal.x = pad->up.y * pad->look.z - pad->look.y * pad->up.z;
		pad->normal.y = pad->up.z * pad->look.x - pad->look.z * pad->up.x;
		pad->normal.z = pad->up.x * pad->look.y - pad->look.x * pad->up.y;
	}

	if ((headerval >> 14) & PADFLAG_HASBBOXDATA) {
		if (fields & PADFIELD_BBOX) {
			pad->bbox.xmin = readF32Unaligned(ptr + 0);
			pad->bbox.xmax = readF32Unaligned(ptr + 4);
			pad->bbox.ymin = readF32Unaligned(ptr + 8);
			pad->bbox.ymax = readF32Unaligned(ptr + 12);
			pad->bbox.zmin = readF32Unaligned(ptr + 16);
			pad->bbox.zmax = readF32Unaligned(ptr + 20);
		}
		ptr += 4 * 6;
	} else {
		if (fields & PADFIELD_BBOX) {
			pad->bbox.xmin = -100;
			pad->bbox.ymin = -100;
			pad->bbox.zmin = -100;
			pad->bbox.xmax = 100;
			pad->bbox.ymax = 100;
			pad->bbox.zmax = 100;
		}
	}

	if (fields & PADFIELD_FLAGS) {
		pad->flags = (headerval >> 14);
	}
}

bool padHasBboxData(s32 padnum)
{
	u32 offset = g_PadOffsets[padnum];
	u32 headerval = readU32Unaligned(&g_StageSetup.padfiledata[offset]);

	return ((headerval >> 14) & PADFLAG_HASBBOXDATA) != 0;
}

void padGetCentre(s32 padnum, struct coord *coord)
{
	struct pad pad;

	padUnpack(padnum, PADFIELD_POS | PADFIELD_LOOK | PADFIELD_UP | PADFIELD_NORMAL | PADFIELD_BBOX, &pad);

	coord->x = pad.pos.f[0] + (
			(pad.bbox.xmin + pad.bbox.xmax) * pad.normal.f[0] +
			(pad.bbox.ymin + pad.bbox.ymax) * pad.up.f[0] +
			(pad.bbox.zmin + pad.bbox.zmax) * pad.look.f[0]) * 0.5f;

	coord->y = pad.pos.f[1] + (
			(pad.bbox.xmin + pad.bbox.xmax) * pad.normal.f[1] +
			(pad.bbox.ymin + pad.bbox.ymax) * pad.up.f[1] +
			(pad.bbox.zmin + pad.bbox.zmax) * pad.look.f[1]) * 0.5f;

	coord->z = pad.pos.f[2] + (
			(pad.bbox.xmin + pad.bbox.xmax) * pad.normal.f[2] +
			(pad.bbox.ymin + pad.bbox.ymax) * pad.up.f[2] +
			(pad.bbox.zmin + pad.bbox.zmax) * pad.look.f[2]) * 0.5f;
}

/**
 * Some door models are rotated weirdly - suspected to be designed using the
 * wrong coordinate system, then the developers implemented a fix here in the
 * code rather than fixing the models.
 *
 * When such a door is placed on a pad, this function is called. It adjusts the
 * pad's orientation to compensate for the model.
 */
static inline void writeF32Unaligned(void *ptr, f32 val)
{
	memcpy(ptr, &val, sizeof(f32));
}

static inline void writeU32Unaligned(void *ptr, u32 val)
{
	memcpy(ptr, &val, sizeof(u32));
}

void padRotateForDoor(s32 padnum)
{
	u32 stack;
	u8 *ptr;
	u8 *headerptr;
	u32 headerval;
	f32 scale;
	s32 offset;

	offset = g_PadOffsets[padnum];
	ptr = (u8 *) &g_StageSetup.padfiledata[offset];
	headerptr = ptr;
	headerval = readU32Unaligned(ptr);

	ptr += 4;

	if ((headerval >> 14) & PADFLAG_INTPOS) {
		ptr += 8;
	} else {
		ptr += 12;
	}

	if (((headerval >> 14) & (PADFLAG_UPALIGNTOX | PADFLAG_UPALIGNTOY | PADFLAG_UPALIGNTOZ)) == 0) {
		f32 upx = readF32Unaligned(ptr + 0);
		f32 upz = readF32Unaligned(ptr + 8);

		writeF32Unaligned(ptr + 4, 0.0f); // up->y = 0

		scale = 1 / sqrtf(upx * upx + upz * upz);

		writeF32Unaligned(ptr + 0, upx * scale);
		writeF32Unaligned(ptr + 8, upz * scale);

		ptr += 12;
	}

	if ((headerval >> 14) & (PADFLAG_LOOKALIGNTOX | PADFLAG_LOOKALIGNTOY | PADFLAG_LOOKALIGNTOZ)) {
		// Unset the LOOKALIGN flags, then set LOOKALIGNTOY
		headerval = headerval ^ (((headerval >> 14) ^ ((headerval >> 14) & ~(PADFLAG_LOOKALIGNTOX | PADFLAG_LOOKALIGNTOY | PADFLAG_LOOKALIGNTOZ | PADFLAG_LOOKALIGNINVERT))) << 14);
		headerval = headerval ^ (((headerval >> 14) ^ ((headerval >> 14) | PADFLAG_LOOKALIGNTOY)) << 14);
		writeU32Unaligned(headerptr, headerval);
	} else {
		writeF32Unaligned(ptr + 0, 0.0f);
		writeF32Unaligned(ptr + 4, 1.0f);
		writeF32Unaligned(ptr + 8, 0.0f);
	}
}

void padCopyBboxFromPad(s32 padnum, struct pad *src)
{
	u32 offset = g_PadOffsets[padnum];
	u8 *ptr = (u8 *)&g_StageSetup.padfiledata[offset];
	u32 headerval = readU32Unaligned(ptr);

	if ((headerval >> 14) & PADFLAG_HASBBOXDATA) {
		ptr += 4;

		if ((headerval >> 14) & PADFLAG_INTPOS) {
			ptr += 8;
		} else {
			ptr += 12;
		}

		if (((headerval >> 14) & (PADFLAG_UPALIGNTOX | PADFLAG_UPALIGNTOY | PADFLAG_UPALIGNTOZ)) == 0) {
			ptr += 12;
		}

		if (((headerval >> 14) & (PADFLAG_LOOKALIGNTOX | PADFLAG_LOOKALIGNTOY | PADFLAG_LOOKALIGNTOZ)) == 0) {
			ptr += 12;
		}

		writeF32Unaligned(ptr + 0, src->bbox.xmin);
		writeF32Unaligned(ptr + 4, src->bbox.xmax);
		writeF32Unaligned(ptr + 8, src->bbox.ymin);
		writeF32Unaligned(ptr + 12, src->bbox.ymax);
		writeF32Unaligned(ptr + 16, src->bbox.zmin);
		writeF32Unaligned(ptr + 20, src->bbox.zmax);
	}
}

void padSetFlag(s32 padnum, u32 flag)
{
	u32 offset = g_PadOffsets[padnum];
	u8 *ptr = (u8 *)&g_StageSetup.padfiledata[offset];
	u32 headerval = readU32Unaligned(ptr);

	headerval = headerval ^ ((headerval >> 14) ^ ((headerval >> 14) | flag)) << 14;
	writeU32Unaligned(ptr, headerval);
}

void padUnsetFlag(s32 padnum, u32 flag)
{
	u32 offset = g_PadOffsets[padnum];
	u8 *ptr = (u8 *)&g_StageSetup.padfiledata[offset];
	u32 headerval = readU32Unaligned(ptr);

	headerval = headerval ^ ((headerval >> 14) ^ ((headerval >> 14) & ~flag)) << 14;
	writeU32Unaligned(ptr, headerval);
}

bool func0f1162c4(s32 padnum, s32 arg1)
{
	return padnum;
}

s32 coverGetCount(void)
{
	return g_PadsFile->numcovers;
}

bool coverUnpack(s32 covernum, struct cover *cover)
{
	struct coverdefinition *def;

	if (covernum >= g_PadsFile->numcovers || covernum < 0 || !g_StageSetup.cover) {
		return false;
	}

	// @bug: Cast to u8 means it loads the pos, look and flags
	// from an incorrect cover if covernum is greater than 255.
	def = g_StageSetup.cover;
	def += (u8)covernum;

	cover->pos = &def->pos;
	cover->look = &def->look;

	g_CoverFlags[covernum] |= def->flags;

	cover->flags = g_CoverFlags[covernum];
	cover->rooms[0] = g_CoverRooms[covernum];
	cover->rooms[1] = -1;

	return true;
}

u16 getNumSpecialCovers(void)
{
	return g_NumSpecialCovers;
}

bool coverUnpackBySpecialNum(s32 index, struct cover *cover)
{
	// Probable @bug: last check should be index >= g_NumSpecialCovers
	// This function is never called though.
	if (!g_SpecialCoverNums || index < 0 || index > g_NumSpecialCovers) {
		return false;
	}

	if (coverUnpack(g_SpecialCoverNums[index], cover)) {
		return true;
	}

	return false;
}

s32 coverGetNumBySpecialNum(s32 index)
{
	// Probable @bug: last check should be index >= g_NumSpecialCovers
	// This function is never called though.
	if (!g_SpecialCoverNums || index < 0 || index > g_NumSpecialCovers) {
		return -1;
	}

	return g_SpecialCoverNums[index];
}

s32 func0f116450(s32 arg0, s32 arg1)
{
	return arg0;
}

bool coverIsInUse(s32 covernum)
{
	// @bug: Second condition should be >=
	if (covernum < 0 || covernum > g_PadsFile->numcovers) {
		return false;
	}

	return g_CoverFlags[covernum] & COVERFLAG_INUSE;
}

void coverSetInUse(s32 covernum, bool enable)
{
	if (covernum >= 0 && covernum < g_PadsFile->numcovers) {
		if (enable) {
			g_CoverFlags[covernum] |= COVERFLAG_INUSE;
		} else {
			g_CoverFlags[covernum] &= ~COVERFLAG_INUSE;
		}
	}
}

void coverSetFlag(s32 covernum, u32 flag)
{
	g_CoverFlags[covernum] |= flag;
}

void coverUnsetFlag(s32 covernum, u32 flag)
{
	g_CoverFlags[covernum] &= ~flag;
}

void coverSetOutOfSight(s32 covernum, bool enable)
{
	if (covernum >= 0 && covernum < g_PadsFile->numcovers) {
		if (enable) {
			g_CoverFlags[covernum] |= COVERFLAG_OUTOFSIGHT;
		} else {
			g_CoverFlags[covernum] &= ~COVERFLAG_OUTOFSIGHT;
		}
	}
}

bool coverIsSpecial(struct cover *cover)
{
	return (cover->flags & (COVERFLAG_SPECIAL1 | COVERFLAG_SPECIAL2 | COVERFLAG_SPECIAL3)) != 0;
}

s32 func0f1165c0(s32 arg0, s32 arg1)
{
	return arg0;
}
