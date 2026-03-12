#include <stdlib.h>


#include <ultra64.h>
#include <PR/ultrasched.h>
#include "lib/sched.h"
#include "lib/vars.h"
#include "constants.h"
#include "game/camdraw.h"
#include "game/cheats.h"
#include "game/debug.h"
#include "game/file.h"
#include "game/lang.h"
#include "game/race.h"
#include "game/body.h"
#include "game/stubs/game_000840.h"
#include "game/stubs/game_000850.h"
#include "game/stubs/game_000860.h"
#include "game/stubs/game_000870.h"
#include "game/smoke.h"
#include "game/stubs/game_0008e0.h"
#include "game/stubs/game_0008f0.h"
#include "game/stubs/game_000900.h"
#include "game/stubs/game_000910.h"
#include "game/tex.h"
#include "game/stubs/game_00b180.h"
#include "game/stubs/game_00b200.h"
#include "game/challenge.h"
#include "game/title.h"
#include "game/pdmode.h"
#include "game/objectives.h"
#include "game/endscreen.h"
#include "game/playermgr.h"
#include "game/game_1531a0.h"
#include "game/gfxmemory.h"
#include "game/lang.h"
#include "game/lv.h"
#include "game/timing.h"
#include "game/music.h"
#include "game/stubs/game_175f50.h"
#include "game/game_175f90.h"
#include "game/zbuf.h"
#include "game/game_1a78b0.h"
#include "game/mplayer/mplayer.h"
#include "game/pak.h"
#include "game/splat.h"
#include "game/utils.h"
#include "bss.h"
#include "lib/audiomgr.h"
#include "lib/args.h"
#include "lib/boot.h"
#include "lib/vm.h"
#include "lib/rzip.h"
#include "lib/vi.h"
#include "lib/fault.h"
#include "lib/crash.h"
#include "lib/dma.h"
#include "lib/joy.h"
#include "lib/main.h"
#include "lib/snd.h"
#include "lib/memp.h"
#include "lib/mema.h"
#include "lib/model.h"
#include "lib/profile.h"
#include "lib/videbug.h"
#include "lib/debughud.h"
#include "lib/anim.h"
#include "lib/rdp.h"
#include "lib/lib_34d0.h"
#include "lib/lib_2f490.h"
#include "lib/rmon.h"
#include "lib/rng.h"
#include "lib/str.h"
#include "data.h"
#include "types.h"
#include "system.h"

extern u8 *g_MempHeap;
extern u32 g_MempHeapSize;

void rngSetSeed(u32 seed);

bool var8005d9b0 = false;
s32 g_StageNum = STAGE_TITLE;
u32 g_MainMemaHeapSize = 1024 * 300;
bool var8005d9bc = false;
s32 var8005d9c0 = 0;
s32 var8005d9c4 = 0;
bool g_MainGameLogicEnabled = true;
u32 g_MainNumGfxTasks = 0;
bool g_MainIsEndscreen = false;
s32 g_DoBootPakMenu = 0;

u32 var8005dd3c = 0x00000000;
u32 var8005dd40 = 0x00000000;
u32 var8005dd44 = 0x00000000;
u32 var8005dd48 = 0x00000000;
u32 var8005dd4c = 0x00000000;
u32 var8005dd50 = 0x00000000;
s32 g_MainChangeToStageNum = -1;
bool g_MainIsDebugMenuOpen = false;

