#include "global.h"
#include "helpers.h"
#include "pokeEncounterModifier.h"
#include "itemModifier.h"
#include "otherModifiers.h"
#include "string.h"
#include "3dstypes.h"
#include "ctr/types.h"
#include "func.h"

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

/*
***
**  BEGIN NECESSARY FUNCTIONS
***
*/

typedef struct PK6
{

	u32 encryptionKey;
	u16 sanityPlaceholder;
	u16 checksum;
	
	u16 pokedexID;
	u16 heldItem;
	u16 TID;
	u16 SID;
	u32 experience;
	u8 ability;
	u8 abilityNumber;
	u16 hitRemainingTainingBag;
	u32 PID;
	u8 nature;
	u8 form;
	u8 EVs[6];
	u8 contest[6];
	u8 markings;
	u8 pokerus;
	u32 goldMedalFlags;
	u8 ribbons[6];
	u16 unused_1;
	u8 contestRibbons;
	u8 battleRibbons;
	u8 superTrainingFlags;
	u8 unused_2[5];
	
	u8 nickname[26];
	u16 moves[4];
	u8 currentPP[4];
	u8 movePPUps[4];
	u16 relearnMoves[4];
	u8 superTrainingFlag;
	u8 unused_3;
	u32 IVs;
	
	u8 latestHandlerName[26];
	u8 handlerGender;
	u8 currentHandler;
	u16 geolocation[5];
	u32 unused_4;
	u8 handlerFriendship;
	u8 handlerAffection;
	u8 handlerMemoryIntensity;
	u8 handlerMemoryLine;
	u8 handlerMemoryFeeling;
	u8 unused_5;
	u16 handlerMemoryTextVar;
	u32 unused_6;
	u8 fullness;
	u8 enjoyment;
	
	u8 trainerName[26];
	u8 trainerFriendship;
	u8 trainerAffection;
	u8 trainerMemoryIntensity;
	u8 trainerMemoryLine;
	u16 trainerMemoryTextVar;
	u8 trainerMemoryFeeling;
	u8 dateEggReceived[3];
	u8 dateMet[3];
	u8 unused_7;
	u16 eggLocation;
	u16 metLocation;
	u8 ball;
	u8 encounterLevel;
	u8 encounterType;
	u8 trinerGameID;
	u8 countryID;
	u8 regionID;
	u8 consoleRegionID;
	u8 languageID;
	u32 unused_8;

} PK6;

typedef enum
{
	HP,
	ATTACK,
	DEFENSE,
	SPEED,
	SP_ATTACK,
	SP_DEFENSE
}Stats;

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

// POKEMON DECRYPTION STUFF
// 

u8 *PK6_LOCATION_1 = (u8*)0x081FEEC8;
u8 *PK6_LOCATION_2 = (u8*)0x081FFA6C;
u16 *TID = (u16*)0x08C81340;
u16 *SID = (u16*)0x08C81342;

u8 aloc[] = {0, 0, 0, 0, 0, 0, 1, 1, 2, 3, 2, 3, 1, 1, 2, 3, 2, 3, 1, 1, 2, 3, 2, 3};
u8 bloc[] = {1, 1, 2, 3, 2, 3, 0, 0, 0, 0, 0, 0, 2, 3, 1, 1, 3, 2, 2, 3, 1, 1, 3, 2};
u8 cloc[] = {2, 3, 1, 1, 3, 2, 2, 3, 1, 1, 3, 2, 0, 0, 0, 0, 0, 0, 3, 2, 3, 2, 1, 1};
u8 dloc[] = {3, 2, 3, 2, 1, 1, 3, 2, 3, 2, 1, 1, 3, 2, 3, 2, 1, 1, 0, 0, 0, 0, 0, 0};

u32 seedStep(u32 seed)
{
  u32 a = 0x41C64E6D;
  u32 c = 0x00006073;

  return ((seed * a + c) & 0xFFFFFFFF);
}

u16 checksum(PK6 *data, u32 len)
{
    u16 chk = 0;
	u16* data_16 = (u16*)data;
	
	u32 i;
    for (i = 4; i < len/2; i ++)
        chk += data_16[i];
		
    return chk;
}

int shuffleArray(u8 *array, u8 sv)
{
  u8 ord[] = {aloc[sv], bloc[sv], cloc[sv], dloc[sv]};
  u8 pkmcpy[232];
  u8 tmp[56];

  memcpy(pkmcpy, array, 232);
  
  int i = 0;
  for (i = 0; i < 4; i++)
  {
    memcpy(tmp, pkmcpy + 8 + 56 * ord[i], 56);
    memcpy(array + 8 + 56 * i, tmp, 56);
  }
  
  return 0;	
}
// 



