#include "global.h"
#include "bg.h"
#include "decompress.h"
#include "international_string_util.h"
#include "main.h"
#include "menu.h"
#include "palette.h"
#include "pokedex.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "sound.h"
#include "sprite.h"
#include "start_menu.h"
#include "start_menu_hud.h"
#include "string_util.h"
#include "strings.h"
#include "text.h"
#include "window.h"
#include "constants/battle.h"
#include "constants/rgb.h"
#include "constants/songs.h"

// Icon-based start menu overlay, drawn over the live field while the start menu
// is open. The field itself is untouched: the panels are BG0 windows and the
// icons are sprites at OAM priority 0, torn down again when the menu closes.
//
// Layout (240x160):
//   - action icons in a column down the right edge (tiles x 26-29)
//   - the party as mon icons along the bottom (tiles y 16-19)
//   - a Pokedex caught/seen counter in the top right
//   - a label naming whatever the cursor is on, above the party bar

enum
{
    HUD_ICON_POKEDEX,
    HUD_ICON_PARTY,
    HUD_ICON_BAG,
    HUD_ICON_PC,
    HUD_ICON_CARD,
    HUD_ICON_SAVE,
    HUD_ICON_OPTIONS,
    HUD_ICON_POKENAV,
    HUD_ICON_DEXNAV,
    HUD_ICON_EXIT,
    HUD_ICON_GENERIC,
    HUD_ICON_COUNT,
};

#define HUD_ICON_TAG 0x2740

// BG0 in the field uses charBaseIndex 2, which leaves tiles up to 0x3FF before
// OBJ VRAM. 0x200-0x21C is the dialog/std window gfx and DexNav sits at 0x1D5,
// so the HUD takes 0x230 upward (226 tiles).
#define HUD_TILE_BASE           0x230
#define HUD_PALETTE_NUM         STD_WINDOW_PALETTE_NUM

#define COLUMN_LEFT_TILE        26
#define COLUMN_WIDTH_TILES      4
#define COLUMN_HEIGHT_TILES     20
#define COLUMN_TILES            (COLUMN_WIDTH_TILES * COLUMN_HEIGHT_TILES)

#define PARTY_BAR_TOP_TILE      16
#define PARTY_BAR_WIDTH_TILES   26
#define PARTY_BAR_HEIGHT_TILES  4
#define PARTY_BAR_TILES         (PARTY_BAR_WIDTH_TILES * PARTY_BAR_HEIGHT_TILES)

#define COUNTER_WIDTH_TILES     8
#define COUNTER_HEIGHT_TILES    2
#define COUNTER_TILES           (COUNTER_WIDTH_TILES * COUNTER_HEIGHT_TILES)

#define LABEL_WIDTH_TILES       13
#define LABEL_HEIGHT_TILES      2

// Icon column geometry, in screen pixels.
#define ICON_SLOT_HEIGHT        16
#define ICON_CENTER_X           224

// Party bar geometry, in screen pixels.
#define PARTY_SLOT_WIDTH        32
#define PARTY_SLOT_FIRST_X      24
#define PARTY_ICON_Y            144

enum
{
    CURSOR_AREA_COLUMN,
    CURSOR_AREA_PARTY,
};

static const u32 sHudIconsGfx[] = INCGFX_U32("graphics/interface/hud_icons.png", ".4bpp.smol");
static const u16 sHudIconsPal[] = INCBIN_U16("graphics/interface/hud_icons.gbapal");

static const u8 sHudTextColor[3] = { 1, 2, 3 };

static const u8 sText_Slash[] = _("/");

static const struct WindowTemplate sWindowTemplate_Column =
{
    .bg = 0,
    .tilemapLeft = COLUMN_LEFT_TILE,
    .tilemapTop = 0,
    .width = COLUMN_WIDTH_TILES,
    .height = COLUMN_HEIGHT_TILES,
    .paletteNum = HUD_PALETTE_NUM,
    .baseBlock = HUD_TILE_BASE,
};

static const struct WindowTemplate sWindowTemplate_PartyBar =
{
    .bg = 0,
    .tilemapLeft = 0,
    .tilemapTop = PARTY_BAR_TOP_TILE,
    .width = PARTY_BAR_WIDTH_TILES,
    .height = PARTY_BAR_HEIGHT_TILES,
    .paletteNum = HUD_PALETTE_NUM,
    .baseBlock = HUD_TILE_BASE + COLUMN_TILES,
};

