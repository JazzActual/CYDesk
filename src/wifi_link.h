// Joining the saved home network, including a mesh (several radios, one name).
//
// A mesh shows up as one network name with a different radio address on every
// node. The ESP32 remembers the first radio it joined. After that node drops
// the board — a move to another room, or the mesh steering the client — the
// radio keeps calling only that one address and never comes back. Setup
// "fixes" it only because picking the network again forgets the address.
//
// This asks for any radio with the saved name, and tries again if the link
// stays down. It never opens the setup portal by itself.
#pragma once

// Station mode, no power-save, scan every channel and prefer the strongest node.
// Call once at boot, before the portal. It changes the Wi-Fi mode.
void prepareWifi();

// Same scan and power-save settings, without changing Wi-Fi mode.
// The setup portal turns power-save back on, so call this when it closes.
void holdWifiPolicy();

// Connect to the saved network. Safe to call again while already trying.
void connectSavedWifi();

// Call from loop(). If the link has been down for a while, try any node again.
void maintainWifi();

// True once the link has changed and then stayed quiet for a moment.
// The screen should be redrawn: a Wi-Fi scan can flip the display mirror bit,
// and the page may have been painted with the wrong font while the link moved.
bool takeSettledWifiChange();
