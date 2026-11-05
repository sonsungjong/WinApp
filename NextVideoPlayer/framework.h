#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <objbase.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dwrite_3.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d3d11.lib")
