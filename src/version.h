#pragma once
// Single source of the RetroAmp version (used by the code, the .rc VERSIONINFO and the installer).
// Bump on every release: patch +1 each new version (1.0.1, 1.0.2, ...) - rule set by Rafał 2026-10-04.
#define RETROAMP_VER_MAJOR 1
#define RETROAMP_VER_MINOR 0
#define RETROAMP_VER_PATCH 4

#define RETROAMP_STR2(x) #x
#define RETROAMP_STR(x) RETROAMP_STR2(x)
#define RETROAMP_VERSION_A RETROAMP_STR(RETROAMP_VER_MAJOR) "." RETROAMP_STR(RETROAMP_VER_MINOR) "." RETROAMP_STR(RETROAMP_VER_PATCH)
#define RETROAMP_WIDEN2(x) L##x
#define RETROAMP_WIDEN(x) RETROAMP_WIDEN2(x)
#define RETROAMP_VERSION_W RETROAMP_WIDEN(RETROAMP_VERSION_A)