struct stageallocation g_StageAllocations8Mb[] = {
	{ STAGE_CITRAINING,    "-ml0 -me0 -mgfx120 -mvtx98 -ma400"             },
	{ STAGE_DEFECTION,     "-ml0 -me0 -mgfx110 -mgfxtra80 -mvtx100 -ma700" },
	{ STAGE_INVESTIGATION, "-ml0 -me0 -mgfx110 -mgfxtra80 -mvtx100 -ma700" },
	{ STAGE_EXTRACTION,    "-ml0 -me0 -mgfx110 -mgfxtra80 -mvtx100 -ma500" },
	{ STAGE_CHICAGO,       "-ml0 -me0 -mgfx110 -mgfxtra80 -mvtx100 -ma700" },
	{ STAGE_G5BUILDING,    "-ml0 -me0 -mgfx110 -mgfxtra80 -mvtx100 -ma700" },
	{ STAGE_VILLA,         "-ml0 -me0 -mgfx110 -mgfxtra80 -mvtx100 -ma600" },
	{ STAGE_INFILTRATION,  "-ml0 -me0 -mgfx110 -mgfxtra80 -mvtx100 -ma500" },
	{ STAGE_RESCUE,        "-ml0 -me0 -mgfx110 -mgfxtra80 -mvtx100 -ma500" },
	{ STAGE_ESCAPE,        "-ml0 -me0 -mgfx110 -mgfxtra80 -mvtx100 -ma500" },
	{ STAGE_AIRBASE,       "-ml0 -me0 -mgfx110 -mgfxtra80 -mvtx100 -ma700" },
	{ STAGE_AIRFORCEONE,   "-ml0 -me0 -mgfx110 -mgfxtra80 -mvtx100 -ma700" },
	{ STAGE_CRASHSITE,     "-ml0 -me0 -mgfx110 -mgfxtra80 -mvtx100 -ma700" },
	{ STAGE_PELAGIC,       "-ml0 -me0 -mgfx110 -mgfxtra80 -mvtx100 -ma700" },
	{ STAGE_DEEPSEA,       "-ml0 -me0 -mgfx110 -mgfxtra80 -mvtx100 -ma700" },
	{ STAGE_DEFENSE,       "-ml0 -me0 -mgfx110 -mgfxtra80 -mvtx100 -ma700" },
	{ STAGE_ATTACKSHIP,    "-ml0 -me0 -mgfx110 -mgfxtra80 -mvtx100 -ma700" },
	{ STAGE_SKEDARRUINS,   "-ml0 -me0 -mgfx110 -mgfxtra80 -mvtx100 -ma700" },
	{ STAGE_MP_SKEDAR,     "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_MP_RAVINE,     "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_MP_PIPES,      "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_MP_G5BUILDING, "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_MP_SEWERS,     "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_MP_WAREHOUSE,  "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_MP_BASE,       "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_MP_COMPLEX,    "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_MP_TEMPLE,     "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_MP_FELICITY,   "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_MP_AREA52,     "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_MP_GRID,       "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_MP_CARPARK,    "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_MP_RUINS,      "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_MP_FORTRESS,   "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_MP_VILLA,      "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_TEST_RUN,      "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_TEST_MP2,      "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_TEST_MP6,      "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_TEST_MP7,      "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_TEST_MP8,      "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_TEST_MP14,     "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_TEST_MP16,     "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_TEST_MP17,     "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_TEST_MP18,     "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_TEST_MP19,     "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_TEST_MP20,     "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_TEST_ASH,      "-ml0 -me0 -mgfx120 -mvtx98 -ma400"             },
	{ STAGE_28,            "-ml0 -me0 -mgfx120 -mvtx98 -ma400"             },
	{ STAGE_MBR,           "-ml0 -me0 -mgfx120 -mvtx100 -ma700"            },
	{ STAGE_TEST_SILO,     "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_24,            "-ml0 -me0 -mgfx120 -mvtx98 -ma400"             },
	{ STAGE_MAIANSOS,      "-ml0 -me0 -mgfx120 -mvtx100 -ma500"            },
	{ STAGE_RETAKING,      "-ml0 -me0 -mgfx120 -mvtx98 -ma400"             },
	{ STAGE_TEST_DEST,     "-ml0 -me0 -mgfx120 -mvtx98 -ma400"             },
	{ STAGE_2B,            "-ml0 -me0 -mgfx120 -mvtx98 -ma400"             },
	{ STAGE_WAR,           "-ml0 -me0 -mgfx120 -mvtx98 -ma400"             },
	{ STAGE_TEST_UFF,      "-ml0 -me0 -mgfx120 -mvtx98 -ma400"             },
	{ STAGE_TEST_OLD,      "-ml0 -me0 -mgfx120 -mvtx98 -ma400"             },
	{ STAGE_DUEL,          "-ml0 -me0 -mgfx120 -mvtx100 -ma700"            },
	{ STAGE_TEST_LAM,      "-ml0 -me0 -mgfx120 -mvtx98 -ma400"             },
	{ STAGE_TEST_ARCH,     "-ml0 -me0 -mgfx200 -mvtx200 -ma400"            },
	{ STAGE_TEST_LEN,      "-ml0 -me0 -mgfx120 -mvtx98 -ma300"             },
	{ STAGE_TITLE,         "-ml0 -me0 -mgfx80 -mvtx20 -ma001"              },
	{ 0,                   "-ml0 -me0 -mgfx120 -mvtx98 -ma300"             },
};

struct stageallocation g_StageAllocations4Mb[] = {
	{ STAGE_MP_SKEDAR,     "-ml0 -me0 -mgfx96 -mvtx96 -ma140"              },
	{ STAGE_MP_PIPES,      "-ml0 -me0 -mgfx96 -mvtx96 -ma140"              },
	{ STAGE_MP_AREA52,     "-ml0 -me0 -mgfx96 -mvtx96 -ma140"              },
	{ STAGE_MP_RAVINE,     "-ml0 -me0 -mgfx96 -mvtx96 -ma140"              },
	{ STAGE_MP_G5BUILDING, "-ml0 -me0 -mgfx96 -mvtx96 -ma140"              },
	{ STAGE_MP_SEWERS,     "-ml0 -me0 -mgfx96 -mvtx96 -ma140"              },
	{ STAGE_MP_WAREHOUSE,  "-ml0 -me0 -mgfx96 -mvtx96 -ma140"              },
	{ STAGE_MP_BASE,       "-ml0 -me0 -mgfx96 -mvtx96 -ma140"              },
	{ STAGE_MP_COMPLEX,    "-ml0 -me0 -mgfx96 -mvtx96 -ma140"              },
	{ STAGE_MP_TEMPLE,     "-ml0 -me0 -mgfx96 -mvtx96 -ma140"              },
	{ STAGE_MP_FELICITY,   "-ml0 -me0 -mgfx96 -mvtx96 -ma140"              },
	{ STAGE_MP_GRID,       "-ml0 -me0 -mgfx96 -mvtx96 -ma140"              },
	{ STAGE_TEST_RUN,      "-ml0 -me0 -mgfx96 -mvtx96 -ma140"              },
	{ STAGE_MP_CARPARK,    "-ml0 -me0 -mgfx96 -mvtx96 -ma140"              },
	{ STAGE_MP_RUINS,      "-ml0 -me0 -mgfx96 -mvtx96 -ma140"              },
	{ STAGE_MP_FORTRESS,   "-ml0 -me0 -mgfx96 -mvtx96 -ma130"              },
	{ STAGE_MP_VILLA,      "-ml0 -me0 -mgfx96 -mvtx96 -ma140"              },
	{ STAGE_TEST_MP2,      "-ml0 -me0 -mgfx96 -mvtx96 -ma115"              },
	{ STAGE_TEST_MP6,      "-ml0 -me0 -mgfx96 -mvtx96 -ma115"              },
	{ STAGE_TEST_MP7,      "-ml0 -me0 -mgfx96 -mvtx96 -ma115"              },
	{ STAGE_TEST_MP8,      "-ml0 -me0 -mgfx96 -mvtx96 -ma115"              },
	{ STAGE_TEST_MP14,     "-ml0 -me0 -mgfx96 -mvtx96 -ma115"              },
	{ STAGE_TEST_MP16,     "-ml0 -me0 -mgfx96 -mvtx96 -ma115"              },
	{ STAGE_TEST_MP17,     "-ml0 -me0 -mgfx96 -mvtx96 -ma115"              },
	{ STAGE_TEST_MP18,     "-ml0 -me0 -mgfx96 -mvtx96 -ma115"              },
	{ STAGE_TEST_MP19,     "-ml0 -me0 -mgfx96 -mvtx96 -ma115"              },
	{ STAGE_TEST_MP20,     "-ml0 -me0 -mgfx96 -mvtx96 -ma115"              },
	{ STAGE_TEST_LEN,      "-ml0 -me0 -mgfx100 -mvtx96 -ma120"             },
	{ STAGE_4MBMENU,       "-mgfx100 -mvtx50 -ma50"                        },
	{ STAGE_TITLE,         "-ml0 -me0 -mgfx80 -mvtx20 -ma001"              },
	{ 0,                   "-ml0 -me0 -mgfx100 -mvtx96 -ma300"             },
};

Gfx var8005dcc8[] = {
	gsSPSegment(0x00, 0x00000000),
	gsSPDisplayList(&var800613a0),
	gsSPDisplayList(&var80061380),
	gsDPFullSync(),
	gsSPEndDisplayList(),
};

s32 g_MainIsBooting = 1;

void mainInit(void)
{
	s32 x;
	s32 i;
	s32 j;
	u32 addr;

	sysLogPrintf(LOG_NOTE, "mainInit: faultInit");
	faultInit();
	sysLogPrintf(LOG_NOTE, "mainInit: dmaInit");
	dmaInit();
	sysLogPrintf(LOG_NOTE, "mainInit: amgrInit");
	amgrInit();
	sysLogPrintf(LOG_NOTE, "mainInit: varsInit");
	varsInit();
	sysLogPrintf(LOG_NOTE, "mainInit: mempInit");
	mempInit();
	sysLogPrintf(LOG_NOTE, "mainInit: memaInit");
	memaInit();
	sysLogPrintf(LOG_NOTE, "mainInit: joyInit");
	joyInit();
	sysLogPrintf(LOG_NOTE, "mainInit: joyReset");
	joyReset();

	var8005d9b0 = rmonIsDisabled();

	g_Is4Mb = (osGetMemSize() <= 0x400000);
	g_VmShowStats = 0;

	sysLogPrintf(LOG_NOTE, "mainInit: viSetMode");
	// no copyright screen
	viSetMode(VIMODE_HI);
	sysLogPrintf(LOG_NOTE, "mainInit: viConfigureForLegal");
	viConfigureForLegal();
	sysLogPrintf(LOG_NOTE, "mainInit: viBlack");
	viBlack(true);
	sysLogPrintf(LOG_NOTE, "mainInit: viUpdateMode");
	viUpdateMode();

	sysLogPrintf(LOG_NOTE, "mainInit: filesInit");
	filesInit();

	if (var8005d9b0) {
		argSetString("          -ml0 -me0 -mgfx100 -mvtx50 -mt700 -ma400");
	}

	sysLogPrintf(LOG_NOTE, "mainInit: mempSetHeap");
	mempSetHeap(g_MempHeap, g_MempHeapSize);

	sysLogPrintf(LOG_NOTE, "mainInit: mempResetPool");
	mempResetPool(MEMPOOL_8);
	mempResetPool(MEMPOOL_PERMANENT);
	sysLogPrintf(LOG_NOTE, "mainInit: crashReset");
	crashReset();
	sysLogPrintf(LOG_NOTE, "mainInit: challengesInit");
	challengesInit();
	sysLogPrintf(LOG_NOTE, "mainInit: utilsInit");
	utilsInit();
	sysLogPrintf(LOG_NOTE, "mainInit: texInit");
	texInit();
	sysLogPrintf(LOG_NOTE, "mainInit: langInit");
	langInit();
	sysLogPrintf(LOG_NOTE, "mainInit: lvInit");
	lvInit();
	sysLogPrintf(LOG_NOTE, "mainInit: cheatsInit");
	cheatsInit();
	sysLogPrintf(LOG_NOTE, "mainInit: textInit");
	textInit();
	sysLogPrintf(LOG_NOTE, "mainInit: dhudInit");
	dhudInit();
	sysLogPrintf(LOG_NOTE, "mainInit: playermgrInit");
	playermgrInit();
	sysLogPrintf(LOG_NOTE, "mainInit: frametimeInit");
	frametimeInit();
	sysLogPrintf(LOG_NOTE, "mainInit: profileInit");
	profileInit();
	sysLogPrintf(LOG_NOTE, "mainInit: smokesInit");
	smokesInit();
	sysLogPrintf(LOG_NOTE, "mainInit: mpInit");
	mpInit(true);
	sysLogPrintf(LOG_NOTE, "mainInit: pheadInit");
	pheadInit();
	sysLogPrintf(LOG_NOTE, "mainInit: paksInit");
	paksInit();
	sysLogPrintf(LOG_NOTE, "mainInit: pheadInit2");
	pheadInit2();
	sysLogPrintf(LOG_NOTE, "mainInit: animsInit");
	animsInit();
	sysLogPrintf(LOG_NOTE, "mainInit: racesInit");
	racesInit();
	sysLogPrintf(LOG_NOTE, "mainInit: bodiesInit");
	bodiesInit();
	sysLogPrintf(LOG_NOTE, "mainInit: titleInit");
	titleInit();

	modelSetDistanceChecksDisabled(true); // don't use LODs

	sysLogPrintf(LOG_NOTE, "mainInit: complete");
	g_MainIsBooting = 0;
}

FILE *g_dbglog = NULL;

void mainProc(void)
{
	FILE *dbglog = g_dbglog;

	sysLogPrintf(LOG_NOTE, "mainProc: calling mainInit");
	if (dbglog) fprintf(dbglog, "mainProc: calling mainInit\n"), fflush(dbglog);
	mainInit();
	if (dbglog) fprintf(dbglog, "mainProc: mainInit done\n"), fflush(dbglog);

	sysLogPrintf(LOG_NOTE, "mainProc: mainInit done, calling rdpInit");
	if (dbglog) fprintf(dbglog, "mainProc: calling rdpInit\n"), fflush(dbglog);
	rdpInit();
	if (dbglog) fprintf(dbglog, "mainProc: rdpInit done\n"), fflush(dbglog);

	sysLogPrintf(LOG_NOTE, "mainProc: rdpInit done, calling sndInit");
	if (dbglog) fprintf(dbglog, "mainProc: calling sndInit\n"), fflush(dbglog);
	sndInit();
	if (dbglog) fprintf(dbglog, "mainProc: sndInit done, entering main loop\n"), fflush(dbglog);

	sysLogPrintf(LOG_NOTE, "mainProc: sndInit done, entering main loop");

	if (dbglog) fprintf(dbglog, "mainProc: entering main loop\n"), fflush(dbglog);
	while (true) {
		if (dbglog) fprintf(dbglog, "mainLoop iteration\n"), fflush(dbglog);
		mainLoop();
	}
}

/**
 * It's suspected that this function would have allowed developers to override
 * the value of variables while the game is running in order to view their
 * effects immediately rather than having to recompile the game each time.
 *
 * The developers would have used rmon to create a table of name/value pairs,
 * then this function would have looked up the given variable name in the table
 * and written the new value to the variable's address.
 */
void mainOverrideVariable(char *name, void *value)
{
	// empty
}

/**
 * This function enters an infinite loop which iterates once per stage load.
 * Within this loop is an inner loop which runs very frequently and decides
 * whether to run mainTick on each iteration.
 *
 * NTSC beta checks two shorts at an offset 64MB into the development board
 * and refuses to continue if they are not any of the allowed values.
 * Decomp patches these reads in its build system so it can be played
 * without the development board.
 */
void mainLoop(void)
{
	extern FILE *g_dbglog;
	s32 ending = false;
	s32 index;
	s32 numplayers;
	u32 stack;

	if (g_dbglog) fprintf(g_dbglog, "mainLoop: start\n"), fflush(g_dbglog);

	sysLogPrintf(LOG_NOTE, "mainLoop: calling func0f175f98");
	if (g_dbglog) fprintf(g_dbglog, "mainLoop: calling func0f175f98\n"), fflush(g_dbglog);
	func0f175f98();
	if (g_dbglog) fprintf(g_dbglog, "mainLoop: func0f175f98 done\n"), fflush(g_dbglog);
	sysLogPrintf(LOG_NOTE, "mainLoop: func0f175f98 done");

	if (g_dbglog) fprintf(g_dbglog, "mainLoop: setting var8005d9c4=0\n"), fflush(g_dbglog);
	var8005d9c4 = 0;

	sysLogPrintf(LOG_NOTE, "mainLoop: calling argGetLevel");
	if (g_dbglog) fprintf(g_dbglog, "mainLoop: calling argGetLevel\n"), fflush(g_dbglog);
	argGetLevel(&g_StageNum);
	if (g_dbglog) fprintf(g_dbglog, "mainLoop: argGetLevel done, stage=%d\n", g_StageNum), fflush(g_dbglog);
	sysLogPrintf(LOG_NOTE, "mainLoop: argGetLevel done, stage=%d", g_StageNum);

	if (g_DoBootPakMenu) {
		g_Vars.pakstocheck = 0xfd;
		g_StageNum = STAGE_BOOTPAKMENU;
	}

	if (g_StageNum != STAGE_TITLE) {
		titleSetNextStage(g_StageNum);

		if (g_StageNum < STAGE_TITLE) {
			func0f01b148(0);

			if (argFindByPrefix(1, "-hard")) {
				lvSetDifficulty(argFindByPrefix(1, "-hard")[0] - '0');
			}
		}
	}

	if (g_StageNum == STAGE_CITRAINING && IS4MB()) {
		g_StageNum = STAGE_4MBMENU;
	}

	if (g_dbglog) fprintf(g_dbglog, "mainLoop: calling rngSetSeed\n"), fflush(g_dbglog);
	sysLogPrintf(LOG_NOTE, "mainLoop: calling rngSetSeed");
	rngSetSeed(osGetCount());
	if (g_dbglog) fprintf(g_dbglog, "mainLoop: rngSetSeed done, entering outer loop\n"), fflush(g_dbglog);
	sysLogPrintf(LOG_NOTE, "mainLoop: entering outer loop");

	// Outer loop - this is infinite because ending is never changed
	while (!ending) {
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: outer loop iteration start\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: outer loop iteration start");
		g_MainNumGfxTasks = 0;
		g_MainGameLogicEnabled = true;
		g_MainIsEndscreen = false;
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: flags set, var8005d9b0=%d var8005d9c4=%d\n", var8005d9b0, var8005d9c4), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: flags set, var8005d9b0=%d var8005d9c4=%d", var8005d9b0, var8005d9c4);

		if (var8005d9b0 && var8005d9c4 == 0) {
			if (g_dbglog) fprintf(g_dbglog, "mainLoop: entering var8005d9b0 block\n"), fflush(g_dbglog);
			index = -1;

			if (g_dbglog) fprintf(g_dbglog, "mainLoop: calling IS4MB\n"), fflush(g_dbglog);
			if (IS4MB()) {
				if (g_dbglog) fprintf(g_dbglog, "mainLoop: IS4MB returned true\n"), fflush(g_dbglog);
				if (g_StageNum < STAGE_TITLE && getNumPlayers() >= 2) {
					index = 0; \
					while (g_StageAllocations4Mb[index].stagenum) { \
						if (g_StageAllocations4Mb[index].stagenum == g_StageNum + 400) { \
							break; \
						} \
						index++;
					}

					if (g_StageAllocations4Mb[index].stagenum == 0) {
						index = -1;
					}
				}

				if (index);

				if (index < 0) {
					index = 0;
					while (g_StageAllocations4Mb[index].stagenum) {
						if (g_StageNum == g_StageAllocations4Mb[index].stagenum) {
							break;
						}

						index++;
					}
				}

				argSetString(g_StageAllocations4Mb[index].string);
			} else {
				// 8MB
				if (g_dbglog) fprintf(g_dbglog, "mainLoop: IS4MB returned false (8MB)\n"), fflush(g_dbglog);
				if (g_StageNum < STAGE_TITLE && getNumPlayers() >= 2) {
					index = 0; \
					while (g_StageAllocations8Mb[index].stagenum) { \
						if (g_StageNum + 400 == g_StageAllocations8Mb[index].stagenum) { \
							break; \
						} \
						index++;
					}

					if (g_StageAllocations8Mb[index].stagenum == 0) {
						index = -1;
					}
				}

				if (index < 0) {
					index = 0;

					while (g_StageAllocations8Mb[index].stagenum) {
						if (g_StageNum == g_StageAllocations8Mb[index].stagenum) {
							break;
						}

						index++;
					}
				}

				argSetString(g_StageAllocations8Mb[index].string);
			}
		}

		if (g_dbglog) fprintf(g_dbglog, "mainLoop: completed var8005d9b0 block, calling mempResetPool\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: after stage alloc, calling mempResetPool");
		var8005d9c4 = 0;

		if (g_dbglog) fprintf(g_dbglog, "mainLoop: calling mempResetPool MEMPOOL_7\n"), fflush(g_dbglog);
		mempResetPool(MEMPOOL_7);
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: calling mempResetPool MEMPOOL_STAGE\n"), fflush(g_dbglog);
		mempResetPool(MEMPOOL_STAGE);
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: mempResetPool done\n"), fflush(g_dbglog);

		if (g_dbglog) fprintf(g_dbglog, "mainLoop: calling filesStop(4)\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: calling filesStop");
		filesStop(4);
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: filesStop done\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: filesStop done");

		if (argFindByPrefix(1, "-ma")) {
			g_MainMemaHeapSize = strtol(argFindByPrefix(1, "-ma"), NULL, 0) * 1024;
		}

		if (g_dbglog) fprintf(g_dbglog, "mainLoop: calling memaReset\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: calling memaReset");
		memaReset(mempAlloc(g_MainMemaHeapSize, MEMPOOL_STAGE), g_MainMemaHeapSize);
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: calling langReset\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: calling langReset");
		langReset(g_StageNum);
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: calling playermgrReset\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: calling playermgrReset");
		playermgrReset();
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: playermgrReset done\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: playermgrReset done, g_StageNum=%d STAGE_TITLE=%d", g_StageNum, STAGE_TITLE);

		if (g_StageNum >= STAGE_TITLE) {
			sysLogPrintf(LOG_NOTE, "mainLoop: numplayers=0 (title screen)");
			numplayers = 0;
		} else {
			if (argFindByPrefix(1, "-play")) {
				numplayers = strtol(argFindByPrefix(1, "-play"), NULL, 0);
			} else {
				numplayers = 1;
			}

			if (getNumPlayers() >= 2) {
				numplayers = getNumPlayers();
			}
		}

		if (numplayers < 2) {
			g_Vars.bondplayernum = 0;
			g_Vars.coopplayernum = -1;
			g_Vars.antiplayernum = -1;
		} else if (argFindByPrefix(1, "-coop")) {
			g_Vars.bondplayernum = 0;
			g_Vars.coopplayernum = 1;
			g_Vars.antiplayernum = -1;
		} else if (argFindByPrefix(1, "-anti")) {
			g_Vars.bondplayernum = 0;
			g_Vars.coopplayernum = -1;
			g_Vars.antiplayernum = 1;
		}

		if (g_dbglog) fprintf(g_dbglog, "mainLoop: calling playermgrAllocatePlayers\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: calling playermgrAllocatePlayers(%d)", numplayers);
		playermgrAllocatePlayers(numplayers);
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: playermgrAllocatePlayers done\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: playermgrAllocatePlayers done");

		if (argFindByPrefix(1, "-mpbots")) {
			g_Vars.lvmpbotlevel = 1;
		}

		if (g_Vars.coopplayernum >= 0 || g_Vars.antiplayernum >= 0) {
			if (g_MpSetup.chrslots & 0xfff0) {
				g_MpSetup.storedbotbits = g_MpSetup.chrslots & 0xfff0;
			}
			g_MpSetup.chrslots = 0x03;
			mpReset();
		} else if (g_Vars.perfectbuddynum) {
			mpReset();
		} else if (g_Vars.mplayerisrunning == false
				&& (numplayers >= 2 || g_Vars.lvmpbotlevel || argFindByPrefix(1, "-play"))) {
			g_MpSetup.chrslots = 1;

			if (numplayers >= 2) {
				g_MpSetup.chrslots |= 1 << 1;
			}

			if (numplayers >= 3) {
				g_MpSetup.chrslots |= 1 << 2;
			}

			if (numplayers >= 4) {
				g_MpSetup.chrslots |= 1 << 3;
			}

			g_MpSetup.stagenum = g_StageNum;
			mpReset();
		}

		if (g_dbglog) fprintf(g_dbglog, "mainLoop: calling gfxReset\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: calling gfxReset");
		gfxReset();
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: gfxReset done, calling joyReset\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: calling joyReset");
		joyReset();
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: joyReset done, calling dhudReset\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: calling dhudReset");
		dhudReset();
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: dhudReset done, calling zbufReset\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: calling zbufReset");
		zbufReset(g_StageNum);
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: zbufReset done, calling lvReset\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: calling lvReset");
		lvReset(g_StageNum);
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: lvReset done, calling viReset\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: calling viReset");
		viReset(g_StageNum);
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: viReset done, calling frametimeCalculate\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: calling frametimeCalculate");
		frametimeCalculate();
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: frametimeCalculate done, calling profileReset\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: frametimeCalculate done");
		profileReset();
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: profileReset done, entering inner loop\n"), fflush(g_dbglog);
		sysLogPrintf(LOG_NOTE, "mainLoop: profileReset done, entering inner loop");

		if (g_dbglog) fprintf(g_dbglog, "mainLoop: inner loop start, g_MainChangeToStageNum=%d\n", g_MainChangeToStageNum), fflush(g_dbglog);
		while (g_MainChangeToStageNum < 0) {
			if (g_dbglog) fprintf(g_dbglog, "mainLoop: inner loop iteration, g_MainChangeToStageNum=%d\n", g_MainChangeToStageNum), fflush(g_dbglog);
			const s32 cycles = osGetCount() - g_Vars.thisframestartt;
			if (!g_Vars.mininc60 || (cycles >= g_Vars.mininc60 * CYCLES_PER_FRAME - CYCLES_PER_FRAME / 2)) {
				if (g_dbglog) fprintf(g_dbglog, "mainLoop: calling schedStartFrame\n"), fflush(g_dbglog);
				sysLogPrintf(LOG_NOTE, "mainLoop: calling schedStartFrame");
				schedStartFrame(&g_Sched);
				if (g_dbglog) fprintf(g_dbglog, "mainLoop: calling mainTick\n"), fflush(g_dbglog);
				sysLogPrintf(LOG_NOTE, "mainLoop: calling mainTick");
				mainTick();
				if (g_dbglog) fprintf(g_dbglog, "mainLoop: mainTick done\n"), fflush(g_dbglog);
				sysLogPrintf(LOG_NOTE, "mainLoop: mainTick done");
				schedEndFrame(&g_Sched);
				if (g_dbglog) fprintf(g_dbglog, "mainLoop: schedEndFrame done\n"), fflush(g_dbglog);
			}
			if (g_TickExtraSleep) {
				sysSleep(EXTRA_SLEEP_TIME);
			}
		}
		if (g_dbglog) fprintf(g_dbglog, "mainLoop: exited inner loop\n"), fflush(g_dbglog);

		lvStop();
		mempDisablePool(MEMPOOL_STAGE);
		mempDisablePool(MEMPOOL_7);
		filesStop(4);
		viBlack(true);
		pak0f116994();

		g_StageNum = g_MainChangeToStageNum;
		g_MainChangeToStageNum = -1;
	}
}

void mainTick(void)
{
	Gfx *gdl = NULL;
	Gfx *gdlstart = NULL;
	OSScMsg msg = {OS_SC_DONE_MSG};
	s32 i;

	sysLogPrintf(LOG_NOTE, "mainTick: entry");

	if (g_MainChangeToStageNum < 0) {
		frametimeCalculate();
		profileReset();
		profileSetMarker(PROFILE_MAINTICK_START);
		joyDebugJoy();
		schedSetCrashEnable2(false);

		sysLogPrintf(LOG_NOTE, "mainTick: g_MainGameLogicEnabled=%d", g_MainGameLogicEnabled);

		if (g_MainGameLogicEnabled) {
#ifdef PLATFORM_WIIU
#endif
			sysLogPrintf(LOG_NOTE, "mainTick: calling gfxGetMasterDisplayList");
			gdl = gdlstart = gfxGetMasterDisplayList();
#ifdef PLATFORM_WIIU
#endif
			sysLogPrintf(LOG_NOTE, "mainTick: gdl=%p", gdl);

			gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0x0000, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);
			gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_4b, 0, 0x0100, 6, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOLOD);

#ifdef PLATFORM_WIIU
#endif
			sysLogPrintf(LOG_NOTE, "mainTick: calling lvTick");
			lvTick();
#ifdef PLATFORM_WIIU
#endif
			sysLogPrintf(LOG_NOTE, "mainTick: calling playermgrShuffle");
			playermgrShuffle();
#ifdef PLATFORM_WIIU
#endif

			if (g_StageNum < STAGE_TITLE) {
				for (i = 0; i < PLAYERCOUNT(); i++) {
					setCurrentPlayerNum(playermgrGetPlayerAtOrder(i));

					if (g_StageNum != STAGE_TEST_OLD || !titleIsKeepingMode()) {
						viSetViewPosition(g_Vars.currentplayer->viewleft, g_Vars.currentplayer->viewtop);
						viSetFovAspectAndSize(
								g_Vars.currentplayer->fovy, g_Vars.currentplayer->aspect,
								g_Vars.currentplayer->viewwidth, g_Vars.currentplayer->viewheight);
					}

					lvTickPlayer();
				}
			}

#ifdef PLATFORM_WIIU
#endif
			sysLogPrintf(LOG_NOTE, "mainTick: calling lvRender");
			gdl = lvRender(gdl);
#ifdef PLATFORM_WIIU
#endif
			sysLogPrintf(LOG_NOTE, "mainTick: lvRender done");

			if (debugGetProfileMode() >= 2) {
				gdl = profileRender(gdl);
			}

			gDPFullSync(gdl++);
			gSPEndDisplayList(gdl++);
		}

#ifdef PLATFORM_WIIU
#endif
		sysLogPrintf(LOG_NOTE, "mainTick: calling gfxSwapBuffers");
		if (g_MainGameLogicEnabled) {
			gfxSwapBuffers();
#ifdef PLATFORM_WIIU
#endif
			sysLogPrintf(LOG_NOTE, "mainTick: gfxSwapBuffers done, calling viUpdateMode");
			viUpdateMode();
#ifdef PLATFORM_WIIU
#endif
			sysLogPrintf(LOG_NOTE, "mainTick: viUpdateMode done");
		}

#ifdef PLATFORM_WIIU
#endif
		sysLogPrintf(LOG_NOTE, "mainTick: calling rdpCreateTask");
		rdpCreateTask(gdlstart, gdl, 0, (uintptr_t) &msg);
#ifdef PLATFORM_WIIU
#endif
		memaPrint();
		profileSetMarker(PROFILE_MAINTICK_END);
		sysLogPrintf(LOG_NOTE, "mainTick: done");
	}
}

