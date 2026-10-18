# Driver build notes

The regular `build.ps1` builds the application using the driver/installers already included in `drivers/`. It does not install a driver, import a certificate or change the controller. The source archive intentionally contains these distributable binary dependencies and their licenses; it excludes private signing keys and research-device data.

## Included WinUHid package

The pinned MIT WinUHid source is under `third_party/WinUHid/WinUHid Driver`, commit `d6cebbef5c7909168d1f881185be8f607d6aefd4`. The driver preserves upstream device/report behavior and SYSTEM/Administrators/UMDF access policy. Build adaptations are limited to user-mode tracing glue. `src/Driver/device.cpp` owns the fixed device `ROOT\WINUHID\FLYDIGI_BRIDGE`, provider `Flydigi Bridge Driver`. It refuses unrelated device instances during installation/removal.

Package version 1.0.0.2 is signed with the project's self-signed development code-signing certificate, subject `CN=Flydigi Bridge Driver`. The public certificate, DLL, INF, catalog, helper, source attribution and hash manifest ship together. The temporary private key was generated outside Windows certificate stores and deleted after signing. The development PC's earlier research certificate and key are not distributed.

The application's explicit install confirmation requests trusting this public certificate in Windows LocalMachine Root and TrustedPublisher stores. This trust persists. Signature/hash checks are performed before installation, and a receipt records only this application's own additions. Existing working WinUHid installations are kept. The driver is not WHQL-certified; clean-PC installation/removal has not yet been tested. A Windows policy refusal is reported, never bypassed by changing Secure Boot or enabling test-signing.

## Rebuild the driver payload

Use Visual Studio C++ Build Tools, Windows SDK signing/tracing tools and a WDK containing UMDF 2.15 libraries:

```powershell
.\tools\build-driver.ps1 -SdkRoot 'C:\path\to\SDK' -WdkRoot 'C:\path\to\WDK' -KitVersion '10.0.26100.0'
```

The script compiles the owned helper, generates a new offline code-signing certificate, signs the DLL, creates/signs its catalog and updates `drivers/WinUHid/package.json`. It installs nothing. The temporary PFX/password are deleted in `finally`; only the public certificate remains in the payload. Rebuild the desktop EXE afterwards to embed the new package.

Each driver rebuild creates a different certificate. Keep the previous installing application for removing its recorded installation before switching packages. A mismatched receipt is deliberately refused. For public production distribution, a supported production signing workflow and clean-PC validation remain release work.

## Other bundled installers

Official unmodified HidHide 1.5.230, Microsoft GameInput 3.5.270 and Visual C++ x64 installers are pinned in `drivers/installers.json`. Their signatures, hashes and licenses are checked/preserved. Microsoft licenses are separate from the project's MIT license. The Flydigi Space Station installer and proprietary transport driver are not bundled.
