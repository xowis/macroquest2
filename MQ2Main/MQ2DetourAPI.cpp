/*****************************************************************************
    MQ2Main.dll: MacroQuest2's extension DLL for EverQuest
    Copyright (C) 2002-2003 Plazmic, 2003-2004 Lax

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2, as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
******************************************************************************/

// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x510
#define DIRECTINPUT_VERSION 0x800

#if !defined(CINTERFACE)
#error /DCINTERFACE
#endif

#define DBG_SPEW

#include "MQ2Main.h"

typedef struct _OurDetours {
    unsigned int addr;
    unsigned int count;
    unsigned char array[50];
	PBYTE pfDetour;
	PBYTE pfTrampoline;

	struct _OurDetours *pNext;
	struct _OurDetours *pLast;
} OurDetours;

OurDetours *ourdetours=0;
CRITICAL_SECTION gDetourCS;

OurDetours *FindDetour(DWORD address)
{
	OurDetours *pDetour=ourdetours;
	while(pDetour)
	{
		if (pDetour->addr=address)
			return pDetour;
		pDetour=pDetour->pNext;
	}
	return 0;
}

BOOL AddDetour(DWORD address, PBYTE pfDetour, PBYTE pfTrampoline, DWORD Count)
{
	CAutoLock Lock(&gDetourCS);
	BOOL Ret=TRUE;
	DebugSpew("AddDetour(0x%X,0x%X,0x%X)",address,pfDetour,pfTrampoline);
	if (FindDetour(address))
	{

		DebugSpew("Already detoured.");
		return FALSE;
	}
	OurDetours *detour = new OurDetours;
	detour->addr=address;
	detour->count=Count;
	memcpy(detour->array,(char *)address, Count);
	detour->pNext=ourdetours;
	if (ourdetours)
		ourdetours->pLast=detour;
	detour->pLast=0;
    if (pfDetour && !DetourFunctionWithEmptyTrampoline(pfTrampoline, 
         (PBYTE)address, 
         pfDetour))
	{
		detour->pfDetour=0;
		detour->pfTrampoline=0;
		Ret=FALSE;
		DebugSpew("Detour failed.");
	}
	else
	{
		detour->pfDetour=pfDetour;
		detour->pfTrampoline=pfTrampoline;
		DebugSpew("Detour success.");
	}
	ourdetours=detour;
	return Ret;
}

void RemoveDetour(DWORD address)
{
	CAutoLock Lock(&gDetourCS);
	DebugSpew("RemoveDetour(%X)",address);
	OurDetours *detour = ourdetours;
	while (detour)
	{
		if (detour->addr==address)
		{
			if (detour->pfDetour)
			{
		      DetourRemove(detour->pfTrampoline, 
				detour->pfDetour); 		
			}
			  if (detour->pLast)
				detour->pLast->pNext=detour->pNext;
			  else
				  ourdetours=detour->pNext;

			  if (detour->pNext)
				  detour->pNext->pLast=detour->pLast;
			delete detour;
			  DebugSpew("Detour removed.");
			  return;
		}
		detour=detour->pNext;
	}
	DebugSpew("Detour not found in RemoveDetour()");
}

void RemoveOurDetours()
{
	CAutoLock Lock(&gDetourCS);
	DebugSpew("RemoveOurDetours()");
	if (!ourdetours)
		return;
	while (ourdetours)
	{
		if (ourdetours->pfDetour)
		{
			DebugSpew("RemoveOurDetours() -- Removing %X",ourdetours->addr);
			DetourRemove(ourdetours->pfTrampoline,ourdetours->pfDetour); 				
		}
//		else
		{
			OurDetours *pNext=ourdetours->pNext;
			delete ourdetours;
			ourdetours=pNext;
		}
	}
}


// this is the memory checker key struct
struct mckey {
    union {
        int x;
        unsigned char a[4];
        char sa[4];
    };
};

