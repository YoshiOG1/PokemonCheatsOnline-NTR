#include "global.h"
#include "helpers.h"
#include "pokeEncounterModifier.h"
#include "itemModifier.h"
#include "otherModifiers.h"
#include "battle/wildModifier.h"
#include "battle/statsModifier.h"



#define     add_search_replace(find, replace)   g_find[g_i] = find; g_replace[g_i] = replace; g_i++
#define     reset_search()						memset(g_find, 0, sizeof(g_find)); memset(g_replace, 0, sizeof(g_replace)); g_i = 0
#define     READU8(addr)                        *(volatile unsigned char*)(addr)
#define     READU16(addr)                       *(volatile unsigned short*)(addr)
#define     READU32(addr)                       *(volatile unsigned int*)(addr)
#define     BUTTON_A                            0x00000001
#define     BUTTON_B                            0x00000002
#define     BUTTON_SE                           0x00000004
#define     BUTTON_ST                           0x00000008
#define     BUTTON_DR                           0x00000010
#define     BUTTON_DL                           0x00000020
#define     BUTTON_DU                           0x00000040
#define     BUTTON_DD                           0x00000080
#define     BUTTON_R                            0x00000100
#define     BUTTON_L                            0x00000200
#define     BUTTON_X                            0x00000400
#define     BUTTON_Y                            0x00000800
#define     IO_BASE_PAD                         1
#define     IO_BASE_LCD                         2
#define     IO_BASE_PDC                         3
#define     IO_BASE_GSPHEAP                     4

enum
{
    FREEZE = 1,
    NOTFREEZE = 0
};

/*
** Globals
*/

Handle                  fsUserHandle;
FS_archive              sdmcArchive;
GAME_PLUGIN_MENU        gamePluginMenu;
u32                     threadStack[0x1000];
u32                     g_find[10];
u32                     g_replace[10];
int                     g_i = 0;
s32                     isNewNtr = 0;
u8                      g_cheatEnabled[64];
u8                      g_state[64];
void                    (*g_functions[64])(void);

/*
**
*/

/*
** Prototypes
*/

u32        plgGetIoBase(u32 IoType);
u32        getCurrentProcessId();

/*
**
*/


void     initMenu()
{
    memset(&gamePluginMenu, 0, sizeof(GAME_PLUGIN_MENU));
    memset(&g_cheatEnabled, 0, sizeof(g_cheatEnabled));
    memset(&g_state, 0, sizeof(g_state));
    memset(&g_functions, 0, sizeof(g_functions));
}

void     addMenuEntry(u8 *str)
{
    u32     pos;
    u32     len;
 
    if (gamePluginMenu.count > 64)
        return;
    pos = gamePluginMenu.count;
    len = strlen(str) + 1;
    gamePluginMenu.offsetInBuffer[pos] = gamePluginMenu.bufOffset;
    strcpy(&(gamePluginMenu.buf[gamePluginMenu.bufOffset]), str);
    gamePluginMenu.count += 1;
    gamePluginMenu.bufOffset += len;
}

void     addCheatMenuEntry(u8 *str, void (*f)(void), u8 state)
{
    u8 buf[100];
 
    g_functions[gamePluginMenu.count] = f;
    g_state[gamePluginMenu.count] = state;
    xsprintf(buf, "[ ] %s", str);
    addMenuEntry(buf);
}

u32     updateMenu()
{
    u32     ret;
    u32     hProcess;
    u32     homeMenuPid;
 
    PLGLOADER_INFO *plgLoaderInfo = (void*)0x07000000;
    plgLoaderInfo->gamePluginPid = getCurrentProcessId();
    plgLoaderInfo->gamePluginMenuAddr = (u32)&gamePluginMenu;
    ret = 0;
    homeMenuPid = plgGetIoBase(5);
    if (homeMenuPid == 0)
        return(1);
    ret = svc_openProcess(&hProcess, homeMenuPid);
    if (ret != 0)
        return (ret);
    copyRemoteMemory(hProcess, &(plgLoaderInfo->gamePluginPid), CURRENT_PROCESS_HANDLE, &(plgLoaderInfo->gamePluginPid), 8);
final:
    svc_closeHandle(hProcess);
    return (ret);
}

