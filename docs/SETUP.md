# Setup guide

## On another PC

1. Launch `FlydigiTriggerBridge.exe` and accept the administrator prompt. Open **Setup**; startup checks installed prerequisites without installing anything.
2. Use **Get Space Station** to obtain Flydigi's software from its official site. This proprietary installer is not bundled. The bridge verifies the transport driver's exact supported checksum and refuses other versions; installing the latest vendor version alone does not prove compatibility.
3. Use **Install** beside **Virtual controller driver** if missing. Read the confirmation and license, then select its unchecked authorization box. This installs the bundled WinUHid development driver and trusts its public signing certificate in this PC's Trusted Root and Trusted Publisher stores. These changes persist after the app exits. The official Visual C++ x64 installer opens first if its runtime is missing; accept its own terms to continue.
4. Use **Install** beside **Microsoft GameInput** and **HidHide** for missing components. Microsoft GameInput has separate license terms shown before installation. HidHide is a persistent Windows HID filter; installation does not enable hiding or change device rules. Existing working installations are kept.
5. Complete any installer windows. Reconnect the dongle or restart Windows if requested. The app does not automatically restart Windows. Click **Check setup** before starting the bridge.

WinUHid, GameInput, HidHide and the Visual C++ installers are inside the EXE, with verified hashes and signatures. **Verify installer package** checks them without installing anything. The WinUHid certificate is a project development certificate, not WHQL certification. Only its public certificate ships; the temporary signing key was deleted. No Secure Boot, test-signing or boot-security bypass is performed. Clean-PC first installation and newly installed driver removal still require validation.

On the development PC all prerequisites are already working. Setup keeps them; rebuilding the application does not reinstall them or change certificate trust.

## Removal

**Remove virtual driver** is available only when this app has an installation receipt. It removes its exact recorded device, package and certificate trust additions. An unrelated or changed driver is refused. Keep the application version that installed the driver: a later driver rebuild generates a different certificate and will not remove a mismatched package. HidHide, GameInput and the Flydigi transport driver remain managed by their own Windows uninstallers.

Installation is separate from a gameplay session. Start/Stop only temporarily changes the controller's working RAM profile and physical-device visibility; installed prerequisites remain installed.

## Before playing

Choose the intended active profile in Space Station, enable Trigger Vibration Switch, and use a profile without macros. The bridge reads the active profile and does not select another slot. Close games and Space Station before the first virtual-channel check. Turn the controller on through the dongle. Start the bridge, wait for Ready to play, then open a game. Four-channel output requires game support. Half-Life Legacy uses Steam Input for impulse triggers.

## Stopping and recovery

Stop & restore sends zero motor output, restores the exact original 840-byte profile, verifies it again after a delay, restores the original controller visibility, and returns the service to its previous state. The app waits for this sequence before exiting.

If the controller is off at stop, reconnect the same controller and use Restore profile. The original backup is retained locally; keep Space Station closed until restoration is complete. Another controller or a changed profile is refused.

## Diagnostics

Activity shows session progress and errors. Open log folder opens local session logs. Export diagnostics exports a summary; original profiles and device addresses are excluded. Detailed diagnostic logs can be enabled for the next session.