static const struct WindowTemplate sWindowTemplate_Counter =
{
    .bg = 0,
    .tilemapLeft = 18,
    .tilemapTop = 0,
    .width = COUNTER_WIDTH_TILES,
    .height = COUNTER_HEIGHT_TILES,
    .paletteNum = HUD_PALETTE_NUM,
    .baseBlock = HUD_TILE_BASE + COLUMN_TILES + PARTY_BAR_TILES,
};

static const struct WindowTemplate sWindowTemplate_Label =
{
    .bg = 0,
    .tilemapLeft = 13,
    .tilemapTop = 14,
    .width = LABEL_WIDTH_TILES,
    .height = LABEL_HEIGHT_TILES,
    .paletteNum = HUD_PALETTE_NUM,
    .baseBlock = HUD_TILE_BASE + COLUMN_TILES + PARTY_BAR_TILES + COUNTER_TILES,
};

static const struct OamData sOamData_HudIcon =
{
    .shape = SPRITE_SHAPE(16x16),
    .size = SPRITE_SIZE(16x16),
    .priority = 0,
};

#define HUD_ICON_ANIM(idx)                              \
    static const union AnimCmd sAnim_HudIcon##idx[] =   \
    {                                                   \
        ANIMCMD_FRAME((idx) * 4, 0),                    \
        ANIMCMD_END,                                    \
    }

HUD_ICON_ANIM(0);
HUD_ICON_ANIM(1);
HUD_ICON_ANIM(2);
HUD_ICON_ANIM(3);
HUD_ICON_ANIM(4);
HUD_ICON_ANIM(5);
HUD_ICON_ANIM(6);
HUD_ICON_ANIM(7);
HUD_ICON_ANIM(8);
HUD_ICON_ANIM(9);
HUD_ICON_ANIM(10);

static const union AnimCmd *const sAnims_HudIcon[] =
{
    sAnim_HudIcon0, sAnim_HudIcon1, sAnim_HudIcon2, sAnim_HudIcon3,
    sAnim_HudIcon4, sAnim_HudIcon5, sAnim_HudIcon6, sAnim_HudIcon7,
    sAnim_HudIcon8, sAnim_HudIcon9, sAnim_HudIcon10,
};

static const struct CompressedSpriteSheet sSpriteSheet_HudIcons =
{
    .data = sHudIconsGfx,
    .size = HUD_ICON_COUNT * 16 * 16 / 2,
    .tag = HUD_ICON_TAG,
};

static const struct SpritePalette sSpritePalette_HudIcons =
{
    .data = sHudIconsPal,
    .tag = HUD_ICON_TAG,
};