int     scanMenu()
{
    u32 i;
 
    for (i = 0; i < gamePluginMenu.count; i++)
    {
        if (gamePluginMenu.state[i])
        {
            gamePluginMenu.state[i] = 0;
            return (i);
        }
    }
    return (-1);
}

int     detectLanguage()
{
    return(0);
}

void     updateCheatEnableDisplay(u32 id)
{
    if(gamePluginMenu.buf[gamePluginMenu.offsetInBuffer[id]] == '[' && gamePluginMenu.buf[gamePluginMenu.offsetInBuffer[id] + 2] == ']')
        gamePluginMenu.buf[gamePluginMenu.offsetInBuffer[id] + 1] = g_cheatEnabled[id] ? 'X' : ' ';
}

void     disableCheat(int index)
{
    g_cheatEnabled[index] = !g_cheatEnabled[index];
    updateCheatEnableDisplay(index);
}

void     onCheatItemChanged(int id, int enable)
{
    if(enable)
    {
        g_functions[id]();
        if(!g_state[id])
            disableCheat(id);
    }
}

void     scanCheatMenu()
{
    int ret = scanMenu();
 
    if (ret != -1)
    {
        g_cheatEnabled[ret] = !g_cheatEnabled[ret];
        updateCheatEnableDisplay(ret);
        onCheatItemChanged(ret, g_cheatEnabled[ret]);
    }
}

void    find_and_replace_multiple(void *start_addr, u32 length)
{
    u32 find_value;
    u32 replace_value;
    int i;

    i = 0;
    while (length-- > 0)
    {
        for (i = 0; i < g_i; i++)
        {
            find_value = g_find[i];
            replace_value = g_replace[i];
            if (*(u16 *)start_addr == find_value)
            {
                *(u16 *)start_addr = replace_value;
                break;
            }
        }
        start_addr += 4;
    }
}

/*
** Cheats functions
*/

u32        add_to_address(void *address, u32 value_to_add)
{
    *(u32 *)address += value_to_add;
}

u32        sub_to_address(void *address, u32 value_to_sub)
{
    *(u32 *)address -= value_to_sub;
}

void cloneSlotOneToSlots(int numberOfSlots) {
    int i;
    int theBase = 0x08C9E134;
    int theLength = 0xE8;
    for(i = 0; i < numberOfSlots; i++) {
        memcpy((u8*) (i * 0xE8 + 0x08C9E134), (u8*) (0x08C9E134), theLength);
    }
}

void cloneFillBoxOne() {  // Fill entire box with cloned pokemon
    cloneSlotOneToSlots(30);
}

void cloneThruSlotTwelve() {  // Clone from slot 1 to slots 2 through 12
    cloneSlotOneToSlots(12);
}

void cloneSinglePkm() {  // Clone from slot one to slot 2 only (just one copy)
    cloneSlotOneToSlots(2);
}

/*
**
*/

void    initCheatMenu()
{
    initMenu();
    addMenuEntry("=== 1-time run codes ===");
    addMenuEntry("-- Cloning codes --");
    addCheatMenuEntry("Clone Box1Slot1 > entire Box", cloneFillBoxOne, NOTFREEZE);
    addCheatMenuEntry("Clone Slot1 to slots 2-12", cloneThruSlotTwelve, NOTFREEZE);
    addCheatMenuEntry("Clone Slot1 to slot 2", cloneSinglePkm, NOTFREEZE);
    addMenuEntry("=== Toggle codes ===");
    addMenuEntry("-- Encounter modifier codes --");
    addMenuEntry(" **COMING SOON?");
    // addCheatMenuEntry("Force Shiny", shinifyCheat, FREEZE);
    updateMenu();
}

void gamePluginEntry()
{
    u32 i;
 
    INIT_SHARED_FUNC(plgGetIoBase, 8);
    INIT_SHARED_FUNC(copyRemoteMemory, 9);
    svc_sleepThread(5000000000);
    if (((NS_CONFIG*)(NS_CONFIGURE_ADDR))->sharedFunc[8])
        isNewNtr = 1;
    else
        isNewNtr = 0;
    if (isNewNtr)
        IoBasePad = plgGetIoBase(IO_BASE_PAD);
    initCheatMenu();
    while (1)
    {
        svc_sleepThread(10000);
        scanCheatMenu();
        for(i = 0; i < 64; i++)
            (g_cheatEnabled[i]) ? g_functions[i]() : 0 ;
    }
}