// pointer to encryption pad for memory checker
unsigned int *extern_array0 = NULL;
unsigned int *extern_array1 = NULL;
unsigned int *extern_array2 = NULL;
unsigned int *extern_array3 = NULL;
int __cdecl memcheck0(unsigned char *buffer, int count, struct mckey key);
int __cdecl memcheck1(unsigned char *buffer, int count, struct mckey key);
int __cdecl memcheck2(unsigned char *buffer, int count, struct mckey key);
int __cdecl memcheck3(unsigned char *buffer, int count, struct mckey key);


DETOUR_TRAMPOLINE_EMPTY(VOID memcheck4_tramp(PVOID,DWORD,PCHAR,DWORD,BOOL)); 

VOID memcheck4(PVOID A,DWORD B,PCHAR C,DWORD D,BOOL E)
{
	if (B==0x00F4)
	{
		int Pos = 4 + strlen(&C[4])+ 1;
		int End = Pos + (int)(71.0*rand()/(RAND_MAX+1.0));
		for (Pos ; Pos < End ; Pos++)
			C[Pos]=0;

		int t;
		for (Pos ; Pos < 1024 ; Pos++)
		{
			t = (int)(397.0*rand()/(RAND_MAX+1.0));
			C[Pos]=(t <= 255) ? (char)t : 0;
		}
	}
	memcheck4_tramp(A,B,C,D,E);
}

// ***************************************************************************
// Function:    HookMemChecker
// Description: Hook MemChecker
// ***************************************************************************
int (__cdecl *memcheck0_tramp)(unsigned char *buffer, int count, struct mckey key);
int (__cdecl *memcheck1_tramp)(unsigned char *buffer, int count, struct mckey key);
int (__cdecl *memcheck2_tramp)(unsigned char *buffer, int count, struct mckey key);
int (__cdecl *memcheck3_tramp)(unsigned char *buffer, int count, struct mckey key);

VOID HookMemChecker(BOOL Patch)
{
    // hit the debugger if we don't hook this
    // take no chances
    if ((!EQADDR_MEMCHECK0) ||
        (!EQADDR_MEMCHECK1) ||
        (!EQADDR_MEMCHECK2) ||
        (!EQADDR_MEMCHECK3)) {
        _asm int 3
    }
    DebugSpew("HookMemChecker - %satching",(Patch)?"P":"Unp");
    if (Patch) {

		AddDetour((DWORD)EQADDR_MEMCHECK0);

        (*(PBYTE*)&memcheck0_tramp) = DetourFunction( (PBYTE) EQADDR_MEMCHECK0,
                                                    (PBYTE) memcheck0);

		AddDetour((DWORD)EQADDR_MEMCHECK1);

        (*(PBYTE*)&memcheck1_tramp) = DetourFunction( (PBYTE) EQADDR_MEMCHECK1,
                                                    (PBYTE) memcheck1);

		AddDetour((DWORD)EQADDR_MEMCHECK2);

        (*(PBYTE*)&memcheck2_tramp) = DetourFunction( (PBYTE) EQADDR_MEMCHECK2,
                                                    (PBYTE) memcheck2);

		AddDetour((DWORD)EQADDR_MEMCHECK3);

        (*(PBYTE*)&memcheck3_tramp) = DetourFunction( (PBYTE) EQADDR_MEMCHECK3,
                                                    (PBYTE) memcheck3);

		EasyDetour((DWORD)send_message,memcheck4,VOID,(PVOID,DWORD,PCHAR,DWORD,BOOL),memcheck4_tramp);
    } else {
        DetourRemove((PBYTE) memcheck0_tramp,
                     (PBYTE) memcheck0);
        memcheck0_tramp = NULL;
		RemoveDetour(EQADDR_MEMCHECK0);

        DetourRemove((PBYTE) memcheck1_tramp,
                     (PBYTE) memcheck1);
        memcheck1_tramp = NULL;
		RemoveDetour(EQADDR_MEMCHECK1);

        DetourRemove((PBYTE) memcheck2_tramp,
                     (PBYTE) memcheck2);
        memcheck2_tramp = NULL;
		RemoveDetour(EQADDR_MEMCHECK2);

		DetourRemove((PBYTE) memcheck3_tramp,
                     (PBYTE) memcheck3);
        memcheck3_tramp = NULL;
		RemoveDetour(EQADDR_MEMCHECK3);
		RemoveDetour((DWORD)send_message);
    }
}