static const struct SpriteTemplate sSpriteTemplate_HudIcon =
{
    .tileTag = HUD_ICON_TAG,
    .paletteTag = HUD_ICON_TAG,
    .oam = &sOamData_HudIcon,
    .anims = sAnims_HudIcon,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

// Which icon stands in for each start menu action.
static const u8 sActionIcons[MENU_ACTION_COUNT] =
{
    [MENU_ACTION_POKEDEX]         = HUD_ICON_POKEDEX,
    [MENU_ACTION_POKEMON]         = HUD_ICON_PARTY,
    [MENU_ACTION_BAG]             = HUD_ICON_BAG,
    [MENU_ACTION_POKENAV]         = HUD_ICON_POKENAV,
    [MENU_ACTION_PLAYER]          = HUD_ICON_CARD,
    [MENU_ACTION_SAVE]            = HUD_ICON_SAVE,
    [MENU_ACTION_OPTION]          = HUD_ICON_OPTIONS,
    [MENU_ACTION_EXIT]            = HUD_ICON_EXIT,
    [MENU_ACTION_RETIRE_SAFARI]   = HUD_ICON_EXIT,
    [MENU_ACTION_PLAYER_LINK]     = HUD_ICON_CARD,
    [MENU_ACTION_REST_FRONTIER]   = HUD_ICON_SAVE,
    [MENU_ACTION_RETIRE_FRONTIER] = HUD_ICON_EXIT,
    [MENU_ACTION_PYRAMID_BAG]     = HUD_ICON_BAG,
    [MENU_ACTION_DEBUG]           = HUD_ICON_GENERIC,
    [MENU_ACTION_DEXNAV]          = HUD_ICON_DEXNAV,
    [MENU_ACTION_PC]              = HUD_ICON_PC,
};

EWRAM_DATA static bool8 sHudActive = FALSE;
EWRAM_DATA static u8 sColumnWindowId = 0;
EWRAM_DATA static u8 sPartyBarWindowId = 0;
EWRAM_DATA static u8 sCounterWindowId = 0;
EWRAM_DATA static u8 sLabelWindowId = 0;
EWRAM_DATA static u8 sIconSpriteIds[START_MENU_ACTION_MAX] = {0};
EWRAM_DATA static u8 sPartySpriteIds[PARTY_SIZE] = {0};
EWRAM_DATA static u8 sActions[START_MENU_ACTION_MAX] = {0};
EWRAM_DATA static u8 sNumActions = 0;
EWRAM_DATA static u8 sNumPartyIcons = 0;
EWRAM_DATA static u8 sCursorArea = CURSOR_AREA_COLUMN;
EWRAM_DATA static u8 sColumnPos = 0;
EWRAM_DATA static u8 sPartyPos = 0;

static void DrawColumn(void);
static void DrawPartyBar(void);
static void DrawLabel(void);
static void DrawCounter(void);
static void CreateIconSprites(void);
static void CreatePartyIcons(void);
static u32 ColumnSlotTop(u32 index);

static u32 ColumnSlotTop(u32 index)
{
    u32 top = (DISPLAY_HEIGHT - (sNumActions * ICON_SLOT_HEIGHT)) / 2;

    return top + (index * ICON_SLOT_HEIGHT);
}

void StartMenuHud_Show(const u8 *actions, u32 numActions, u32 cursorPos)
{
    u32 i;

    if (sHudActive)
        StartMenuHud_Hide();

    sNumActions = min(numActions, START_MENU_ACTION_MAX);
    for (i = 0; i < sNumActions; i++)
        sActions[i] = actions[i];

    sCursorArea = CURSOR_AREA_COLUMN;
    sColumnPos = (cursorPos < sNumActions) ? cursorPos : 0;
    sPartyPos = 0;

    sColumnWindowId = AddWindow(&sWindowTemplate_Column);
    sPartyBarWindowId = AddWindow(&sWindowTemplate_PartyBar);
    sCounterWindowId = AddWindow(&sWindowTemplate_Counter);
    sLabelWindowId = AddWindow(&sWindowTemplate_Label);

    PutWindowTilemap(sColumnWindowId);
    PutWindowTilemap(sPartyBarWindowId);
    PutWindowTilemap(sCounterWindowId);
    PutWindowTilemap(sLabelWindowId);

    LoadCompressedSpriteSheetUsingHeap(&sSpriteSheet_HudIcons);
    LoadSpritePalette(&sSpritePalette_HudIcons);

    CreateIconSprites();
    CreatePartyIcons();

    DrawColumn();
    DrawPartyBar();
    DrawCounter();
    DrawLabel();

    sHudActive = TRUE;
}

void StartMenuHud_Hide(void)
{
    u32 i;

    if (!sHudActive)
        return;

    for (i = 0; i < sNumActions; i++)
    {
        if (sIconSpriteIds[i] != MAX_SPRITES)
            DestroySprite(&gSprites[sIconSpriteIds[i]]);
    }

    for (i = 0; i < sNumPartyIcons; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        bool32 isEgg = GetMonData(mon, MON_DATA_IS_EGG);

        FreeMonIconPalette(isEgg ? SPECIES_EGG : GetMonData(mon, MON_DATA_SPECIES));

        if (sPartySpriteIds[i] != MAX_SPRITES)
        {
            // Mon icons point sprite->images straight at ROM; this restores a
            // real SpriteFrameImage before destroying, which DestroySprite needs.
            FreeAndDestroyMonIconSprite(&gSprites[sPartySpriteIds[i]]);
        }
    }

    FreeSpriteTilesByTag(HUD_ICON_TAG);
    FreeSpritePaletteByTag(HUD_ICON_TAG);

    ClearWindowTilemap(sColumnWindowId);
    ClearWindowTilemap(sPartyBarWindowId);
    ClearWindowTilemap(sCounterWindowId);
    ClearWindowTilemap(sLabelWindowId);
    CopyWindowToVram(sColumnWindowId, COPYWIN_MAP);
    CopyWindowToVram(sPartyBarWindowId, COPYWIN_MAP);
    CopyWindowToVram(sCounterWindowId, COPYWIN_MAP);
    CopyWindowToVram(sLabelWindowId, COPYWIN_MAP);

    RemoveWindow(sColumnWindowId);
    RemoveWindow(sPartyBarWindowId);
    RemoveWindow(sCounterWindowId);
    RemoveWindow(sLabelWindowId);
    sColumnWindowId = WINDOW_NONE;
    sPartyBarWindowId = WINDOW_NONE;
    sCounterWindowId = WINDOW_NONE;
    sLabelWindowId = WINDOW_NONE;

    sNumActions = 0;
    sNumPartyIcons = 0;
    sHudActive = FALSE;
}

bool32 StartMenuHud_IsActive(void)
{
    return sHudActive;
}

static void CreateIconSprites(void)
{
    u32 i;

    for (i = 0; i < sNumActions; i++)
    {
        u32 spriteId = CreateSprite(&sSpriteTemplate_HudIcon, ICON_CENTER_X,
                                    ColumnSlotTop(i) + (ICON_SLOT_HEIGHT / 2), 0);

        sIconSpriteIds[i] = spriteId;
        if (spriteId == MAX_SPRITES)
            continue;

        StartSpriteAnim(&gSprites[spriteId], sActionIcons[sActions[i]]);
        gSprites[spriteId].oam.priority = 0;
    }
}

static void CreatePartyIcons(void)
{
    u32 i;

    sNumPartyIcons = 0;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        enum Species species = GetMonData(mon, MON_DATA_SPECIES);
        bool32 isEgg;
        u32 spriteId;

        if (species == SPECIES_NONE)
            break;

        isEgg = GetMonData(mon, MON_DATA_IS_EGG);
        LoadMonIconPalette(isEgg ? SPECIES_EGG : species);
        spriteId = CreateMonIconIsEgg(species, SpriteCB_MonIcon,
                                      PARTY_SLOT_FIRST_X + (i * PARTY_SLOT_WIDTH), PARTY_ICON_Y,
                                      0, GetMonData(mon, MON_DATA_PERSONALITY), isEgg);
        sPartySpriteIds[i] = spriteId;
        sNumPartyIcons++;
        if (spriteId == MAX_SPRITES)
            continue;

        gSprites[spriteId].oam.priority = 0;
    }
}

