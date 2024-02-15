#pragma once

#include "defs.h"
#include "GraphicsMgr.h"
#include "AudioMgr.h"
#include <imgui.h>
#include <SDL.h>
#include "logger.h"
#include "HardwareStructs.h"


#include <queue>

#define HWMGR_ERR_ALREADY_RUNNING		0x00000001

class HardwareMgr {
public:
	static u8 InitHardware();
	static void ShutdownHardware();
	static void NextFrame();
	static void RenderFrame();
	static void ProcessInput(bool& _running);
	static void ToggleFullscreen();

	static void InitGraphicsBackend(virtual_graphics_information& _virt_graphics_info);
	static void InitAudioBackend(virtual_audio_information& _virt_audio_info);
	static void DestroyGraphicsBackend();
	static void DestroyAudioBackend();

	static void UpdateGpuData();

	static std::queue<std::pair<SDL_Keycode, SDL_EventType>>& GetKeys();
	static Sint32 GetScroll();

private:
	HardwareMgr() = default;
	~HardwareMgr() = default;

	static GraphicsMgr* graphicsMgr;
	static AudioMgr* audioMgr;
	static SDL_Window* window;

	static graphics_information graphicsInfo;
	static audio_information audioInfo;

	// control
	static std::queue<std::pair<SDL_Keycode, SDL_EventType>> keyMap;
	static Sint32 mouseScroll;

	static HardwareMgr* instance;
	static u32 errors;
};