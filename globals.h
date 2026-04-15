#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <cstdint>
#include "DMALib/includes/DMAMemoryManagement/includes.h"
#include "vector.h"

struct _globals
{
	DWORD process_id;
	DMAMem::VmmManager* vmmManager;
	DMAMem::StaticManager* staticManager;

	HWND OverlayWnd;
	HWND TargetWnd;
	int Width, Height;
	bool console;
	const char lTargetWindow[256] = "DayZ";
	const char lWindowName[256] = "DayZ Client";

	uintptr_t modulebase = NULL;
	uint64_t Base;
	uint64_t World;
};

extern LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef struct _player_t
{
	std::uint64_t EntityPtr;
	std::uint64_t TableEntry;
	int NetworkID;

	// Cached data from scatter reads (populated in update_list)
	std::uint64_t visualStatePtr;
	std::uint64_t entityTypePtr;
	std::uint64_t playerSkeletonPtr;
	std::uint64_t zmSkeletonPtr;
	std::uint64_t playerAnimClass;
	std::uint64_t playerMatrixClass;
	std::uint64_t zmAnimClass;
	std::uint64_t zmMatrixClass;
	float coordX, coordY, coordZ;
	std::uint8_t isDead;

	// Pre-computed bone world positions (populated in background thread)
	static const int MAX_BONE_IDX = 114; // max bone index is 113
	Vector3 bonePositions[114];
	bool boneValid[114];
	bool bonesCached;
	std::string entityTypeName;
	std::string entityRealName;
	std::string playerName;
	std::string itemInHands;
	bool dataCached;
} player_t;

typedef struct _item_t
{
	std::uint64_t ItemPtr;
	std::uint64_t ItemTable;
} item_t;

extern _globals globals;