static void DrawColumn(void)
{
    FillWindowPixelBuffer(sColumnWindowId, PIXEL_FILL(1));

    if (sCursorArea == CURSOR_AREA_COLUMN)
    {
        FillWindowPixelRect(sColumnWindowId, PIXEL_FILL(3), 4, ColumnSlotTop(sColumnPos),
                            COLUMN_WIDTH_TILES * 8 - 8, ICON_SLOT_HEIGHT);
    }

    CopyWindowToVram(sColumnWindowId, COPYWIN_GFX);
}

static void DrawPartyBar(void)
{
    FillWindowPixelBuffer(sPartyBarWindowId, PIXEL_FILL(1));

    if (sCursorArea == CURSOR_AREA_PARTY && sNumPartyIcons != 0)
    {
        FillWindowPixelRect(sPartyBarWindowId, PIXEL_FILL(3),
                            (PARTY_SLOT_FIRST_X - (PARTY_SLOT_WIDTH / 2)) + (sPartyPos * PARTY_SLOT_WIDTH), 0,
                            PARTY_SLOT_WIDTH, PARTY_BAR_HEIGHT_TILES * 8);
    }

    CopyWindowToVram(sPartyBarWindowId, COPYWIN_GFX);
}

static void DrawCounter(void)
{
    u32 width;

    FillWindowPixelBuffer(sCounterWindowId, PIXEL_FILL(1));

    ConvertIntToDecimalStringN(gStringVar1, GetNationalPokedexCount(FLAG_GET_CAUGHT), STR_CONV_MODE_LEFT_ALIGN, 4);
    ConvertIntToDecimalStringN(gStringVar2, GetNationalPokedexCount(FLAG_GET_SEEN), STR_CONV_MODE_LEFT_ALIGN, 4);
    StringCopy(gStringVar4, gStringVar1);
    StringAppend(gStringVar4, sText_Slash);
    StringAppend(gStringVar4, gStringVar2);

    width = GetStringWidth(FONT_SMALL, gStringVar4, 0);
    AddTextPrinterParameterized3(sCounterWindowId, FONT_SMALL, (COUNTER_WIDTH_TILES * 8) - width - 2, 2,
                                 sHudTextColor, TEXT_SKIP_DRAW, gStringVar4);
    CopyWindowToVram(sCounterWindowId, COPYWIN_GFX);
}