//  004F7D24: 55                 push        ebp
//  004F7D25: 8B EC              mov         ebp,esp
//  004F7D27: 8A 45 10           mov         al,byte ptr [ebp+10h]
//  004F7D2A: 56                 push        esi


int __cdecl memcheck0(unsigned char *buffer, int count, struct mckey key)
{
    unsigned int x, i;
    unsigned int ecx;
    unsigned int eax = ~key.a[0] & 0xff;

    if (!extern_array0) {
        if (!EQADDR_ENCRYPTPAD0) {
            //_asm int 3
        } else {
          extern_array0 = (unsigned int *)EQADDR_ENCRYPTPAD0;
        }
    }

//  004F7D2B: F6 D0              not         al
//  004F7D2D: 0F B6 C0           movzx       eax,al
    unsigned int edx = key.a[1] & 0xff;
//  004F7D30: 0F B6 55 11        movzx       edx,byte ptr [ebp+11h]
//  004F7D34: 8B 04 85 6C 7B 5C  mov         eax,dword ptr [eax*4+005C7B6Ch]
//            00

    eax = extern_array0[eax];

//  004F7D3B: BE FF FF FF 00     mov         esi,0FFFFFFh
//  004F7D40: 33 C6              xor         eax,esi
    eax ^= 0xffffff;

//  004F7D42: 57                 push        edi
//  004F7D43: 8B C8              mov         ecx,eax
//  004F7D45: BF FF 00 00 00     mov         edi,0FFh

//  004F7D4A: 23 CF              and         ecx,edi
    ecx = eax & 0xff;
//  004F7D4C: 33 CA              xor         ecx,edx
    ecx ^= edx;
//  004F7D4E: 0F B6 55 12        movzx       edx,byte ptr [ebp+12h]
    edx = key.a[2] & 0xff;

//  004F7D52: 8B 0C 8D 6C 7B 5C  mov         ecx,dword ptr [ecx*4+005C7B6Ch]
//            00
    ecx = extern_array0[ecx];

//  004F7D59: C1 F8 08           sar         eax,8
//  004F7D5C: 23 C6              and         eax,esi
    eax = ((int)eax>>8) & 0xffffff;

//  004F7D5E: 33 C8              xor         ecx,eax
    ecx ^= eax;

//  004F7D60: 8B C1              mov         eax,ecx
//  004F7D62: 23 C7              and         eax,edi
//  004F7D64: 33 C2              xor         eax,edx
    eax = (ecx & 0xff) ^ edx;

//  004F7D66: C1 F9 08           sar         ecx,8
    ecx = (int)ecx >> 8;

//  004F7D69: 8B 14 85 6C 7B 5C  mov         edx,dword ptr [eax*4+005C7B6Ch]
//            00
    edx = extern_array0[eax];

//  004F7D70: 23 CE              and         ecx,esi
//  004F7D72: 33 D1              xor         edx,ecx
    edx ^= ecx & 0xffffff;

//  004F7D74: 0F B6 4D 13        movzx       ecx,byte ptr [ebp+13h]
    ecx = key.a[3] & 0xff;

//  004F7D78: 8B C2              mov         eax,edx
//  004F7D7A: 23 C7              and         eax,edi
//  004F7D7C: 33 C1              xor         eax,ecx
//  004F7D7E: 8B 4D 08           mov         ecx,dword ptr [ebp+8]
//  004F7D81: C1 FA 08           sar         edx,8
//  004F7D84: 8B 04 85 6C 7B 5C  mov         eax,dword ptr [eax*4+005C7B6Ch]
//            00

    eax = extern_array0[(edx & 0xff) ^ ecx];

//  004F7D8B: 23 D6              and         edx,esi
//  004F7D8D: 33 C2              xor         eax,edx
    eax ^= ((int)edx>>8) & 0xffffff;

//  004F7D8F: 8B 55 0C           mov         edx,dword ptr [ebp+0Ch]
//  004F7D92: 03 D1              add         edx,ecx
//  004F7D94: 89 4D 10           mov         dword ptr [ebp+10h],ecx
//  004F7D97: 3B CA              cmp         ecx,edx
//  004F7D99: 73 24              jae         004F7DBF
    if (count == 0) return ~eax;

//  004F7D9B: 53                 push        ebx
//  004F7D9C: 8B 5D 10           mov         ebx,dword ptr [ebp+10h]
//  004F7D9F: 8B C8              mov         ecx,eax
//  004F7DA1: 23 CF              and         ecx,edi
//  004F7DA3: 0F B6 1B           movzx       ebx,byte ptr [ebx]
//  004F7DA6: 33 CB              xor         ecx,ebx
//  004F7DA8: C1 F8 08           sar         eax,8
//  004F7DAB: 8B 0C 8D 6C 7B 5C  mov         ecx,dword ptr [ecx*4+005C7B6Ch]
//            00
//  004F7DB2: 23 C6              and         eax,esi
//  004F7DB4: 33 C1              xor         eax,ecx
//  004F7DB6: FF 45 10           inc         dword ptr [ebp+10h]
//  004F7DB9: 39 55 10           cmp         dword ptr [ebp+10h],edx
//  004F7DBC: 72 DE              jb          004F7D9C

//  004F7DBE: 5B                 pop         ebx
//  004F7DBF: 5F                 pop         edi
//  004F7DC0: 5E                 pop         esi
//  004F7DC1: F7 D0              not         eax
//  004F7DC3: 5D                 pop         ebp
//  004F7DC4: C3                 ret

    for (i=0;i<(unsigned int)count;i++) {
        unsigned char tmp;
		OurDetours *detour = ourdetours;
        unsigned int b=(int) &buffer[i];
		while(detour)
		{
			if (detour->count && (b >= detour->addr) &&
                 (b < detour->addr+detour->count) ) {
                 tmp = detour->array[b - detour->addr];
                 break;
            }
			detour=detour->pNext;
		}
        if (!detour) tmp = buffer[i];
        x = (int)tmp ^ (eax & 0xff);
        eax = ((int)eax >> 8) & 0xffffff;
        x = extern_array0[x];
        eax ^= x;
    }
    return ~eax;
}

