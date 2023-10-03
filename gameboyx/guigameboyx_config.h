#pragma once

#include "imgui.h"
#include <vector>
#include <string>

/* ***********************************************************************************************************
    GBX DEFINES
*********************************************************************************************************** */
#define _DEBUG_GBX

#define GBX_RELEASE					"PRE-ALPHA"
#define GBX_VERSION_MAJOR			0
#define GBX_VERSION_MINOR			0
#define GBX_VERSION_PATCH           0
#define GBX_AUTHOR					"MatthewMer"

/* ***********************************************************************************************************
    IMGUI GBX CONSTANTS/DEFINES
*********************************************************************************************************** */
#define GUI_WIN_WIDTH               1280
#define GUI_WIN_HEIGHT              720

inline const std::string APP_TITLE = "GameboyX";

inline const std::vector<std::pair<std::string, float>> GAMES_COLUMNS = { 
    {"", 1 / 12.f},
    {"Console", 1 / 12.f},
    {"Title", 1 / 6.f},
    {"File path", 1 / 3.f},
    {"File name", 4 / 12.f},
};

inline const ImVec2 debug_win_size(500.f, 0.f);
inline const std::vector<float> DEBUG_INSTR_COLUMNS = {
    1 / 15.f,
    7 / 15.f,
    7 / 15.f
};
inline const std::vector<float> DEBUG_REGISTER_COLUMNS = {
    1 / 16.f,
    7 / 16.f,
    1 / 16.f,
    7 / 16.f
};

inline const ImVec2 hw_info_win_size(250.f, 0.f);
inline const std::vector<float> HW_INFO_COLUMNS = {
    1 / 2.f,
    1 / 2.f
};

inline const std::vector<std::string> GAMES_CONSOLES = { "Gameboy", "Gameboy Color" };

#define BG_COL 0.1f
#define CLR_COL 0.0f
inline const ImVec4 IMGUI_BG_COLOR(BG_COL, BG_COL, BG_COL, 1.0f);
inline const ImVec4 IMGUI_CLR_COLOR(CLR_COL, CLR_COL, CLR_COL, 1.0f);

//inline const std::vector<std::string> popup_items = {"Test1", "Test2"};

#define DEBUG_ALLOWED_INSTRUCTION_OUTPUT_SIZE 20