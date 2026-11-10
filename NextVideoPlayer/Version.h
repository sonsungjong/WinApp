#pragma once

// Shared by executable resources and Packaging/Build-Packages.ps1.
// Increment the first three fields for every MSI release.
// Store requires a nonzero major version and revision 0.
#define NVP_VERSION_MAJOR 1
#define NVP_VERSION_MINOR 0
#define NVP_VERSION_PATCH 0
#define NVP_VERSION_REVISION 0

#define NVP_STRINGIFY_IMPL(value) #value
#define NVP_STRINGIFY(value) NVP_STRINGIFY_IMPL(value)
#define NVP_VERSION_STRING NVP_STRINGIFY(NVP_VERSION_MAJOR) "." \
    NVP_STRINGIFY(NVP_VERSION_MINOR) "." NVP_STRINGIFY(NVP_VERSION_PATCH) "." \
    NVP_STRINGIFY(NVP_VERSION_REVISION)