int __cdecl memcheck1(unsigned char *buffer, int count, struct mckey key);
int __cdecl memcheck1(unsigned char *buffer, int count, struct mckey key) 
{
    unsigned int i;
    unsigned int ebx, eax, edx;

    if (!extern_array1) {
        if (!EQADDR_ENCRYPTPAD1) {
            //_asm int 3
        } else {
          extern_array1 = (unsigned int *)EQADDR_ENCRYPTPAD1;
        }
    }
//                push    ebp
//                mov     ebp, esp
//                push    esi
//                push    edi
//                or      edi, 0FFFFFFFFh
//                cmp     [ebp+arg_8], 0
    if (key.x != 0) {
//                mov     esi, 0FFh
//                mov     ecx, 0FFFFFFh
//                jz      short loc_4C3978
//                xor     eax, eax
//                mov     al, byte ptr [ebp+arg_8]
//                xor     edx, edx
//                mov     dl, byte ptr [ebp+arg_8+1]
    edx = key.a[1];
//                not     eax
//                and     eax, esi
    eax = ~key.a[0] & 0xff;
//                mov     eax, encryptpad1[eax*4]
    eax = extern_array1[eax];
//                xor     eax, ecx
    eax ^= 0xffffff;
//                xor     edx, eax
//                and     edx, esi
    edx = (edx ^ eax) & 0xff;
//                sar     eax, 8
//                and     eax, ecx
    eax = ((int)eax >> 8) & 0xffffff;
//                xor     eax, encryptpad1[edx*4]
    eax ^= extern_array1[edx];
//                xor     edx, edx
//                mov     dl, byte ptr [ebp+arg_8+2]
    edx = key.a[2];
//                xor     edx, eax
//                sar     eax, 8
//                and     edx, esi
    edx = (edx ^ eax) & 0xff;
//                and     eax, ecx
    eax = ((int)eax >> 8) & 0xffffff;
//                xor     eax, encryptpad1[edx*4]
    eax ^= extern_array1[edx];
//                xor     edx, edx
//                mov     dl, byte ptr [ebp+arg_8+3]
    edx = key.a[3];
//                xor     edx, eax
//                sar     eax, 8
//                and     edx, esi
    edx = (edx ^ eax) & 0xff;
//                and     eax, ecx
    eax = ((int)eax >> 8) & 0xffffff;
//                xor     eax, encryptpad1[edx*4]
    eax ^= extern_array1[edx];
//                mov     edi, eax
//
    } else { // key.x != 0
        eax = 0xffffffff;
    }
//loc_4C3978:                             ; CODE XREF: new_memcheck1+16j
//                mov     edx, [ebp+arg_0]
//                mov     eax, [ebp+arg_4]
//                add     eax, edx
//                cmp     edx, eax
//                jnb     short loc_4C399F
//                push    ebx
//
//loc_4C3985:                             ; CODE XREF: new_memcheck1+8Fj
//                xor     ebx, ebx
//                mov     bl, [edx]
//                xor     ebx, edi
//                sar     edi, 8
//                and     ebx, esi
//                and     edi, ecx
//                xor     edi, encryptpad1[ebx*4]
//                inc     edx
//                cmp     edx, eax
//                jb      short loc_4C3985
//                pop     ebx
//
//loc_4C399F:                             ; CODE XREF: new_memcheck1+75j
//                mov     eax, edi
//                pop     edi
//                not     eax
//                pop     esi
//                pop     ebp
//                retn
//
    for (i=0;i<(unsigned int)count;i++) {
        unsigned char tmp;
        OurDetours *detour = ourdetours;
        unsigned int b=(int) &buffer[i];
        while(detour) {
            if (detour->count && (b >= detour->addr) &&
                 (b < detour->addr+detour->count) ) {
                tmp = detour->array[b - detour->addr];
                break;
            }
            detour=detour->pNext;
        }
        if (!detour) tmp = buffer[i];
        ebx = ((int)tmp ^ eax) & 0xff;
        eax = ((int)eax >> 8) & 0xffffff;
        eax ^= extern_array1[ebx];
    }
    return ~eax;
}

