#include "BaseCPU.h"
#include "GameboyCPU.h"

#include "logger.h"

#include "general_config.h"

BaseCPU* BaseCPU::instance = nullptr;

BaseCPU* BaseCPU::getInstance(machine_information& _machine_info, graphics_information& _graphics_info, GraphicsMgr* _graphics_mgr, audio_information& _audio_info, AudioMgr* _audio_mgr) {
	resetInstance();

	instance = new GameboyCPU(_machine_info, _graphics_info, _graphics_mgr, _audio_info, _audio_mgr);

	return instance;
}

void BaseCPU::resetInstance() {
	if (instance != nullptr) {
		BaseMMU::resetInstance();
		BaseGPU::resetInstance();
		delete instance;
		instance = nullptr;
	}
}