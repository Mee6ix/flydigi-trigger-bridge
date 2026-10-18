# Flydigi Trigger Bridge

**Independent trigger rumble over your wireless dongle.**

Repository name: `flydigi-trigger-bridge`.

A Windows desktop application for independent grip and trigger rumble over the Flydigi wireless dongle. The interface and documentation are in English.

Games connect to a virtual Xbox controller with four rumble channels. Their values are forwarded unchanged: **61 → 61, 100 → 100**, with no gain control or amplitude reduction. The bridge temporarily adjusts the working controller profile and restores the original profile when stopped. Reconnecting the same controller reapplies the temporary adjustment automatically.

## Supported configuration

- Windows 10/11, x64, .NET Framework 4.8.
- Flydigi Vader 4 Pro, model 105, controller firmware **6.9.5.9**, using its dongle.
- The audited Space Station transport driver, SHA-256 `04f6e18f5b08bfee0411e2e75897b7dea74f9baf83148e84dc59516ebab58913`.
- WinUHid driver, Microsoft GameInput v3 runtime, and HidHide.

Other controller firmware and transport driver versions are refused until validated. Bluetooth is not the bridge's physical transport. Separate trigger effects also depend on game support.

## Run

1. Launch `FlydigiTriggerBridge.exe` and accept its administrator prompt.
2. Open **Setup**. Use the **Install** buttons for missing WinUHid, GameInput and HidHide components. Each installation requires explicit confirmation. Use **Get Space Station** for the separate Flydigi transport prerequisite, then click **Check setup**.
3. Select your intended profile with **Trigger Vibration Switch enabled** and no macros. Close Space Station, including its tray icon. Switch on the controller through the dongle.
4. Click **Start bridge**. On the first run of a new engine build, keep the small four-channel validation window active briefly.
5. Wait for **Ready to play**, then launch your game. Keep Steam Input enabled for Half-Life Legacy.
6. Click **Stop & restore** when finished. Closing the app also requests restoration and waits for cleanup. Minimizing can keep the app in the tray.

The bridge runs until stopped, rather than imposing a ten-minute gameplay limit. It preserves the virtual device during controller sleep and discards stale rumble while offline.

If stopped while the controller is unavailable, the original backup is retained and the Space Station service remains paused. Switch on the same controller and click **Restore profile** before reopening Space Station. The recovery path checks controller identity and the full profile before restoring it.

## Build from source

Install Visual Studio Build Tools with **Desktop development with C++** and a Windows SDK. The standard .NET Framework compiler is used for the GUI; a separate .NET SDK is not required.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

Output: `dist/FlydigiTriggerBridge.exe`. The native engine, session scripts, driver installers, documentation and license notices are embedded in the executable. They are extracted to a versioned folder under `%LOCALAPPDATA%\FlydigiBridge` at runtime. No companion console scripts need to be launched by the user.

The internal data folder, session mutexes, native engine filename and signed driver identities retain their original names for compatibility with existing profile backups, installation receipts and signatures. These are implementation identities; the application and release packages are named Flydigi Trigger Bridge. Rebranding does not reinstall or re-sign a driver.

The build downloads a pinned, checksum-verified Microsoft GameInput SDK header to `.cache/` if it is missing. WinUHid userspace source is vendored at a pinned MIT-licensed commit. Research binaries, firmware images, private profiles, signing keys and personal logs are not part of this repository. Building the app does not install drivers or change the controller.

`FlydigiTriggerBridge.sln` is also provided for Visual Studio; it additionally requires the .NET Framework 4.8 targeting pack. The validated build path is `build.ps1`.

To produce portable and source release archives after building:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\package.ps1
```

The EXE includes the WinUHid driver package, official HidHide 1.5.230 installer, Microsoft GameInput 3.5.270 redistributable and official Visual C++ x64 runtime installer. Setup keeps existing working components. The WinUHid package uses a development signing certificate: its confirmation explicitly requests adding the public certificate to Windows Trusted Root and Trusted Publisher stores. It is not WHQL-certified. No private signing key is shipped, and no Secure Boot or test-signing setting is changed. Installation on a clean PC and removal of a newly installed driver have not yet been validated. See [Setup](docs/SETUP.md) and [driver build notes](docs/DRIVER.md).

## Project layout

| Path | Purpose |
|---|---|
| `src/App/` | C# WinForms application, session model and UI |
| `src/Native/` | C++ bridge, transport and profile restoration |
| `src/Driver/` | Owned WinUHid device installation/removal helper |
| `runtime/` | Service and owned HidHide session orchestration |
| `drivers/` | Bundled driver/installers, public certificate, hashes and licenses |
| `third_party/WinUHid/` | Pinned userspace dependency and its license |
| `dependencies.json` | SDK source URL and checksum |
| `docs/` | Setup, architecture and validation notes |
| `.github/workflows/` | Windows build and portable EXE artifact |

## Validation

The build runs hardware-free native, application and installer checks. These cover exact amplitude mapping, refresh markers, reconnect identity, immutable original backups, UI state transitions, log tailing, command-line quoting, package signatures and tamper rejection. Physical four-channel duration and real game support are recorded separately in `docs/VALIDATION.md`.

For a visual preview without hardware access:

```powershell
.\dist\FlydigiTriggerBridge.exe --render .\build\dashboard.png
```

## License and attribution

Original project code, WinUHid and HidHide are MIT-licensed. Microsoft redistributables retain their separate license terms. Installed proprietary Flydigi software is a separate dependency; see `THIRD_PARTY_NOTICES.md`.

This is an unofficial community project, not affiliated with Flydigi or Microsoft. The bridge performs no firmware flash, DFU/IAP operation or persistent profile save.