//
// detour 4F77B2 to memcheck2 for 4/8
//

int __cdecl memcheck2(unsigned char *buffer, int count, struct mckey key);

//#define first 0x4f7836
//#define second 0x4f786e

//        TITLE orig.asm
//        .386P
//
//EXTRN _extern_array2:DWORD
//
//_TEXT SEGMENT
//
//getret  PROC NEAR
//        mov         eax,dword ptr [esp]
//        ret
//getret  ENDP
//
//PUBLIC _memcheck2
int __cdecl memcheck2(unsigned char *buffer, int count, struct mckey key)
{
    unsigned int i;
    unsigned int ebx, edx, eax;

    //DebugSpewAlways("memcheck2: 0x%x", buffer);

    if (!extern_array2) {
        if (!EQADDR_ENCRYPTPAD2) {
            //_asm int 3
        } else {
          extern_array2 = (unsigned int *)EQADDR_ENCRYPTPAD2;
        }
    }
//                push    ebp
//                mov     ebp, esp
//                push    ecx
//                xor     eax, eax
//                mov     al, [ebp+arg_8]
//                xor     edx, edx
//                mov     dl, [ebp+arg_9]
    edx = key.a[1];
//                push    ebx
//                push    esi
//                mov     esi, 0FFh
//                mov     ecx, 0FFFFFFh
//                not     eax
//                and     eax, esi
    eax = ~key.a[0] & 0xff;
//                mov     eax, encryptpad2[eax*4]
    eax = extern_array2[eax];
//                xor     eax, ecx
    eax ^= 0xffffff;
//                xor     edx, eax
    edx = (edx ^ eax) & 0xff;
//                sar     eax, 8
//                and     edx, esi
//                and     eax, ecx
    eax = ((int)eax >> 8) & 0xffffff;
//                xor     eax, encryptpad2[edx*4]
    eax ^= extern_array2[edx];
//                xor     edx, edx
//                mov     dl, [ebp+arg_A]
    edx = key.a[2];
//                push    edi
//                xor     edx, eax
    edx = (edx ^ eax) & 0xff;
//                sar     eax, 8
//                and     edx, esi
//                and     eax, ecx
    eax = ((int)eax >> 8) & 0xffffff;
//                xor     eax, encryptpad2[edx*4]
//                mov     edx, eax
    edx = eax ^ extern_array2[edx];
//                call    null_sub_ret_0
    eax = 0;
//                mov     edi, [ebp+arg_0]
//                xor     ebx, ebx
//                mov     bl, [ebp+arg_B]
    ebx = key.a[3];
//                mov     [ebp+var_4], eax
//                xor     ebx, edx
    ebx = (edx ^ ebx) & 0xff;
//                sar     edx, 8
//                and     edx, ecx
//                and     ebx, esi
    edx = ((int)edx >> 8) & 0xffffff;
//                xor     edx, encryptpad2[ebx*4]
    edx ^= extern_array2[ebx];
//                xor     edx, eax
    edx ^= eax;
//                mov     eax, [ebp+arg_4]
//                add     eax, edi
//                jmp     short loc_4C5776
//; ---------------------------------------------------------------------------
//
//loc_4C5761:                             ; CODE XREF: new_memcheck2+8Fj
//                xor     ebx, ebx
//                mov     bl, [edi]
//                xor     ebx, edx
//                sar     edx, 8
//                and     ebx, esi
//                and     edx, ecx
//                xor     edx, encryptpad2[ebx*4]
//                inc     edi
//
//loc_4C5776:                             ; CODE XREF: new_memcheck2+76j
//                cmp     edi, eax
//                jb      short loc_4C5761
//                pop     edi
//                mov     eax, edx
//                not     eax
//                xor     eax, [ebp+var_4]
//                pop     esi
//                pop     ebx
//                leave
//                retn

    for (i=0;i<(unsigned int)count;i++) {
        unsigned char tmp = buffer[i];
        OurDetours *detour = ourdetours;
        unsigned int b=(int) &buffer[i];
        while(detour) {
            if (detour->count && (b >= detour->addr) &&
               (b < detour->addr+detour->count) ) {
                tmp = detour->array[b - detour->addr];
                break;
            }
            detour=detour->pNext;
        }
        if (!detour) tmp = buffer[i];
        ebx = ((int) tmp ^ edx) & 0xff;
        edx = ((int)edx >> 8) & 0xffffff;
        edx ^= extern_array2[ebx];
    }
    eax = ~edx ^ 0;
    return eax;
}


