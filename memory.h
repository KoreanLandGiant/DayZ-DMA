#pragma once
#include <Windows.h>
#include "globals.h"
#include "offsets.h"

static __forceinline bool ZwCopyMemory(uint64_t address, PVOID buffer, uint64_t size, BOOL write = false)
{
	if (!globals.vmmManager || !globals.process_id)
		return false;

	if (!write)
	{
		return globals.vmmManager->readMemory(globals.process_id, address, buffer, (int)size);
	}
	else
	{
		return globals.vmmManager->writeMemory(globals.process_id, address, buffer, (int)size);
	}
};

template <typename T>
static T ReadData(uint64_t address)
{
	if (!address)
		return T();

	T buffer;
	return ZwCopyMemory(address, &buffer, sizeof(T), false) ? buffer : T();
};

template <typename T>
static void WriteData(uint64_t address, T data)
{
	if (!address)
		return;

	ZwCopyMemory(address, &data, sizeof(T), true);
};

static std::string ReadString(uint64_t address, size_t size)
{
	if (!address || size > 1024)
		return "";

	char string[1024] = "";
	return ZwCopyMemory(address, string, size, false) ? std::string(string) : "";
};

std::string ReadArmaString(uint64_t address)
{
	int length = ReadData<int>(address + OFF_LENGTH);

	std::string text = ReadString(address + OFF_TEXT, length);

	return text.c_str();
}

// =============================================
// Scatter read helpers for batching DMA reads
// =============================================
static VMMDLL_SCATTER_HANDLE ScatterBegin()
{
	if (!globals.vmmManager || !globals.process_id)
		return nullptr;
	return globals.vmmManager->initializeScatter(globals.process_id);
}

static void ScatterAdd(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* dest, int size)
{
	if (handle && address)
		globals.vmmManager->addScatterRead(handle, address, size, dest);
}

template <typename T>
static void ScatterAdd(VMMDLL_SCATTER_HANDLE handle, uint64_t address, T* dest)
{
	if (handle && address)
		globals.vmmManager->addScatterRead(handle, address, sizeof(T), dest);
}

static void ScatterExecute(VMMDLL_SCATTER_HANDLE handle)
{
	if (handle)
		globals.vmmManager->executeScatter(handle);
}
