#include "HardwareMgr.h"

HardwareMgr* HardwareMgr::instance = nullptr;
u32 HardwareMgr::errors = 0x00000000;
GraphicsMgr* HardwareMgr::graphicsMgr = nullptr;
AudioMgr* HardwareMgr::audioMgr = nullptr;
SDL_Window* HardwareMgr::window = nullptr;

graphics_information HardwareMgr::graphicsInfo = graphics_information();
audio_information HardwareMgr::audioInfo = audio_information();

std::queue<std::pair<SDL_Keycode, SDL_EventType>> HardwareMgr::keyMap = std::queue<std::pair<SDL_Keycode, SDL_EventType>>();
Sint32 HardwareMgr::mouseScroll = 0;

u8 HardwareMgr::InitHardware() {
	errors = 0;

	if (instance == nullptr) {
		instance = new HardwareMgr();
	}else{
		errors |= HWMGR_ERR_ALREADY_RUNNING;
		return errors;
	}


	// sdl
	window = nullptr;
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
		LOG_ERROR("[SDL]", SDL_GetError());
		return false;
	} else {
		LOG_INFO("[SDL] initialized");
	}

	// graphics
	graphicsMgr = GraphicsMgr::getInstance(&window);
	if (graphicsMgr != nullptr) {
		if (!graphicsMgr->InitGraphics()) { return false; }
		if (!graphicsMgr->StartGraphics()) { return false; }

		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		if (!graphicsMgr->InitImgui()) { return false; }

		graphicsMgr->EnumerateShaders();
	}

	SDL_SetWindowMinimumSize(window, GUI_WIN_WIDTH_MIN, GUI_WIN_HEIGHT_MIN);

	// audio
	audioMgr = AudioMgr::getInstance();
	if (audioMgr != nullptr) {
		audioMgr->InitAudio(false);
	}

	return true;
}

void HardwareMgr::ShutdownHardware() {
	// graphics
	graphicsMgr->DestroyImgui();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	graphicsMgr->StopGraphics();
	graphicsMgr->ExitGraphics();

	SDL_DestroyWindow(window);
	SDL_Quit();
}

void HardwareMgr::NextFrame() {
	u32 win_min = SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED;

	if (!win_min) {
		graphicsMgr->NextFrameImGui();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
	}
}

void HardwareMgr::RenderFrame() {
	graphicsMgr->RenderFrame();
}

void HardwareMgr::ProcessInput(bool& _running) {
	SDL_Event event;
	auto& key_map = keyMap;
	auto& mouse_scroll = mouseScroll;

	while (SDL_PollEvent(&event)) {
		ImGui_ImplSDL2_ProcessEvent(&event);
		if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
			_running = false;

		switch (event.type) {
		case SDL_QUIT:
			_running = false;
			break;
		case SDL_KEYDOWN:
			key_map.push(std::pair(event.key.keysym.sym, SDL_KEYDOWN));
			break;
		case SDL_KEYUP:
			key_map.push(std::pair(event.key.keysym.sym, SDL_KEYUP));
			break;
		case SDL_MOUSEWHEEL:
			mouse_scroll = event.wheel.y;
			break;
		default:
			mouse_scroll = 0;
			break;
		}
	}
}

std::queue<std::pair<SDL_Keycode, SDL_EventType>>& HardwareMgr::GetKeys() {
	return keyMap;
}

Sint32& HardwareMgr::GetScroll() {
	Sint32 scroll = mouseScroll;
	mouseScroll = 0;
	return scroll;
}

void HardwareMgr::ToggleFullscreen() {
	if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) {
		SDL_SetWindowFullscreen(window, 0);
	} else {
		SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
	}
}

void HardwareMgr::InitGraphicsBackend(virtual_graphics_information& _virt_graphics_info) {
	graphicsMgr->InitGraphicsBackend(_virt_graphics_info);
}

void HardwareMgr::InitAudioBackend(virtual_audio_information& _virt_audio_info) {
	audioMgr->InitAudioBackend(_virt_audio_info);
}

void HardwareMgr::DestroyGraphicsBackend() {
	graphicsMgr->DestroyGraphicsBackend();
}

void HardwareMgr::DestroyAudioBackend() {
	audioMgr->DestroyAudioBackend();
}

void HardwareMgr::UpdateGpuData() {
	graphicsMgr->UpdateGpuData();
}