/*****************************************************************************
    eqlib.dll: MacroQuest's extension DLL for EverQuest
    Copyright (C) 2002-2003 Plazmic

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2, as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
******************************************************************************/

#if !defined(CINTERFACE)
#error /DCINTERFACE
#endif

#define DBG_SPEW

#include "stdafx.h"
#include "MQ2Main.h"

DWORD ManaGained=0;
DWORD HealthGained=0;


void Pulse()
{
     if (!EQADDR_CHAR) return;
     if (!*EQADDR_CHAR) return;
	 PSPAWNINFO pCharOrMount = NULL;
	PCHARINFO pCharInfo = GetCharInfo();
    PSPAWNINFO pChar = pCharOrMount = *EQADDR_CHAR;
	if (pCharInfo && pCharInfo->pSpawn) pChar=pCharInfo->pSpawn;

	WORD LastZone=-1;
	
    PSPAWNINFO pCharOld = NULL;
    FLOAT LastX = 0.0f;
    FLOAT LastY = 0.0f;
	DWORD LastMoveTick = 0;
    DWORD MapDelay = 0;

	DWORD LastHealth = 0;
	DWORD LastMana = 0;



   // Drop out here if we're waiting for something.
    if ((!pChar) || (gZoning)/* || (gDelayZoning)*/) return;

	if (pChar!=pCharOld && WereWeZoning)
	{
		WereWeZoning = FALSE;
		pCharOld=pChar;
        gFaceAngle = 10000.0f;
        gLookAngle = 10000.0f;
        gbMoving = FALSE;
        LastX = pChar->X;
        LastY = pChar->Y;
		LastMoveTick=GetTickCount();
		EnviroTarget.Name[0]=0;
		DoorEnviroTarget.Name[0]=0;
		LastHealth=0;
		LastMana=0;
		ManaGained=0;
		HealthGained=0;
		PluginsZoned();

    } else if ((LastX!=pChar->X) || (LastY!=pChar->Y) || LastMoveTick>GetTickCount()-100) {
		if ((LastX!=pChar->X) || (LastY!=pChar->Y)) LastMoveTick=GetTickCount();
        gbMoving = TRUE;
        LastX = pChar->X;
        LastY = pChar->Y;
    } else {
        gbMoving = FALSE;
    }
// TODO

	DWORD CurrentHealth=GetCurHPS();
	if (LastHealth && CurrentHealth>LastHealth)
	{
		if (pChar->HPCurrent!=GetMaxHPS())
		{ // gained health, and not max
			HealthGained=CurrentHealth-LastHealth;
		}
	}
	LastHealth=CurrentHealth;

	if (LastMana && pCharInfo->Mana>LastMana)
	{
		if (pCharInfo->Mana!=GetMaxMana())
		{ // gained mana, and not max
			ManaGained=pCharInfo->Mana-LastMana;
		}
	}
	LastMana=pCharInfo->Mana;
/**/

    if (gbDoAutoRun && pChar && pChar->pCharInfo) {
        gbDoAutoRun = FALSE;
        CHAR szServerAndName[MAX_STRING] = {0};
        CHAR szAutoRun[MAX_STRING] = {0};
        PCHAR pAutoRun = szAutoRun;
        sprintf(szServerAndName,"%s.%s",pChar->pCharInfo->Server,pChar->pCharInfo->Name);
        GetPrivateProfileString("AutoRun",szServerAndName,"",szAutoRun,MAX_STRING,gszINIFilename);
        while (pAutoRun[0]==' ' || pAutoRun[0]=='\t') pAutoRun++;
        if (szAutoRun[0]!=0) DoCommand(pChar,pAutoRun);
    }

    if ((gFaceAngle != 10000.0f) || (gLookAngle != 10000.0f)) {
        BOOL NotDone = FALSE;
    if (gFaceAngle != 10000.0f) {
        if (abs((INT)(pCharOrMount->Heading - gFaceAngle)) < 10.0f) {
            pCharOrMount->Heading = (FLOAT)gFaceAngle;
            pCharOrMount->SpeedHeading = 0.0f;
            gFaceAngle = 10000.0f;
        } else {
                NotDone = TRUE;
            DOUBLE c1 = pCharOrMount->Heading + 256.0f;
            DOUBLE c2 = gFaceAngle;
            if (c2<pChar->Heading) c2 += 512.0f;
            DOUBLE turn = (DOUBLE)(rand()%200)/10;
            if (c2<c1) {
                pCharOrMount->Heading += (FLOAT)turn;
                pCharOrMount->SpeedHeading = 12.0f;
                if (pCharOrMount->Heading>=512.0f) pCharOrMount->Heading-=512.0f;
            } else {
                pCharOrMount->Heading -= (FLOAT)turn;
                pCharOrMount->SpeedHeading = -12.0f;
                if (pCharOrMount->Heading<0.0f) pCharOrMount->Heading+=512.0f;
            }
            }
        }

        if (gLookAngle != 10000.0f) {
            if (abs((INT)(pChar->CameraAngle - gLookAngle)) < 5.0f) {
                pChar->CameraAngle = (FLOAT)gLookAngle;
                gLookAngle = 10000.0f;
            } else {
                NotDone = TRUE;
                FLOAT c1 = pChar->CameraAngle;
                FLOAT c2 = (FLOAT)gLookAngle;

                DOUBLE turn = (DOUBLE)(rand()%200)/20;
                if (c1<c2) {
                    pChar->CameraAngle += (FLOAT)turn;
                    if (pChar->CameraAngle>=128.0f) pChar->CameraAngle -= 128.0f;
                } else {
                    pChar->CameraAngle -= (FLOAT)turn;
                    if (pChar->CameraAngle<=-128.0f) pChar->CameraAngle += 128.0f;
                }
            }
        }

        if (NotDone) {
            IsMouseWaiting();
            return;
        }
    }

    if (IsMouseWaiting()) return;

	if (!gDelay && !gMacroPause && (!gMQPauseOnChat || *EQADDR_NOTINCHATMODE) &&
        gMacroBlock && gMacroStack) {
            gMacroStack->Location=gMacroBlock;
            DoCommand(pChar,gMacroBlock->Line);
            if (gMacroBlock) {
                if (!gMacroBlock->pNext) {
                    GracefullyEndBadMacro(pChar,gMacroBlock,"Reached end of macro.");
                } else {
                    gMacroBlock = gMacroBlock->pNext;
                }
            }
    }
	else
	{ /* TEMPORARILY LEAVING THIS IN HERE, IT WILL BE A PLUGIN. */
		HWND EQhWnd=*(HWND*)EQADDR_HWND;
		HMODULE hMod=GetModuleHandle("eqw.dll");
		if (hMod)
		{
			DWORD GetEQWHWND=(DWORD)hMod+0x12C0;
			__asm
			{
				push eax;
				call [GetEQWHWND];
				mov [EQhWnd], eax;
				pop eax;
			};
		}
		
		if (GetForegroundWindow()==EQhWnd)
		{
			Sleep(20);
		}
		else
		{
			Sleep(50);
		}
	}
}