static void DrawLabel(void)
{
    const u8 *text;
    u32 width;

    FillWindowPixelBuffer(sLabelWindowId, PIXEL_FILL(1));

    if (sCursorArea == CURSOR_AREA_PARTY)
    {
        if (sNumPartyIcons == 0)
            text = gText_EmptyString2;
        else if (GetMonData(&gParties[B_TRAINER_PLAYER][sPartyPos], MON_DATA_IS_EGG))
            text = gText_EggNickname;
        else
            text = GetSpeciesName(GetMonData(&gParties[B_TRAINER_PLAYER][sPartyPos], MON_DATA_SPECIES));
    }
    else
    {
        text = StartMenu_GetActionName(sActions[sColumnPos]);
    }

    StringExpandPlaceholders(gStringVar4, text);
    width = GetStringWidth(FONT_NORMAL, gStringVar4, 0);
    AddTextPrinterParameterized3(sLabelWindowId, FONT_NORMAL, (LABEL_WIDTH_TILES * 8) - width - 2, 1,
                                 sHudTextColor, TEXT_SKIP_DRAW, gStringVar4);
    CopyWindowToVram(sLabelWindowId, COPYWIN_GFX);
}

bool32 StartMenuHud_HandleDpadInput(void)
{
    if (!sHudActive)
        return FALSE;

    if (sCursorArea == CURSOR_AREA_COLUMN)
    {
        if (JOY_NEW(DPAD_UP))
        {
            sColumnPos = (sColumnPos == 0) ? sNumActions - 1 : sColumnPos - 1;
        }
        else if (JOY_NEW(DPAD_DOWN))
        {
            sColumnPos = (sColumnPos + 1 == sNumActions) ? 0 : sColumnPos + 1;
        }
        else if (JOY_NEW(DPAD_LEFT) && sNumPartyIcons != 0)
        {
            sCursorArea = CURSOR_AREA_PARTY;
            DrawPartyBar();
        }
        else
        {
            return FALSE;
        }
    }
    else
    {
        if (JOY_NEW(DPAD_LEFT))
        {
            sPartyPos = (sPartyPos == 0) ? sNumPartyIcons - 1 : sPartyPos - 1;
        }
        else if (JOY_NEW(DPAD_RIGHT))
        {
            if (sPartyPos + 1 == sNumPartyIcons)
            {
                sCursorArea = CURSOR_AREA_COLUMN;
                DrawPartyBar();
            }
            else
            {
                sPartyPos++;
            }
        }
        else if (JOY_NEW(DPAD_UP) || JOY_NEW(DPAD_DOWN))
        {
            sCursorArea = CURSOR_AREA_COLUMN;
            DrawPartyBar();
        }
        else
        {
            return FALSE;
        }
    }

    PlaySE(SE_SELECT);
    DrawColumn();
    if (sCursorArea == CURSOR_AREA_PARTY)
        DrawPartyBar();
    DrawLabel();

    return TRUE;
}

bool32 StartMenuHud_IsOnParty(void)
{
    return sHudActive && sCursorArea == CURSOR_AREA_PARTY;
}

u32 StartMenuHud_GetPartyPos(void)
{
    return sPartyPos;
}

u32 StartMenuHud_GetCursorPos(void)
{
    return sColumnPos;
}
