#include "global.h"
#include "randomizer_menu.h"
#include "bg.h"
#include "gpu_regs.h"
#include "main.h"
#include "menu.h"
#include "palette.h"
#include "randomization.h"
#include "scanline_effect.h"
#include "sprite.h"
#include "task.h"
#include "text.h"
#include "text_window.h"
#include "window.h"
#include "constants/rgb.h"

// Shown after selecting NEW GAME, before the Birch intro. The chosen
// toggles are stored as pending choices that InitializeRandomization
// applies when the new save is created.

#define tMenuSelection data[0]
#define tEncounters    data[1]
#define tTrainers      data[2]
#define tAbilities     data[3]
#define tMoves         data[4]
#define tItems         data[5]
#define tSimilarStats  data[6]

enum
{
    MENUITEM_ENCOUNTERS,
    MENUITEM_TRAINERS,
    MENUITEM_ABILITIES,
    MENUITEM_MOVES,
    MENUITEM_ITEMS,
    MENUITEM_SIMILAR_STATS,
    MENUITEM_START,
    MENUITEM_COUNT,
};

enum
{
    WIN_HEADER,
    WIN_OPTIONS
};

static void Task_RandomizerMenuFadeIn(u8 taskId);
static void Task_RandomizerMenuProcessInput(u8 taskId);
static void Task_RandomizerMenuConfirm(u8 taskId);
static void Task_RandomizerMenuFadeOut(u8 taskId);
static void HighlightMenuItem(u8 selection);
static u8 OnOff_ProcessInput(u8 selection);
static void OnOff_DrawChoices(u8 item, u8 selection);
static void DrawHeaderText(void);
static void DrawMenuTexts(void);
static void DrawBgWindowFrames(void);

static const u8 sText_On[]  = _("{COLOR GREEN}{SHADOW LIGHT_GREEN}ON");
static const u8 sText_Off[] = _("{COLOR GREEN}{SHADOW LIGHT_GREEN}OFF");

static const u8 *const sMenuItemNames[MENUITEM_COUNT] =
{
    [MENUITEM_ENCOUNTERS]    = COMPOUND_STRING("ENCOUNTERS"),
    [MENUITEM_TRAINERS]      = COMPOUND_STRING("TRAINERS"),
    [MENUITEM_ABILITIES]     = COMPOUND_STRING("ABILITIES"),
    [MENUITEM_MOVES]         = COMPOUND_STRING("MOVES"),
    [MENUITEM_ITEMS]         = COMPOUND_STRING("ITEMS"),
    [MENUITEM_SIMILAR_STATS] = COMPOUND_STRING("SIMILAR POWER"),
    [MENUITEM_START]         = COMPOUND_STRING("START GAME"),
};

static const struct WindowTemplate sRandomizerMenuWinTemplates[] =
{
    [WIN_HEADER] = {
        .bg = 1,
        .tilemapLeft = 2,
        .tilemapTop = 1,
        .width = 26,
        .height = 2,
        .paletteNum = 1,
        .baseBlock = 2
    },
    [WIN_OPTIONS] = {
        .bg = 0,
        .tilemapLeft = 2,
        .tilemapTop = 5,
        .width = 26,
        .height = 14,
        .paletteNum = 1,
        .baseBlock = 0x36
    },
    DUMMY_WIN_TEMPLATE
};

static const struct BgTemplate sRandomizerMenuBgTemplates[] =
{
    {
        .bg = 1,
        .charBaseIndex = 1,
        .mapBaseIndex = 30,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0
    },
    {
        .bg = 0,
        .charBaseIndex = 1,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0
    }
};

static const u16 sRandomizerMenuBg_Pal[] = {RGB(17, 18, 31)};
static const u16 sRandomizerMenuText_Pal[] = INCGFX_U16("graphics/interface/option_menu_text.pal", ".gbapal");

static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