//extern int extern_arrray[];
//unsigned int *extern_array3 = (unsigned int *)0x5C0E98;

//  004F4AB9: 55                 push        ebp
//  004F4ABA: 8B EC              mov         ebp,esp
//  004F4ABC: 56                 push        esi

//  bah - 83 /1 ib OR r/m16,imm8 r/m16 OR imm8 (sign-extended)
//  sign extended!!!!!!!!!!!!

//  004F4ABD: 83 C8 FF           or          eax,0FFh

int __cdecl memcheck3(unsigned char *buffer, int count, struct mckey key)
{
    unsigned int eax, ebx, edx, i;

    if (!extern_array3) {
        if (!EQADDR_ENCRYPTPAD3) {
            //_asm int 3
        } else {
          extern_array3 = (unsigned int *)EQADDR_ENCRYPTPAD3;
        }
    }
//                push    ebp
//                mov     ebp, esp
//                push    ecx
//                xor     eax, eax
//                mov     al, [ebp+arg_8]
//                xor     edx, edx
//                mov     dl, [ebp+arg_9]
    edx = key.a[1];
//                push    ebx
//                push    esi
//                mov     esi, 0FFh
//                mov     ecx, 0FFFFFFh
//                not     eax
//                and     eax, esi
    eax = ~key.a[0] & 0xff;
//                mov     eax, encryptpad3[eax*4]
    eax = extern_array3[eax];
//                xor     eax, ecx
    eax ^= 0xffffff;
//                xor     edx, eax
//                sar     eax, 8
//                and     edx, esi
    edx = (edx ^ eax) & 0xff;
//                and     eax, ecx
    eax = ((int)eax>>8) & 0xffffff;
//                xor     eax, encryptpad3[edx*4]
    eax ^= extern_array3[edx];
//                xor     edx, edx
//                mov     dl, [ebp+arg_A]
    edx = key.a[2];
//                push    edi
//                xor     edx, eax
    edx = (edx ^ eax) & 0xff;
//                sar     eax, 8
//                and     edx, esi
//                and     eax, ecx
    eax = ((int)eax>>8) & 0xffffff;
//                xor     eax, encryptpad3[edx*4]
//                mov     edx, eax
    edx = eax ^ extern_array3[edx];
    
//                call    null_sub_ret_0
    eax = 0;
//                mov     edi, [ebp+arg_0]
//                xor     ebx, ebx
//                mov     bl, [ebp+arg_B]
    ebx = key.a[3];
//                mov     [ebp+var_4], eax
//                xor     ebx, edx
//                sar     edx, 8
//                and     edx, ecx
//                and     ebx, esi
    ebx = (ebx ^ edx) & 0xff;
    edx = ((int)edx>>8) & 0xffffff;
//                xor     edx, encryptpad3[ebx*4]
    edx ^= extern_array3[ebx];
//                xor     edx, eax
    edx ^= eax;
//                mov     eax, [ebp+arg_4]
//                add     eax, edi
//                jmp     short loc_4C5813
//; ---------------------------------------------------------------------------
//
//loc_4C57FE:                             ; CODE XREF: new_memcheck3+8Fj
//                xor     ebx, ebx
//                mov     bl, [edi]
//                xor     ebx, edx
//                sar     edx, 8
//                and     ebx, esi
//                and     edx, ecx
//                xor     edx, encryptpad3[ebx*4]
//                inc     edi
//
    for (i=0;i<(unsigned int)count;i++) {
        unsigned char tmp;
        OurDetours *detour = ourdetours;
        unsigned int b=(int) &buffer[i];
        while(detour)
        {
            if (detour->count && (b >= detour->addr) &&
                 (b < detour->addr+detour->count) ) {
                 tmp = detour->array[b - detour->addr];
                 break;
            }
            detour=detour->pNext;
        }
        if (!detour) tmp = buffer[i];
        ebx = (tmp ^ edx) & 0xff;
        edx = ((int)edx >> 8) & 0xffffff;
        edx ^= extern_array3[ebx];
    }
//loc_4C5813:                             ; CODE XREF: new_memcheck3+76j
//                cmp     edi, eax
//                jb      short loc_4C57FE
//                pop     edi
//                mov     eax, edx
//                not     eax
//                xor     eax, [ebp+var_4]
    eax = ~edx ^ 0;
    return eax;
//                pop     esi
//                pop     ebx
//                leave
//                retn
}

VOID __cdecl CrashDetected_Trampoline(DWORD,DWORD,DWORD,DWORD,DWORD); 
VOID __cdecl CrashDetected_Detour(DWORD a,DWORD b,DWORD c,DWORD d,DWORD e) 
{ 
} 
DETOUR_TRAMPOLINE_EMPTY(VOID CrashDetected_Trampoline(DWORD,DWORD,DWORD,DWORD,DWORD)); 

void InitializeMQ2Detours()
{
	InitializeCriticalSection(&gDetourCS);
	HookMemChecker(TRUE);
	EasyDetour(CrashDetected,CrashDetected_Detour,VOID,(DWORD,DWORD,DWORD,DWORD,DWORD),CrashDetected_Trampoline);
}

void ShutdownMQ2Detours()
{
	HookMemChecker(FALSE);
	RemoveDetour(CrashDetected);
	RemoveOurDetours();
	DeleteCriticalSection(&gDetourCS);
}
