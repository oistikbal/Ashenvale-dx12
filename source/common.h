#pragma once

#define __STDC_LIMIT_MACROS

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <cfloat>
#include <cstdint>
#include <assert.h>

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <winrt/base.h>

#if _DEBUG
#define SET_DX_NAME(object, name) object->SetName(name);
#else
#define SET_DX_NAME(object, name) ((void)0);
#endif