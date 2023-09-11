#pragma once

#include "Cartridge.h"
#include "CoreBase.h"
#include "GraphicsUnitBase.h"

#include "SDL.h"


class VHardwareMgr
{
public:
	static VHardwareMgr* getInstance(const game_info& _game_ctx);
	static void resetInstance();

	// clone/assign protection
	VHardwareMgr(VHardwareMgr const&) = delete;
	VHardwareMgr(VHardwareMgr&&) = delete;
	VHardwareMgr& operator=(VHardwareMgr const&) = delete;
	VHardwareMgr& operator=(VHardwareMgr&&) = delete;

	// members for running hardware
	void ProcessNext();

	// SDL
	void KeyDown(const SDL_Keycode& _key);
	void KeyUp(const SDL_Keycode& _key);

private:
	// constructor
	explicit VHardwareMgr(const game_info& _game_ctx);
	static VHardwareMgr* instance;
	~VHardwareMgr() = default;

	// hardware instances
	CoreBase* core_instance;
	GraphicsUnitBase* graphics_instance;
	Cartridge* cart_instance;
};