void mainEndStage(void)
{
	sndStopNosedive();

	if (!g_MainIsEndscreen) {
		pak0f11c6d0();
		joyDisableTemporarily();

		if (g_Vars.coopplayernum >= 0) {
			s32 prevplayernum = g_Vars.currentplayernum;
			s32 i;

			for (i = 0; i < PLAYERCOUNT(); i++) {
				setCurrentPlayerNum(i);
				endscreenPushCoop();
			}

			setCurrentPlayerNum(prevplayernum);
			musicStartMenu();
		} else if (g_Vars.antiplayernum >= 0) {
			s32 prevplayernum = g_Vars.currentplayernum;
			s32 i;

			for (i = 0; i < PLAYERCOUNT(); i++) {
				setCurrentPlayerNum(i);
				endscreenPushAnti();
			}

			setCurrentPlayerNum(prevplayernum);
			musicStartMenu();
		} else if (g_Vars.normmplayerisrunning) {
			mpEndMatch();
		} else {
			endscreenPrepare();
			musicStartMenu();
		}
	}

	g_MainIsEndscreen = true;
}

/**
 * Change to the given stage at the end of the current frame.
 */
void mainChangeToStage(s32 stagenum)
{
	pak0f11c6d0();

	g_MainChangeToStageNum = stagenum;
}

s32 mainGetStageNum(void)
{
	return g_StageNum;
}

void func0000e990(void)
{
	objectivesCheckAll();
	objectivesDisableChecking();
	mainEndStage();
}
