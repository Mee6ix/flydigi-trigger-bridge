# Architecture

`FlydigiTriggerBridge.exe` contains a C# WinForms UI and an embedded runtime ZIP. It requires administrator access for the existing transport port and service management. The executable verifies and locks its packaged scripts/engine before a session starts.

The display name, assembly/namespace, solution/project and public packages use Flydigi Trigger Bridge. Compatibility identifiers intentionally remain unchanged: `%LOCALAPPDATA%\FlydigiBridge`, the shared application/hardware mutexes, receipt owner values, `FlydigiBridgeEngine.exe`, and the signed WinUHid provider/certificate/device identity. This preserves previous backups and prevents the old and renamed applications from controlling hardware concurrently. No signed driver file is edited for rebranding.

The same package embeds installer payloads, licenses, public signing certificate and pinned manifests. Setup checks are read-only. Installation is a separate explicit mode with an unchecked authorization/license box; it does not begin a bridge session or change the controller profile. Scripts and all installer files remain verified and read-locked for the worker lifetime. Official installers are hash/signer checked; the owned driver also uses cryptographic signature verification before its explicitly authorized certificate trust change. Existing working components are kept. Persistent driver installation is distinct from temporary gameplay changes.

The WinUHid installation receipt records the exact owned root node, package and new certificate trust additions. Removal refuses changed or unrelated installations. No private key is embedded, no OS security setting is bypassed, and installation cannot run while profile recovery is pending. Installer completion/restart messages remain visible in the UI; closing the app waits for installer completion.

The session runner performs prerequisite checks, validates the virtual controller once per engine checksum, pauses the Space Station service, and creates an owned HidHide session. It launches the native engine without a console window. Session events are tailed asynchronously by the UI; it does not block the Windows message loop while waiting for a controller.

The native engine reads physical input and forwards it to a WinUHid Xbox One preset. Four rumble values0..100 are copied unchanged into the proprietary motor packet. A reserved byte alternates0/1 to refresh constant output without changing amplitude. Firmware identity, controller address and the complete840-byte profile are checked before RAM changes.

A durable original-profile backup is flushed to disk before the first RAM patch. A pending journal preserves the original service intent and previous backup on recovery. GUI Stop signals a named event. GUI process death is independently monitored by the engine. Neither path terminates the worker forcibly: restoration is attempted before service/visibility cleanup.

When the controller disappears, the virtual device stays connected with neutral input. Stale rumble is discarded. The same controller is verified and repatched when it returns. Stopping while offline retains the original backup and leaves the service stopped. Restore profile uses a dedicated restoration mode without creating a virtual device or reapplying the gameplay patch.

No flash, firmware upload, DFU/IAP operation or persistent profile-save command exists in the allowed write path. The tested firmware and transport driver versions are deliberately narrow. Physical motor force is still subject to the controller's firmware/profile behavior; the app preserves the game's numeric commands, not a calibrated physical force.
