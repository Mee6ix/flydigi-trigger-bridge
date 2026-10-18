#include "pch.h"
#include "WinUHidXOne.h"

#include <wrl/wrappers/corewrappers.h>
using namespace Microsoft::WRL;

//
// This device emulates a Microsoft Xbox One gamepad
//

const BYTE k_XOneReportDescriptor[] =
{
	0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
	0x09, 0x05,        // Usage (Game Pad)
	0xA1, 0x01,        // Collection (Application)
	0xA1, 0x00,        //   Collection (Physical)
	0x09, 0x30,        //     Usage (X)
	0x09, 0x31,        //     Usage (Y)
	0x15, 0x00,        //     Logical Minimum (0)
	0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
	0x95, 0x02,        //     Report Count (2)
	0x75, 0x10,        //     Report Size (16)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0xC0,              //   End Collection
	0xA1, 0x00,        //   Collection (Physical)
	0x09, 0x33,        //     Usage (Rx)
	0x09, 0x34,        //     Usage (Ry)
	0x15, 0x00,        //     Logical Minimum (0)
	0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
	0x95, 0x02,        //     Report Count (2)
	0x75, 0x10,        //     Report Size (16)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0xC0,              //   End Collection
	0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
	0x09, 0x32,        //   Usage (Z)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
	0x95, 0x01,        //   Report Count (1)
	0x75, 0x0A,        //   Report Size (10)
	0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0x00,        //   Logical Maximum (0)
	0x75, 0x06,        //   Report Size (6)
	0x95, 0x01,        //   Report Count (1)
	0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
	0x09, 0x35,        //   Usage (Rz)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
	0x95, 0x01,        //   Report Count (1)
	0x75, 0x0A,        //   Report Size (10)
	0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0x00,        //   Logical Maximum (0)
	0x75, 0x06,        //   Report Size (6)
	0x95, 0x01,        //   Report Count (1)
	0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x05, 0x09,        //   Usage Page (Button)
	0x19, 0x01,        //   Usage Minimum (0x01)
	0x29, 0x0A,        //   Usage Maximum (0x0A)
	0x95, 0x0A,        //   Report Count (10)
	0x75, 0x01,        //   Report Size (1)
	0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0x00,        //   Logical Maximum (0)
	0x75, 0x06,        //   Report Size (6)
	0x95, 0x01,        //   Report Count (1)
	0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
	0x09, 0x39,        //   Usage (Hat switch)
	0x15, 0x01,        //   Logical Minimum (1)
	0x25, 0x08,        //   Logical Maximum (8)
	0x35, 0x00,        //   Physical Minimum (0)
	0x46, 0x3B, 0x01,  //   Physical Maximum (315)
	0x66, 0x14, 0x00,  //   Unit (System: English Rotation, Length: Centimeter)
	0x75, 0x04,        //   Report Size (4)
	0x95, 0x01,        //   Report Count (1)
	0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
	0x75, 0x04,        //   Report Size (4)
	0x95, 0x01,        //   Report Count (1)
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0x00,        //   Logical Maximum (0)
	0x35, 0x00,        //   Physical Minimum (0)
	0x45, 0x00,        //   Physical Maximum (0)
	0x65, 0x00,        //   Unit (None)
	0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0xA1, 0x02,        //   Collection (Logical)
	0x05, 0x0F,        //     Usage Page (PID Page)
	0x09, 0x97,        //     Usage (0x97)
	0x15, 0x00,        //     Logical Minimum (0)
	0x25, 0x01,        //     Logical Maximum (1)
	0x75, 0x04,        //     Report Size (4)
	0x95, 0x01,        //     Report Count (1)
	0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	0x15, 0x00,        //     Logical Minimum (0)
	0x25, 0x00,        //     Logical Maximum (0)
	0x91, 0x03,        //     Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	0x09, 0x70,        //     Usage (0x70)
	0x15, 0x00,        //     Logical Minimum (0)
	0x25, 0x64,        //     Logical Maximum (100)
	0x75, 0x08,        //     Report Size (8)
	0x95, 0x04,        //     Report Count (4)
	0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	0x09, 0x50,        //     Usage (0x50)
	0x66, 0x01, 0x10,  //     Unit (System: SI Linear, Time: Seconds)
	0x55, 0x0E,        //     Unit Exponent (-2)
	0x26, 0xFF, 0x00,  //     Logical Maximum (255)
	0x95, 0x01,        //     Report Count (1)
	0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	0x09, 0xA7,        //     Usage (0xA7)
	0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	0x65, 0x00,        //     Unit (None)
	0x55, 0x00,        //     Unit Exponent (0)
	0x09, 0x7C,        //     Usage (0x7C)
	0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	0xC0,              //   End Collection
	0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
	0x09, 0x80,        //   Usage (Sys Control)
	0xA1, 0x00,        //   Collection (Physical)
	0x09, 0x85,        //     Usage (Sys Main Menu)
	0x15, 0x00,        //     Logical Minimum (0)
	0x25, 0x01,        //     Logical Maximum (1)
	0x95, 0x01,        //     Report Count (1)
	0x75, 0x01,        //     Report Size (1)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x15, 0x00,        //     Logical Minimum (0)
	0x25, 0x00,        //     Logical Maximum (0)
	0x75, 0x07,        //     Report Size (7)
	0x95, 0x01,        //     Report Count (1)
	0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0xC0,              //   End Collection
	0x05, 0x06,        //   Usage Page (Generic Dev Ctrls)
	0x09, 0x20,        //   Usage (Battery Strength)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0xFF, 0x00,  //   Logical Maximum (255)
	0x75, 0x08,        //   Report Size (8)
	0x95, 0x01,        //   Report Count (1)
	0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0xC0,              // End Collection
};