void CB2_InitRandomizerMenu(void)
{
    switch (gMain.state)
    {
    default:
    case 0:
        SetVBlankCallback(NULL);
        gMain.state++;
        break;
    case 1:
        DmaClearLarge16(3, (void *)(VRAM), VRAM_SIZE, 0x1000);
        DmaClear32(3, OAM, OAM_SIZE);
        DmaClear16(3, PLTT, PLTT_SIZE);
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sRandomizerMenuBgTemplates, ARRAY_COUNT(sRandomizerMenuBgTemplates));
        ChangeBgX(0, 0, BG_COORD_SET);
        ChangeBgY(0, 0, BG_COORD_SET);
        ChangeBgX(1, 0, BG_COORD_SET);
        ChangeBgY(1, 0, BG_COORD_SET);
        InitWindows(sRandomizerMenuWinTemplates);
        DeactivateAllTextPrinters();
        SetGpuReg(REG_OFFSET_WIN0H, 0);
        SetGpuReg(REG_OFFSET_WIN0V, 0);
        SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_BG0);
        SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG0 | WINOUT_WIN01_BG1 | WINOUT_WIN01_CLR);
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG0 | BLDCNT_EFFECT_DARKEN);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 4);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
        ShowBg(0);
        ShowBg(1);
        gMain.state++;
        break;
    case 2:
        ResetPaletteFade();
        ScanlineEffect_Stop();
        ResetTasks();
        ResetSpriteData();
        gMain.state++;
        break;
    case 3:
        LoadBgTiles(1, GetWindowFrameTilesPal(gSaveBlock2Ptr->optionsWindowFrameType)->tiles, 0x120, 0x1A2);
        gMain.state++;
        break;
    case 4:
        LoadPalette(sRandomizerMenuBg_Pal, BG_PLTT_ID(0), sizeof(sRandomizerMenuBg_Pal));
        LoadPalette(GetWindowFrameTilesPal(gSaveBlock2Ptr->optionsWindowFrameType)->pal, BG_PLTT_ID(7), PLTT_SIZE_4BPP);
        gMain.state++;
        break;
    case 5:
        LoadPalette(sRandomizerMenuText_Pal, BG_PLTT_ID(1), sizeof(sRandomizerMenuText_Pal));
        gMain.state++;
        break;
    case 6:
        PutWindowTilemap(WIN_HEADER);
        DrawHeaderText();
        gMain.state++;
        break;
    case 7:
        PutWindowTilemap(WIN_OPTIONS);
        DrawMenuTexts();
        gMain.state++;
        break;
    case 8:
        DrawBgWindowFrames();
        gMain.state++;
        break;
    case 9:
    {
        u8 taskId = CreateTask(Task_RandomizerMenuFadeIn, 0);
        u8 i;

        gTasks[taskId].tMenuSelection = 0;
        gTasks[taskId].tEncounters = FALSE;
        gTasks[taskId].tTrainers = FALSE;
        gTasks[taskId].tAbilities = FALSE;
        gTasks[taskId].tMoves = FALSE;
        gTasks[taskId].tItems = FALSE;
        gTasks[taskId].tSimilarStats = TRUE;

        for (i = MENUITEM_ENCOUNTERS; i <= MENUITEM_SIMILAR_STATS; i++)
            OnOff_DrawChoices(i, gTasks[taskId].data[1 + i]);
        HighlightMenuItem(gTasks[taskId].tMenuSelection);

        CopyWindowToVram(WIN_OPTIONS, COPYWIN_FULL);
        gMain.state++;
        break;
    }
    case 10:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB);
        SetMainCallback2(MainCB2);
        return;
    }
}

static void Task_RandomizerMenuFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_RandomizerMenuProcessInput;
}

static void Task_RandomizerMenuProcessInput(u8 taskId)
{
    if (JOY_NEW(A_BUTTON) && gTasks[taskId].tMenuSelection == MENUITEM_START)
    {
        gTasks[taskId].func = Task_RandomizerMenuConfirm;
    }
    else if (JOY_NEW(DPAD_UP))
    {
        if (gTasks[taskId].tMenuSelection > 0)
            gTasks[taskId].tMenuSelection--;
        else
            gTasks[taskId].tMenuSelection = MENUITEM_START;
        HighlightMenuItem(gTasks[taskId].tMenuSelection);
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        if (gTasks[taskId].tMenuSelection < MENUITEM_START)
            gTasks[taskId].tMenuSelection++;
        else
            gTasks[taskId].tMenuSelection = 0;
        HighlightMenuItem(gTasks[taskId].tMenuSelection);
    }
    else if (gTasks[taskId].tMenuSelection <= MENUITEM_SIMILAR_STATS)
    {
        u8 item = gTasks[taskId].tMenuSelection;
        u8 previousOption = gTasks[taskId].data[1 + item];
        gTasks[taskId].data[1 + item] = OnOff_ProcessInput(previousOption);
        if (previousOption != gTasks[taskId].data[1 + item])
        {
            OnOff_DrawChoices(item, gTasks[taskId].data[1 + item]);
            CopyWindowToVram(WIN_OPTIONS, COPYWIN_GFX);
        }
    }
}