u16 decryptPokemon(u8 *enc, PK6 *res)
{
  u8* dec = (u8*)res;
  memcpy(dec, enc, 232);

  u32 pv = res->encryptionKey;
  u8 sv = (((pv & 0x3E000) >> 0xD) % 24);

  u32 seed = pv;
  u16 tmp;
  
  int i = 0;
  for (i = 8; i < 232; i += 2)
  {
    memcpy(&tmp, dec + i, 2);
    tmp ^= (seedStep(seed) >> 16);
    seed = seedStep(seed);
    memcpy(dec + i, &tmp, 2);
  }
  shuffleArray((u8*)dec, sv);
  
  return checksum(res, 232) == ((u16*)enc)[3];
}

int encryptPokemon(PK6 *dec, u8 *enc)
{
  u32 pv = dec->encryptionKey;
  u32 sv = (((pv & 0x3E000) >> 0xD) % 24);
  u16 tmp;

  int i = 0;
  memcpy(enc, dec, 232);
  for(i = 0; i < 11; i++)
    shuffleArray((u8*)enc, sv);

  u32 seed = pv;
  for(i = 8; i < 232; i += 2)
  {
    memcpy(&tmp, enc + i, 2);
    tmp ^= (seedStep(seed) >> 16);
    seed = seedStep(seed);
    memcpy(enc + i, &tmp, 2);
  }
  
  return 0;
}

int decryptBattleStats(u32 key, PK6 *enc, PK6 *dec)
{
    memcpy(dec, enc, 0x1C);
    u32 seed = key;
    u16 tmp;
	
	int i;
    for (i = 0; i < 0x1C; i += 2)
    {
        memcpy(&tmp, dec + i, 2);
        tmp ^= (seedStep(seed) >> 16);
        seed = seedStep(seed);
        memcpy(dec + i, &tmp, 2);
    }
	
    return 0;
}

// STUFF FOR MODIFYING POKEMON

void updateChecksum(PK6* in)
{
	in->checksum = checksum(in, 232);
}

u16 getCurrentPokemon(PK6* out)
{
	return decryptPokemon(PK6_LOCATION_1, out);
}

Result setCurrentPokemon(PK6* in)
{
	updateChecksum(in);
	encryptPokemon(in, PK6_LOCATION_1);
}

Result setCurrentPokemonWild(PK6* in)
{
    updateChecksum(in);
	encryptPokemon(in, PK6_LOCATION_1);
    encryptPokemon(in, PK6_LOCATION_2);
}

void makeShiny(PK6* pk6)
{
	u16 s_xor = ((*TID) ^ (*SID) ^ (u16)(pk6->PID & 0x0000FFFF)) & 0xFFF0;
	pk6->PID = (pk6->PID & 0x000FFFFF) | (s_xor << 16);
}

void makeShinyPC(PK6* pk6)
{
	u16 s_xor = (pk6->TID ^ pk6->SID ^ (u16)(pk6->PID & 0x0000FFFF)) & 0xFFF0;
	pk6->PID = (pk6->PID & 0x000FFFFF) | (s_xor << 16);
}

void setIV(PK6* pk6, Stats s, u8 amount)
{
	if(amount > 31) return;
	pk6->IVs = (pk6->IVs & ~(0xFF << (5 * s))) | (amount << (5 * s));
}

u8 isShiny(PK6* pk6)
{
	return (*TID ^ *SID ^ (u16)(pk6->PID & 0x0000FFFF) ^ (u16)((pk6->PID & 0xFFFF0000) >> 16)) < 16;
}

u16 getPSV(PK6* pk6) {
    u16 pidLo = (u16)(pk6->PID & 0x0000FFFF);
    u16 pidHi = (u16)((pk6->PID & 0xFFFF0000) >> 16);
    return (pidLo ^ pidHi) >> 4;  // Am I doing this right?
}

u16 getTSV(PK6* pk6) {
    u16 theTID = pk6->TID;
    u16 theSID = pk6->SID;
    return (theTID ^ theSID) >> 4;  // Am I doing this right?
}

/*
***
**  END NECESSARY FUNCTIONS
***
*/

// */
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

void cloneBoxOneToBoxTwo() {  // Copy all Pokemon from Box 1 into Box 2 (forward)
    memcpy((u8*) (0x1B30 + 0x08C9E134), (u8*) (0x08C9E134), 0x1B30);
}

void cloneBoxTwoToBoxOne() {  // Copy all Pokemon from Box 2 back into Box 1 (backward)
    memcpy((u8*) (0x08C9E134), (u8*) (0x1B30 + 0x08C9E134), 0x1B30);
}