const WINUHID_DEVICE_CONFIG k_XOneConfig =
{
	WINUHID_EVENT_WRITE_REPORT,
	0x045e, // Microsoft
	0x02ff, // Xbox One Controller
	0,
	sizeof(k_XOneReportDescriptor),
	k_XOneReportDescriptor,
	{},
	NULL,
	L"HID\\VID_045E&PID_02FF&IG_00\0",
};

typedef struct _XONE_OUTPUT_REPORT {
	UCHAR MotorsEnabled;
	UCHAR LeftTriggerMotor;
	UCHAR RightTriggerMotor;
	UCHAR LeftMotor;
	UCHAR RightMotor;
	UCHAR Duration; // 10ms units
	UCHAR Delay; // 10ms units
	UCHAR Repeat;
} XONE_OUTPUT_REPORT, *PXONE_OUTPUT_REPORT;
#define XONE_TIME_TO_FILETIME(x) ((x) * -100000)

typedef struct _WINUHID_XONE_GAMEPAD {
	PWINUHID_DEVICE Device;
	SRWLOCK Lock;

	HANDLE RumbleThread;
	HANDLE RumbleUpdatedEvent;
	BOOL Stopping;
	XONE_OUTPUT_REPORT RumbleState;

	PWINUHID_XONE_FF_CB Callback;
	PVOID CallbackContext;
} WINUHID_XONE_GAMEPAD, *PWINUHID_XONE_GAMEPAD;

VOID WinUHidXOneCallback(PVOID CallbackContext, PWINUHID_DEVICE Device, PCWINUHID_EVENT Event)
{
	auto gamepad = (PWINUHID_XONE_GAMEPAD)CallbackContext;

	//
	// There's only one defined output report
	//
	if (Event->ReportId != 0 || Event->Write.DataLength < sizeof(XONE_OUTPUT_REPORT)) {
		WinUHidCompleteWriteEvent(Device, Event, FALSE);
		return;
	}

	//
	// Pass the report over to the rumble thread if a FF callback is registered
	//
	if (gamepad->RumbleThread) {
		auto report = (PXONE_OUTPUT_REPORT)Event->Write.Data;
		AcquireSRWLockExclusive(&gamepad->Lock);
		gamepad->RumbleState = *report;
		ReleaseSRWLockExclusive(&gamepad->Lock);
		SetEvent(gamepad->RumbleUpdatedEvent);
	}

	WinUHidCompleteWriteEvent(Device, Event, TRUE);
}