static void Task_RandomizerMenuConfirm(u8 taskId)
{
    u8 flags = 0;

    if (gTasks[taskId].tEncounters)
        flags |= RANDOMIZATION_FLAG_WILD | RANDOMIZATION_FLAG_STATIC | RANDOMIZATION_FLAG_TRADES;
    if (gTasks[taskId].tTrainers)
        flags |= RANDOMIZATION_FLAG_TRAINERS;
    if (gTasks[taskId].tAbilities)
        flags |= RANDOMIZATION_FLAG_ABILITIES;
    if (gTasks[taskId].tMoves)
        flags |= RANDOMIZATION_FLAG_MOVES;
    if (gTasks[taskId].tItems)
        flags |= RANDOMIZATION_FLAG_ITEMS;

#if RANDOMIZATION_ENABLED == TRUE
    Randomizer_SetPendingChoices(flags, gTasks[taskId].tSimilarStats);
#endif

    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    gTasks[taskId].func = Task_RandomizerMenuFadeOut;
}

static void Task_RandomizerMenuFadeOut(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        FreeAllWindowBuffers();
        SetMainCallback2(gMain.savedCallback);
    }
}

static void HighlightMenuItem(u8 index)
{
    SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(16, DISPLAY_WIDTH - 16));
    SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE(index * 16 + 40, index * 16 + 56));
}

static u8 OnOff_ProcessInput(u8 selection)
{
    if (JOY_NEW(DPAD_RIGHT) || JOY_NEW(DPAD_LEFT) || JOY_NEW(A_BUTTON))
        selection = !selection;
    return selection;
}

static void DrawMenuChoice(const u8 *text, u8 x, u8 y, u8 style)
{
    u8 dst[16];
    u16 i;

    for (i = 0; *text != EOS && i < ARRAY_COUNT(dst) - 1; i++)
        dst[i] = *(text++);

    if (style != 0)
    {
        dst[2] = TEXT_COLOR_RED;
        dst[5] = TEXT_COLOR_LIGHT_RED;
    }

    dst[i] = EOS;
    AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, dst, x, y + 1, TEXT_SKIP_DRAW, NULL);
}

static void OnOff_DrawChoices(u8 item, u8 selection)
{
    u8 y = item * 16;

    // Clear the choice area before redrawing
    FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(1), 104, y, 104, 16);
    DrawMenuChoice(sText_On, 104, y, selection ? 1 : 0);
    DrawMenuChoice(sText_Off, 144, y, selection ? 0 : 1);
}

static void DrawHeaderText(void)
{
    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(1));
    AddTextPrinterParameterized(WIN_HEADER, FONT_NORMAL, COMPOUND_STRING("RANDOMIZER SETUP"), 8, 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);
}

static void DrawMenuTexts(void)
{
    u8 i;

    FillWindowPixelBuffer(WIN_OPTIONS, PIXEL_FILL(1));
    for (i = 0; i < MENUITEM_COUNT; i++)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, sMenuItemNames[i], 8, (i * 16) + 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(WIN_OPTIONS, COPYWIN_FULL);
}

#define TILE_TOP_CORNER_L 0x1A2
#define TILE_TOP_EDGE     0x1A3
#define TILE_TOP_CORNER_R 0x1A4
#define TILE_LEFT_EDGE    0x1A5
#define TILE_RIGHT_EDGE   0x1A7
#define TILE_BOT_CORNER_L 0x1A8
#define TILE_BOT_EDGE     0x1A9
#define TILE_BOT_CORNER_R 0x1AA

static void DrawBgWindowFrames(void)
{
    //                     bg, tile,              x, y, width, height, palNum
    // Draw title window frame
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_L,  1,  0,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_EDGE,      2,  0, 27,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_R, 28,  0,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_LEFT_EDGE,     1,  1,  1,  2,  7);
    FillBgTilemapBufferRect(1, TILE_RIGHT_EDGE,   28,  1,  1,  2,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_L,  1,  3,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_EDGE,      2,  3, 27,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_R, 28,  3,  1,  1,  7);

    // Draw options list window frame
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_L,  1,  4,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_EDGE,      2,  4, 26,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_R, 28,  4,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_LEFT_EDGE,     1,  5,  1, 18,  7);
    FillBgTilemapBufferRect(1, TILE_RIGHT_EDGE,   28,  5,  1, 18,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_L,  1, 19,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_EDGE,      2, 19, 26,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_R, 28, 19,  1,  1,  7);

    CopyBgTilemapBufferToVram(1);
}
