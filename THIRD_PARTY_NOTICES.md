# Third-party notices

## WinUHid

Source: https://github.com/cgutman/WinUHid

Pinned commit: d6cebbef5c7909168d1f881185be8f607d6aefd4.

Copyright (c) 2025 Cameron Gutman. MIT License. The license is included at `third_party/WinUHid/LICENSE` and `drivers/WinUHid/LICENSE`. The userspace Xbox One preset is compiled into the engine. The bundled UMDF driver is built from the pinned source with local tracing adaptations and retains the upstream access policy. The project driver helper is original MIT-licensed code.

## Microsoft GameInput

Source: https://github.com/microsoftconnect/GameInput

SDK header commit: 9b8fc167466ec09a710bb48beae4fdf85e8330b5. The header is downloaded for compilation and is not included in the source archive. The official GameInput 3.5.270 redistributable from Microsoft's NuGet package is bundled unmodified at `drivers/GameInput/GameInputRedist.msi`, with its complete Microsoft license at `drivers/GameInput/LICENSE.txt`. These are separate terms, not the project's MIT license. Installed GameInput is loaded from Windows System32.

Package: https://www.nuget.org/packages/Microsoft.GameInput/3.5.270

## HidHide

Source and official installer: https://github.com/nefarius/HidHide/releases/tag/v1.5.230.0

The unmodified official HidHide 1.5.230 x64 installer and MIT license are included at `drivers/HidHide/`. Copyright (c) 2020 Eric Korff de Gidts; Copyright (c) 2021-2023 Benjamin Höglinger-Stelzer. Preserve the exact original license file supplied with the installer. The bridge invokes the installed official CLI and restores only its owned hiding changes.

## Microsoft Visual C++ Runtime

Official installer: https://aka.ms/vs/17/release/vc_redist.x64.exe

The unmodified official x64 redistributable is included at `drivers/VCRuntime/vc_redist.x64.exe`. It retains Microsoft's separate license terms, shown by its own full installer UI when installation is required. This redistributable is not licensed under the project's MIT license.

## Flydigi Space Station

Flydigi software and its transport driver are external proprietary prerequisites. Setup links to https://www.flydigi.com/ . No Flydigi installer, firmware image, private profile or decompiled vendor source is included in this application repository.

Installer provenance, hashes and signer pins are retained in `drivers/installers.json` and `drivers/WinUHid/package.json`. Only the newly generated project driver's public certificate is bundled; no private signing key or earlier research certificate is distributed.