void shinyB1S1() {
    PK6 pkm;
    PK6_LOCATION_1 = (u8*)0x08C9E134;
    if(getCurrentPokemon(&pkm) && !isShiny(&pkm))
    {
        makeShinyPC(&pkm);
        updateChecksum(&pkm);
        setCurrentPokemon(&pkm);
    }
    PK6_LOCATION_1 = (u8*)0x081FEEC8;
}

void shinifyCheat() {  // credit to hartmannaf and Nba_Yoh for this cheat?
    PK6 pkm;
    if(getCurrentPokemon(&pkm) && !isShiny(&pkm)) {
        makeShiny(&pkm);
        setCurrentPokemonWild(&pkm);
    }
}

void EncConstToNickB1S1() {  // test that changes the nickname of slot 1 pkm to the encryption constant (maybe)
    PK6 pkm;
    int i;
    PK6_LOCATION_1 = (u8*)0x08C9E134;
    if(getCurrentPokemon(&pkm)) {
        char charbytes[12];
        xsprintf(charbytes, "%X", READU32(0x08C9E134));
        charbytes[8] = 0x00;
        charbytes[9] = 0x00;
        charbytes[10] = 0x00;
        charbytes[11] = 0x00;
        for (i = 0; i < 12; i++) {
            (&pkm)->nickname[i * 2] = charbytes[i];
            (&pkm)->nickname[(i * 2) + 1] = 0x00;
        }
        setCurrentPokemon(&pkm);
    }
    PK6_LOCATION_1 = (u8*)0x081FEEC8;
}

void PSVtoNick() {  // Changes nickname of Box1Slot1 Pkm to its Shiny Value (useful for eggs)
    PK6 pkm;
    int i;
    PK6_LOCATION_1 = (u8*)0x08C9E134;
    if(getCurrentPokemon(&pkm)) {
        char charbytes[12];
        u16 thePSV = getPSV(&pkm);
        xsprintf(charbytes, "E%04d", thePSV);
        charbytes[5] = 0x00;
        charbytes[6] = 0x00;
        charbytes[7] = 0x00;
        charbytes[8] = 0x00;
        charbytes[9] = 0x00;
        charbytes[10] = 0x00;
        charbytes[11] = 0x00;
        for (i = 0; i < 12; i++) {
            (&pkm)->nickname[i * 2] = charbytes[i];
            (&pkm)->nickname[(i * 2) + 1] = 0x00;
        }
        setCurrentPokemon(&pkm);
    }
    PK6_LOCATION_1 = (u8*)0x081FEEC8;
}

void SIDtoNick() {  // Changes nickname of Box1Slot1 Pkm to its OT's Secret ID (only use on an unwanted pokemon)
    PK6 pkm;
    int i;
    PK6_LOCATION_1 = (u8*)0x08C9E134;
    if(getCurrentPokemon(&pkm)) {
        char charbytes[12];
        u16 theSID = (&pkm)->SID;
        xsprintf(charbytes, "SID=%05d", theSID);
        charbytes[9] = 0x00;
        charbytes[10] = 0x00;
        charbytes[11] = 0x00;
        for (i = 0; i < 12; i++) {
            (&pkm)->nickname[i * 2] = charbytes[i];
            (&pkm)->nickname[(i * 2) + 1] = 0x00;
        }
        setCurrentPokemon(&pkm);
    }
    PK6_LOCATION_1 = (u8*)0x081FEEC8;
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
    addCheatMenuEntry("Copy all Box1 mons to Box2 (>>)", cloneBoxOneToBoxTwo, NOTFREEZE);
    addCheatMenuEntry("Copy all Box2 mons to Box1 (<<)", cloneBoxTwoToBoxOne, NOTFREEZE);
    
    addMenuEntry("-- Slot 1 PC Modifiers --");
    addCheatMenuEntry("Make Pokemon in Box1Slot1 SHINY", shinyB1S1, NOTFREEZE);
    
    // Debug only
    addMenuEntry("-- DEBUG (risky!) --");
    addCheatMenuEntry("Egg SV > Nickname (B1S1)", PSVtoNick, NOTFREEZE);
    addCheatMenuEntry("OT SecretID > Nickname (B1S1)", SIDtoNick, NOTFREEZE);
    addCheatMenuEntry("EncryptionKey > Nickname (B1S1)", EncConstToNickB1S1, NOTFREEZE);
    // */
    
    addMenuEntry("=== Toggle codes ===");
    addMenuEntry("-- Encounter modifier codes --");
    addCheatMenuEntry("Force Shiny", shinifyCheat, FREEZE); //  */
    addMenuEntry(" **Randomizer coming soon?");
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
