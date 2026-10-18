#include "pch.h"
#include "WinUHidDevs.h"

VOID PopulateDeviceInfo(PWINUHID_DEVICE_CONFIG Config, PCWINUHID_PRESET_DEVICE_INFO Info)
{
	if (!Info) {
		return;
	}

	if (Info->VendorID != 0) {
		Config->VendorID = Info->VendorID;
		Config->ProductID = Info->ProductID;
		Config->VersionNumber = Info->VersionNumber;
	}

	Config->ContainerId = Info->ContainerId;
	Config->InstanceID = Info->InstanceID;
	Config->HardwareIDs = Info->HardwareIDs;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}