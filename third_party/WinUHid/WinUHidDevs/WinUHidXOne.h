#pragma once

#include "WinUHidDevs.h"

typedef struct _WINUHID_XONE_GAMEPAD *PWINUHID_XONE_GAMEPAD;

//
// Optional callback to be invoked when the state of the force feedback motors change.
//
// Note: The values are specified as percentages!
//
// NOTE: This callback is invoked in the context of rumble thread and delaying it
// may impact the reliability and timing of force feedback effects. Do not perform
// any operations that may block in this context.
//
typedef VOID WINUHID_XONE_FF_CB(PVOID CallbackContext, UCHAR LeftMotor, UCHAR RightMotor, UCHAR LeftTriggerMotor, UCHAR RightTriggerMotor);
typedef WINUHID_XONE_FF_CB *PWINUHID_XONE_FF_CB;

//
// Creates a new Xbox One gamepad. Device info can optionally be provided to override
// a subset of identification information of the created device.
//
// To destroy the device, call WinUHidXOneDestroy().
//
// On failure, the function will return NULL. Call GetLastError() to the error code.
//
WINUHID_API PWINUHID_XONE_GAMEPAD WinUHidXOneCreate(PCWINUHID_PRESET_DEVICE_INFO Info, PWINUHID_XONE_FF_CB Callback, PVOID CallbackContext);

#include <pshpack1.h>

typedef struct _WINUHID_XONE_INPUT_REPORT
{
	USHORT LeftStickX; // 0x8000 is centered
	USHORT LeftStickY; // 0x8000 is centered
	USHORT RightStickX; // 0x8000 is centered
	USHORT RightStickY; // 0x8000 is centered
	USHORT LeftTrigger : 10;
	USHORT RightTrigger : 10;
	UCHAR ButtonA : 1;
	UCHAR ButtonB : 1;
	UCHAR ButtonX : 1;
	UCHAR ButtonY : 1;
	UCHAR ButtonLB : 1;
	UCHAR ButtonRB : 1;
	UCHAR ButtonBack : 1;
	UCHAR ButtonMenu : 1;
	UCHAR ButtonLS : 1;
	UCHAR ButtonRS : 1;
	UCHAR Reserved3: 6;
	UCHAR Hat : 4; // Use WinUHidXOneSetHatState()
	UCHAR Reserved4: 4;
	UCHAR ButtonHome : 1;
	UCHAR Reserved5: 7;
	UCHAR BatteryLevel; // 0 - 0xFF
} WINUHID_XONE_INPUT_REPORT, *PWINUHID_XONE_INPUT_REPORT;
typedef CONST WINUHID_XONE_INPUT_REPORT *PCWINUHID_XONE_INPUT_REPORT;

#include <poppack.h>

//
// Initializes the input report with neutral data.
//
WINUHID_API VOID WinUHidXOneInitializeInputReport(PWINUHID_XONE_INPUT_REPORT Report);

//
// Sets the hat state in the input report.
//
// Each axis of the hat can have one of 3 values: -1, 0, 1
//
// Negative values are left/up. Positive values are right/down. 0 is neutral.
//
WINUHID_API VOID WinUHidXOneSetHatState(PWINUHID_XONE_INPUT_REPORT Report, INT HatX, INT HatY);

//
// Submits an input report to the device.
//
// On failure, the function will return FALSE. Call GetLastError() to the error code.
//
WINUHID_API BOOL WinUHidXOneReportInput(PWINUHID_XONE_GAMEPAD Gamepad, PCWINUHID_XONE_INPUT_REPORT Report);

//
// Destroys the Xbox One gamepad.
//
// This function never fails as long as the provided argument is valid.
//
WINUHID_API VOID WinUHidXOneDestroy(PWINUHID_XONE_GAMEPAD Gamepad);