DWORD WINAPI XOneRumbleThreadProc(LPVOID lpParameter)
{
	auto device = (PWINUHID_XONE_GAMEPAD)lpParameter;

	//
	// Use a high resolution timer to ensure maximum effect timing accuracy. This timer
	// will only be set while a FF effect is playing on the device, so it won't impact
	// system power usage in any meaningful way.
	//
	Wrappers::HandleT<Wrappers::HandleTraits::HANDLENullTraits> timer{
		CreateWaitableTimerExW(NULL, NULL,
			CREATE_WAITABLE_TIMER_MANUAL_RESET | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
			TIMER_ALL_ACCESS)
	};
	if (!timer.IsValid()) {
		return GetLastError();
	}

	while (!device->Stopping) {
		//
		// Wait for an output report
		//
		DWORD result = WaitForSingleObject(device->RumbleUpdatedEvent, INFINITE);
		if (device->Stopping) {
			break;
		}

		//
		// Load the most recent FF output report
		//
		AcquireSRWLockShared(&device->Lock);
		XONE_OUTPUT_REPORT outputReport = device->RumbleState;
		ResetEvent(device->RumbleUpdatedEvent);
		ReleaseSRWLockShared(&device->Lock);

		//
		// Determine the motor amplitudes to send to the callback
		//
		UCHAR leftMotor = (outputReport.MotorsEnabled & 0x1) ? outputReport.LeftMotor : 0;
		UCHAR rightMotor = (outputReport.MotorsEnabled & 0x2) ? outputReport.RightMotor : 0;
		UCHAR leftTriggerMotor = (outputReport.MotorsEnabled & 0x4) ? outputReport.LeftTriggerMotor : 0;
		UCHAR rightTriggerMotor = (outputReport.MotorsEnabled & 0x8) ? outputReport.RightTriggerMotor : 0;

		for (USHORT i = 0; i <= outputReport.Repeat; i++) {
			HANDLE objects[]{ device->RumbleUpdatedEvent, timer.Get() };
			LARGE_INTEGER dueTime;

			//
			// Check to see if we need to do anything
			//
			if (outputReport.Duration == 0) {
				device->Callback(device->CallbackContext, 0, 0, 0, 0);
				break;
			}
			else if (leftMotor == 0 && rightMotor == 0 && leftTriggerMotor == 0 && rightTriggerMotor == 0) {
				device->Callback(device->CallbackContext, 0, 0, 0, 0);
				break;
			}

			//
			// Wait for the delay period before initiating the FF effect
			//
			if (outputReport.Delay != 0) {
				dueTime.QuadPart = XONE_TIME_TO_FILETIME(outputReport.Delay);
				SetWaitableTimer(timer.Get(), &dueTime, 0, NULL, NULL, FALSE);
				result = WaitForMultipleObjects(ARRAYSIZE(objects), objects, FALSE, INFINITE);
				if (result == WAIT_OBJECT_0 || device->Stopping) {
					//
					// We got a new output report, so break and start over
					//
					break;
				}
			}

			//
			// Notify the user callback that the motors are now on
			//
			device->Callback(device->CallbackContext, leftMotor, rightMotor, leftTriggerMotor, rightTriggerMotor);

			//
			// Wait for the duration period before stopping the FF effect
			//
			dueTime.QuadPart = XONE_TIME_TO_FILETIME(outputReport.Duration);
			if (outputReport.Delay == 0 && outputReport.Repeat != 0) {
				//
				// We specially handle the common case of repeats with no delay specified.
				// We can optimize the series of short waits and motor on/off cycles by
				// consolidating all the repeats into the initial wait.
				//
				dueTime.QuadPart += dueTime.QuadPart * outputReport.Repeat;
				outputReport.Repeat = 0;
			}
			SetWaitableTimer(timer.Get(), &dueTime, 0, NULL, NULL, FALSE);
			result = WaitForMultipleObjects(ARRAYSIZE(objects), objects, FALSE, INFINITE);
			if (result == WAIT_OBJECT_0 || device->Stopping) {
				//
				// We got a new output report, so break and start over
				//
				break;
			}

			//
			// Notify the user callback that the motors are now off
			//
			device->Callback(device->CallbackContext, 0, 0, 0, 0);
		}
	}

	return GetLastError();
}