void Heartbeat()
{
    static DWORD LastGetTick = 0;
    DWORD Tick = GetTickCount();
    if (Tick<LastGetTick) LastGetTick=Tick;
    while (Tick>LastGetTick+100) {
        LastGetTick+=100;
        if (gDelay>0) gDelay--;
		DropTimers();
    }
    bRunNextCommand   = TRUE;
    DWORD CurTurbo=0;
	while ((!gKeyStack)   && (bRunNextCommand)) {
        bRunNextCommand   = FALSE;
		Pulse();
        PulsePlugins();
        if (!gTurbo) bRunNextCommand = FALSE;
		if (++CurTurbo>gMaxTurbo) bRunNextCommand =   FALSE;
	}
}

// *************************************************************************** 
// Function:    ProcessGameEvents 
// Description: Our ProcessGameEvents Hook
// *************************************************************************** 
BOOL Trampoline_ProcessGameEvents(VOID); 
BOOL Detour_ProcessGameEvents(VOID) 
{ 
	Heartbeat();
	return Trampoline_ProcessGameEvents();
}

DETOUR_TRAMPOLINE_EMPTY(BOOL Trampoline_ProcessGameEvents(VOID)); 
class CZoningHook {
public:
	VOID EnterZone_Trampoline(PVOID pVoid);
	VOID EnterZone_Detour(PVOID pVoid)
	{
		EnterZone_Trampoline(pVoid);
		gZoning = TRUE;
		WereWeZoning = TRUE;
	}
};

DETOUR_TRAMPOLINE_EMPTY(VOID CZoningHook::EnterZone_Trampoline(PVOID));

void InitializeMQ2Pulse()
{
	DebugSpew("Initializing Pulse");

	BOOL (*pfDetour_ProcessGameEvents)(VOID) = Detour_ProcessGameEvents; 
	BOOL (*pfTrampoline_ProcessGameEvents)(VOID) = Trampoline_ProcessGameEvents; 
	AddDetour((DWORD)ProcessGameEvents,*(PBYTE*)&pfDetour_ProcessGameEvents,*(PBYTE*)&pfTrampoline_ProcessGameEvents);
/**/
   void (CZoningHook::*pfEnterZone_Detour)(PVOID) = CZoningHook::EnterZone_Detour;
   void (CZoningHook::*pfEnterZone_Trampoline)(PVOID) = CZoningHook::EnterZone_Trampoline;
	AddDetour(EQADDR_ZONING,*(PBYTE*)&pfEnterZone_Detour,*(PBYTE*)&pfEnterZone_Trampoline);
}

void ShutdownMQ2Pulse()
{
	RemoveDetour((DWORD)ProcessGameEvents);
	RemoveDetour((DWORD)EQADDR_ZONING);
}