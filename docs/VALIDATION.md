# Validation

## Established transport behavior

On the development Vader 4 Pro (model105, firmware6.9.5.9), four independent motors were confirmed with released triggers. Constant100% requests sustained three-second output for both grips and both triggers while only packet byte14 alternated. The user confirmed each motor separately. This was a synthetic GameInput test, not a motor sensor measurement.

Half-Life Legacy previously sent nonzero grip/LT/RT commands through the virtual controller and dongle bridge. The game test preceded removal of the initial amplitude reduction; its game compatibility evidence is separate from the later full-amplitude physical test.

## Application build checks

Version 0.1.2 renames the product to Flydigi Trigger Bridge, including the GUI, tray/dialog titles, native test-window titles, assembly/namespace, solution/project, executable, documentation and release archives. The native log revision identifies this release. Transport and recovery behavior are unchanged; compatibility identifiers and the entire signed driver payload retain their prior bytes/values. Existing profile backups and installation receipts continue to use the same data folder and ownership checks. A changed native checksum requires the normal first-run four-channel API validation; no validation receipt is carried across different engine binaries.

Version 0.1.1 adds bundled prerequisite installers and explicit Setup installation dialogs. Offline checks verify original package signatures, reject a modified DLL even when the fixture manifest hash is updated, refuse installation without explicit intent, and confirm no private key is bundled. Read-only elevated checks verified the embedded payload and exercised all three existing-component skip paths. No driver or certificate was installed during these checks. Clean-PC first installation and the removal path remain untested; existing-component checks do not prove them.

The controller preview uses a symmetric shell with integrated shoulder panels. Buttons, sticks, D-pad and motor highlights are clipped to that shell. Dashboard and Setup previews were rendered without hardware access.

`build.ps1` runs native mapping/reconnect self-tests and C# application workflow checks. It does not access physical devices or install drivers. App checks cover exact display values, clearing stale output while offline, keeping cleanup nonterminal until all restoration steps finish, retaining recovery state, incremental journal reads, and command-line escaping.

`--render` draws a demo window without hardware/service access. `--smoke-test --data-root <folder>` performs installed-prerequisite checks only, with no physical writes.

The live application lifecycle check passed on 8 October 2026: its actual Start and Stop buttons launched an unbounded bridge session, reached the ready state, then requested restoration after five seconds. The original 840-byte profile was restored and verified, controller visibility and the service were restored, and no pending recovery remained. This passive check did not repeat the earlier physical motor test.

First-run virtual validation exposed a hidden-window startup issue, corrected by explicitly showing the intentional validation window. It waits for activation and performs up to three complete zero/request attempts per channel, requiring the exact callback before authorizing physical setup. Windows may still require clicking that window. A disabled active profile was refused before a RAM patch; selecting the intended enabled profile resolved it.

The source ZIP was extracted into a separate folder and successfully rebuilt with its offline checks. The pinned SDK header was supplied from the verified local cache for that build. The Visual Studio solution and hosted GitHub Actions run have not been separately exercised.

The GitHub workflow builds Windows x64 artifacts and runs offline checks. It cannot validate attached hardware. Public clean-PC installation, sleep by the controller's timer, arbitrary radio faults and compatibility with additional games remain separate validation cases.