WINUHID_API PWINUHID_XONE_GAMEPAD WinUHidXOneCreate(PCWINUHID_PRESET_DEVICE_INFO Info, PWINUHID_XONE_FF_CB Callback, PVOID CallbackContext)
{
	WINUHID_DEVICE_CONFIG config = k_XOneConfig;
	PopulateDeviceInfo(&config, Info);

	if (config.VendorID == 0) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return NULL;
	}

	PWINUHID_XONE_GAMEPAD gamepad = (PWINUHID_XONE_GAMEPAD)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*gamepad));
	if (!gamepad) {
		SetLastError(ERROR_OUTOFMEMORY);
		return NULL;
	}

	InitializeSRWLock(&gamepad->Lock);
	gamepad->Callback = Callback;
	gamepad->CallbackContext = CallbackContext;

	//
	// If the caller wants FF callbacks, start the rumble thread
	//
	if (gamepad->Callback) {
		gamepad->RumbleUpdatedEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
		if (!gamepad->RumbleUpdatedEvent) {
			WinUHidXOneDestroy(gamepad);
			return NULL;
		}

		gamepad->RumbleThread = CreateThread(NULL, 0, XOneRumbleThreadProc, gamepad, 0, NULL);
		if (!gamepad->RumbleThread) {
			WinUHidXOneDestroy(gamepad);
			return NULL;
		}

		SetThreadDescription(gamepad->RumbleThread, L"WinUHidDev XOne Rumble Thread");
	}

	gamepad->Device = WinUHidCreateDevice(&config);
	if (!gamepad->Device) {
		WinUHidXOneDestroy(gamepad);
		return NULL;
	}

	if (!WinUHidStartDevice(gamepad->Device, WinUHidXOneCallback, gamepad)) {
		WinUHidXOneDestroy(gamepad);
		return NULL;
	}

	//
	// Send an neutral input report
	//
	WINUHID_XONE_INPUT_REPORT inputReport;
	WinUHidXOneInitializeInputReport(&inputReport);
	if (!WinUHidXOneReportInput(gamepad, &inputReport)) {
		WinUHidXOneDestroy(gamepad);
		return NULL;
	}

	return gamepad;
}

WINUHID_API VOID WinUHidXOneInitializeInputReport(PWINUHID_XONE_INPUT_REPORT Report)
{
	RtlZeroMemory(Report, sizeof(*Report));

	Report->LeftStickX = 0x8000;
	Report->LeftStickY = 0x8000;
	Report->RightStickX = 0x8000;
	Report->RightStickY = 0x8000;
	Report->BatteryLevel = 0xFF;
}

WINUHID_API VOID WinUHidXOneSetHatState(PWINUHID_XONE_INPUT_REPORT Report, INT HatX, INT HatY)
{
	if (HatX == 0 && HatY == 0) {
		Report->Hat = 0x0; // Neutral
	}
	else if (HatX < 0 && HatY < 0) {
		Report->Hat = 0x8; // Top-left
	}
	else if (HatX < 0 && HatY == 0) {
		Report->Hat = 0x7; // Left
	}
	else if (HatX < 0 && HatY > 0) {
		Report->Hat = 0x6; // Bottom-left
	}
	else if (HatX == 0 && HatY > 0) {
		Report->Hat = 0x5; // Bottom
	}
	else if (HatX > 0 && HatY > 0) {
		Report->Hat = 0x4; // Bottom-right
	}
	else if (HatX > 0 && HatY == 0) {
		Report->Hat = 0x3; // Right
	}
	else if (HatX > 0 && HatY < 0) {
		Report->Hat = 0x2; // Top-right
	}
	else {
		Report->Hat = 0x1; // Top
	}
}

WINUHID_API BOOL WinUHidXOneReportInput(PWINUHID_XONE_GAMEPAD Gamepad, PCWINUHID_XONE_INPUT_REPORT Report)
{
	return WinUHidSubmitInputReport(Gamepad->Device, Report, sizeof(*Report));
}

WINUHID_API VOID WinUHidXOneDestroy(PWINUHID_XONE_GAMEPAD Gamepad)
{
	if (!Gamepad) {
		return;
	}

	Gamepad->Stopping = TRUE;

	if (Gamepad->RumbleThread) {
		SetEvent(Gamepad->RumbleUpdatedEvent);
		WaitForSingleObject(Gamepad->RumbleThread, INFINITE);
		CloseHandle(Gamepad->RumbleThread);
	}

	if (Gamepad->RumbleUpdatedEvent) {
		CloseHandle(Gamepad->RumbleUpdatedEvent);
	}

	if (Gamepad->Device) {
		WinUHidDestroyDevice(Gamepad->Device);
	}

	HeapFree(GetProcessHeap(), 0, Gamepad);
}