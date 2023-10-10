#include "ImGuiGameboyX.h"

/* ***********************************************************************************************************
    INCLUDES
*********************************************************************************************************** */
#include "Cartridge.h"
#include "imgui.h"
#include "data_io.h"
#include "nfd.h"
#include "logger.h"
#include "helper_functions.h"
#include "imguigameboyx_config.h"
#include "io_config.h"
#include <format>
#include <cmath>

using namespace std;

/* ***********************************************************************************************************
    PROTOTYPES
*********************************************************************************************************** */
static void create_fs_hierarchy();

/* ***********************************************************************************************************
    IMGUIGAMEBOYX FUNCTIONS
*********************************************************************************************************** */
ImGuiGameboyX* ImGuiGameboyX::instance = nullptr;

ImGuiGameboyX* ImGuiGameboyX::getInstance(machine_information& _machine_info, game_status& _game_status) {
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }

    instance = new ImGuiGameboyX(_machine_info, _game_status);
    return instance;
}

void ImGuiGameboyX::resetInstance() {
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }
    instance = nullptr;
}

ImGuiGameboyX::ImGuiGameboyX(machine_information& _machine_info, game_status& _game_status) : machineInfo(_machine_info), gameState(_game_status) {
    NFD_Init();
    create_fs_hierarchy();
    if (read_games_from_config(this->games, CONFIG_FOLDER + GAMES_CONFIG_FILE)) {
        InitGamesGuiCtx();
    }
}

/* ***********************************************************************************************************
    GUI PROCESS ENTRY POINT
*********************************************************************************************************** */
void ImGuiGameboyX::ProcessGUI() {
    IM_ASSERT(ImGui::GetCurrentContext() != nullptr && "Missing dear imgui context. Refer to examples app!");

    if (showMainMenuBar) { ShowMainMenuBar(); }
    if (machineInfo.instruction_debug_enabled) { ShowDebugInstructions(); }
    if (showWinAbout) { ShowWindowAbout(); }
    if (machineInfo.track_hardware_info) { ShowHardwareInfo(); }
    if (showMemoryInspector) { ShowDebugMemoryInspector(); }

    if (gameState.game_running) {
        if (machineInfo.instruction_debug_enabled) {
            if (machineInfo.instruction_logging) { WriteInstructionLog(); }
        }
    }
    else {
        // gui elements
        if (showGameSelect) { ShowGameSelect(); }
        if (showNewGameDialog) { ShowNewGameDialog(); }

        // actions
        if (deleteGames) { ActionDeleteGames(); }
        if (gameState.pending_game_start) { ActionStartGame(gameSelectedIndex); }

        // special functions
        ProcessMainMenuKeys();
    }
}

/* ***********************************************************************************************************
    GUI BUILDER
*********************************************************************************************************** */
void ImGuiGameboyX::ShowMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {

        if (ImGui::BeginMenu("Games")) {
            if (gameState.game_running) {
                ImGui::MenuItem("Stop game", nullptr, &gameState.pending_game_stop);
            }
            else {
                ImGui::MenuItem("Add new game", nullptr, &showNewGameDialog);
                ImGui::MenuItem("Remove game(s)", nullptr, &deleteGames);
                ImGui::MenuItem("Start game", nullptr, &gameState.pending_game_start);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings")) {
            ImGui::MenuItem("Show menu bar", nullptr, &showMainMenuBar);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Control")) {

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Graphics")) {

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("Hardware Info", nullptr, &machineInfo.track_hardware_info);
            ImGui::MenuItem("Debug Instructions", nullptr, &machineInfo.instruction_debug_enabled);
            ImGui::MenuItem("Memory Inspector", nullptr, &showMemoryInspector);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("About", nullptr, &showWinAbout);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void ImGuiGameboyX::ShowWindowAbout() {
    const ImGuiWindowFlags win_flags =
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("About", &showWinAbout, win_flags)) {
        ImGui::Text("GameboyX Emulator");
        ImGui::Text("Version: %s %d.%d", GBX_RELEASE, GBX_VERSION_MAJOR, GBX_VERSION_MINOR, GBX_VERSION_PATCH);
        ImGui::Text("Author: %s", GBX_AUTHOR);
        ImGui::Spacing();
    }
    ImGui::End();
}

void ImGuiGameboyX::ShowDebugInstructions() {
    const ImGuiWindowFlags win_flags =
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_AlwaysAutoResize;

    ImGui::SetNextWindowSize(debug_instr_win_size);

    if (ImGui::Begin("Debug Instructions", &machineInfo.instruction_debug_enabled, win_flags)) {
        if (gameState.game_running) {
            DebugCheckScroll(machineInfo.program_buffer);
            CurrentPCAutoScroll();
        }

        if (gameState.game_running && !debugAutoRun) {
            if (ImGui::Button("Next Instruction")) {
                machineInfo.pause_execution = CheckBreakPoint();
            }
        }
        else {
            ImGui::BeginDisabled();
            ImGui::Button("Next Instruction");
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (gameState.game_running) {
            if (ImGui::Button("Jump to PC")) { ActionScrollToCurrentPC(); }
            ImGui::SameLine();
            if (ImGui::Button("Reset")) { ActionRequestReset(); }
        }
        else {
            ImGui::BeginDisabled();
            ImGui::Button("Jump to PC");
            ImGui::SameLine();
            ImGui::Button("Reset");
            ImGui::EndDisabled();
        }

        ImGui::Checkbox("Auto run", &debugAutoRun);
        if (debugAutoRun) {
            machineInfo.pause_execution = CheckBreakPoint();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Send to *_instructions.log", &machineInfo.instruction_logging);

        const ImGuiTableFlags table_flags =
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_BordersOuterH;

        const ImGuiTableColumnFlags column_flags =
            ImGuiTableColumnFlags_NoHeaderLabel |
            ImGuiTableColumnFlags_NoResize |
            ImGuiTableColumnFlags_WidthFixed;

        const int column_num_instr = DEBUG_INSTR_COLUMNS.size();

        ImGuiStyle& style = ImGui::GetStyle();

        ImGui::Separator();
        ImGui::TextColored(style.Colors[ImGuiCol_TabActive], "Instructions:");
        if (ImGui::BeginTable("instruction_debug", column_num_instr, table_flags)) {

            for (int i = 0; i < column_num_instr; i++) {
                ImGui::TableSetupColumn("", column_flags, DEBUG_INSTR_COLUMNS[i] * (debug_instr_win_size.x - (style.WindowPadding.x * 2) - (style.FramePadding.x * 2)));
            }

            if (gameState.game_running) {

                // print instructions
                const ImGuiSelectableFlags sel_flags = ImGuiSelectableFlags_SpanAllColumns;

                debug_instr_data current_entry;
                while (machineInfo.program_buffer.GetNextEntry(current_entry)) {
                    
                    ImGui::TableNextColumn();

                    if (debugCurrentBreakpointSet) {
                        if (machineInfo.program_buffer.CompareIndex(debugCurrentBreakpoint)) {
                            ImGui::TextColored(style.Colors[ImGuiCol_HeaderHovered], ">>>");
                        }
                    }
                    ImGui::TableNextColumn();

                    if (machineInfo.program_buffer.CompareIndex(debugInstrIndex)) {
                        ImGui::Selectable(current_entry.first.c_str(), true, sel_flags);
                    }
                    else {
                        ImGui::Selectable(current_entry.first.c_str(), false, sel_flags);
                    }

                    // process actions
                    if (ImGui::IsItemHovered()) {
                        if (ImGui::IsMouseClicked(0)) { SetBreakPoint(machineInfo.program_buffer.GetCurrentIndex()); }
                    }
                    ImGui::TableNextColumn();

                    ImGui::TextUnformatted(current_entry.second.c_str());
                    ImGui::TableNextRow();
                }
            }
            else {
                for (int i = 0; i < DEBUG_INSTR_ELEMENTS; i++) {
                    ImGui::TableNextColumn();

                    ImGui::TableNextColumn();

                    ImGui::TextUnformatted("");
                    ImGui::TableNextColumn();

                    ImGui::TextUnformatted("");
                    ImGui::TableNextRow();
                }
            }
        }
        ImGui::EndTable();

        if (gameState.game_running) {
            if (ImGui::InputInt("ROM Bank", &debugBankToSearch)) {
                ActionBankSwitch();
            }
            const ImGuiInputTextFlags bank_addr_flags =
                ImGuiInputTextFlags_CharsHexadecimal |
                ImGuiInputTextFlags_CharsUppercase |
                ImGuiInputTextFlags_EnterReturnsTrue;
            if (ImGui::InputInt("ROM Address", &debugAddrToSearch, 1, 100, bank_addr_flags)) {
                ActionDebugInstrJumpToAddr();
            }
        }
        else {
            ImGui::BeginDisabled();
            ImGui::InputInt("ROM Bank", &debugBankToSearch);
            ImGui::InputInt("ROM Address", &debugAddrToSearch, 1, 100, 0);
            ImGui::EndDisabled();
        }


        const int column_num_registers = DEBUG_REGISTER_COLUMNS.size();

        ImGui::Separator();
        ImGui::TextColored(style.Colors[ImGuiCol_TabActive], "Registers:");
        if (!machineInfo.register_values.empty()) {
            if (ImGui::BeginTable("registers_debug", column_num_registers, table_flags)) {
                for (int i = 0; i < column_num_registers; i++) {
                    ImGui::TableSetupColumn("", column_flags, DEBUG_REGISTER_COLUMNS[i] * (debug_instr_win_size.x - (style.WindowPadding.x * 2) - (style.FramePadding.x * 2)));
                }

                ImGui::TableNextColumn();
                for (int i = 0; const auto & n : machineInfo.register_values) {
                    ImGui::TextUnformatted(n.first.c_str());
                    ImGui::TableNextColumn();

                    ImGui::TextUnformatted(n.second.c_str());

                    if (i++ % 2) { ImGui::TableNextRow(); }
                    ImGui::TableNextColumn();
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void ImGuiGameboyX::ShowDebugMemoryInspector() {
    const ImGuiWindowFlags win_flags =
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_AlwaysAutoResize;

    const ImGuiTableFlags table_flags =
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_BordersInnerH |
        ImGuiTableFlags_BordersOuterH |
        ImGuiTableFlags_NoPadOuterX;

    const ImGuiTableColumnFlags column_flags =
        ImGuiTableColumnFlags_NoResize |
        ImGuiTableColumnFlags_WidthFixed;

    ImGui::SetNextWindowSize(debug_mem_win_size);

    if (ImGui::Begin("Memory Inspector", &showMemoryInspector, win_flags)) {
        if (ImGui::BeginTabBar("memory_inspector_tabs", 0)) {

            if (gameState.game_running) {
                int tab_index = 0;
                int tab_index_prev = -1;

                ImGuiStyle& style = ImGui::GetStyle();

                for (int i = 0; const auto & [type, num, size, base_ptr, ref] : machineInfo.debug_memory) {

                    tab_index = type.first;

                    if (ImGui::BeginTabItem(type.second.c_str())) {

                        if (num > 1) {
                            ImGui::InputInt("Bank", &debugMemoryIndex[i].first);
                        }
                        else {
                            ImGui::BeginDisabled();
                            ImGui::InputInt("Bank", &debugMemoryIndex[i].first);
                            ImGui::EndDisabled();
                        }

                        if (ImGui::BeginTabBar("memory_inspector_tabs_sub", 0)) {
                            int column_num_mem = DEBUG_MEM_COLUMNS.size();
                            float table_width = ((float)debug_mem_win_size.x - (style.WindowPadding.x * 2) - (style.FramePadding.x * 2));

                            if (ImGui::BeginTable("memory_bank_table", column_num_mem, table_flags)) {
                                for (const auto& [n, m] : DEBUG_MEM_COLUMNS) {
                                    ImGui::TableSetupColumn(n.c_str(), column_flags, m * table_width);
                                }
                                ImGui::TableHeadersRow();

                                for (int j = debugMemoryIndex[i].second[j].first.y; j < debugMemoryIndex[i].second[j].second.y; j++) {

                                }

                                ImGui::EndTable();
                            }

                            ImGui::EndTabBar();
                        }
                        ImGui::EndTabItem();
                    }

                    tab_index_prev = tab_index;
                    i++;
                }
            }
            else {
                if (ImGui::BeginTabItem("NONE")) {
                    ImGui::BeginDisabled();
                    int i = 0;
                    ImGui::InputInt("Bank", &i);
                    ImGui::EndDisabled();

                    ImGui::EndTabItem();
                }
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void ImGuiGameboyX::ShowHardwareInfo() {
    const ImGuiWindowFlags win_flags =
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_AlwaysAutoResize;

    ImGui::SetNextWindowSize(hw_info_win_size);

    if (ImGui::Begin("Hardware Info", &machineInfo.track_hardware_info, win_flags)) {

        static const ImGuiTableColumnFlags column_flags =
            ImGuiTableColumnFlags_NoHeaderLabel |
            ImGuiTableColumnFlags_NoResize;

        static const ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV;

        if (ImGui::BeginTable("hardware_info", 2, table_flags)) {
            ImGuiStyle& style = ImGui::GetStyle();

            static const int column_num = HW_INFO_COLUMNS.size();

            for (int i = 0; i < column_num; i++) {
                ImGui::TableSetupColumn("", column_flags, HW_INFO_COLUMNS[i] * (hw_info_win_size.x - (style.WindowPadding.x * 2) - (style.FramePadding.x * 2)));
            }

            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 3, 3 });

            ImGui::TableNextColumn();
            ImGui::TextUnformatted("CPU Clock");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted((to_string(machineInfo.current_frequency) + " MHz").c_str());
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted("ROM Banks");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(to_string(machineInfo.rom_bank_num).c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("ROM Selected");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(to_string(machineInfo.rom_bank_selected).c_str());
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted("RAM Banks");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(to_string(machineInfo.ram_bank_num).c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("RAM Selected");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(machineInfo.ram_bank_selected < machineInfo.ram_bank_num ? to_string(machineInfo.ram_bank_selected).c_str() : "-");
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted("RTC Register");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(machineInfo.ram_bank_selected >= machineInfo.ram_bank_num ? to_string(machineInfo.ram_bank_selected).c_str() : "-");
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted("WRAM Banks");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(to_string(machineInfo.wram_bank_num).c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("WRAM Selected");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(to_string(machineInfo.wram_bank_selected).c_str());

            ImGui::PopStyleVar();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

void ImGuiGameboyX::ShowNewGameDialog() {
    check_and_create_config_folders();

    string current_path = get_current_path();
    string s_path_rom_folder = current_path + ROM_FOLDER;

    auto filter_items = new nfdfilteritem_t[sizeof(nfdfilteritem_t) * FILE_EXTS.size()];
    for (int i = 0; i < FILE_EXTS.size(); i++) {
        filter_items[i] = { FILE_EXTS[i][0].c_str(), FILE_EXTS[i][1].c_str() };
    }

    nfdchar_t* out_path = nullptr;
    const auto result = NFD_OpenDialog(&out_path, filter_items, 2, s_path_rom_folder.c_str());
    delete[] filter_items;

    if (result == NFD_OKAY) {
        if (out_path != nullptr) {
            string path_to_rom(out_path);
            if (!ActionAddGame(path_to_rom)) { return; }
        }
        else {
            LOG_ERROR("No path (nullptr)");
            return;
        }
    }
    else if (result != NFD_CANCEL) {
        LOG_ERROR("Couldn't open file dialog");
    }

    showNewGameDialog = false;
}

void ImGuiGameboyX::ShowGameSelect() {
    const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x, main_viewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(main_viewport->Size.x, main_viewport->Size.y));

    const ImGuiWindowFlags win_flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IMGUI_BG_COLOR);
    if (ImGui::Begin("games_select", nullptr, win_flags)) {

        static const ImGuiTableFlags table_flags =
            ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_BordersInnerV;

        static const int column_num = GAMES_COLUMNS.size();

        static const ImGuiTableColumnFlags col_flags = ImGuiTableColumnFlags_WidthFixed;

        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 10, 10 });

        if (ImGui::BeginTable("game_list", column_num, table_flags)) {

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, style.CellPadding.y * 2));

            for (int i = 0; const auto & [label, fraction] : GAMES_COLUMNS) {
                ImGui::TableSetupColumn(label.c_str(), col_flags, main_viewport->Size.x * fraction);
                ImGui::TableSetupScrollFreeze(i, 0);
                i++;
            }
            ImGui::TableHeadersRow();

            for (int i = 0; const auto & game : games) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                // TODO: icon
                ImGui::TableNextColumn();

                const ImGuiSelectableFlags sel_flags = ImGuiSelectableFlags_SpanAllColumns;
                ImGui::Selectable((game.is_cgb ? GAMES_CONSOLES[1].c_str() : GAMES_CONSOLES[0].c_str()), gamesSelected[i], sel_flags);

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    for (int j = 0; j < gamesSelected.size(); j++) {
                        gamesSelected[i] = i == j;
                    }

                    ActionStartGame(i);
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
                    if (sdlkShiftDown) {
                        static int lower, upper;
                        if (i <= gameSelectedIndex) {
                            lower = i;
                            upper = gameSelectedIndex;
                        }
                        else {
                            lower = gameSelectedIndex;
                            upper = i;
                        }

                        for (int j = 0; j < gamesSelected.size(); j++) {
                            gamesSelected[j] = ((j >= lower) && (j <= upper)) || (sdlkCtrlDown ? gamesSelected[j] : false);
                        }
                    }
                    else {
                        for (int j = 0; j < gamesSelected.size(); j++) {
                            gamesSelected[j] = (i == j ? !gamesSelected[j] : (sdlkCtrlDown ? gamesSelected[j] : false));
                        }
                        gameSelectedIndex = i;
                        if (!sdlkCtrlDown) {
                            gamesSelected[i] = true;
                        }
                    }
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(1)) {
                    // TODO: context menu
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(game.title.c_str());
                ImGui::SetItemTooltip(game.title.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(game.file_path.c_str());
                ImGui::SetItemTooltip(game.file_path.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(game.file_name.c_str());
                ImGui::SetItemTooltip(game.file_name.c_str());

                i++;
            }
            ImGui::PopStyleVar();
        }
        ImGui::PopStyleVar();
        ImGui::EndTable();
        ImGui::End();
    }
    ImGui::PopStyleColor();
}

/* ***********************************************************************************************************
    ACTIONS TO READ/WRITE/PROCESS DATA ETC.
*********************************************************************************************************** */

// ***** ADD/REMOVE GAMES *****
bool ImGuiGameboyX::ActionAddGame(const string& _path_to_rom) {
    auto game_ctx = game_info();

    if (!Cartridge::read_new_game(game_ctx, _path_to_rom)) {
        showNewGameDialog = false;
        return false;
    }

    for (const auto& n : games) {
        if (game_ctx == n) {
            LOG_WARN("Game already added ! Process aborted");
            showNewGameDialog = false;
            return false;
        }
    }

    if (const vector<game_info> new_game = { game_ctx }; !write_games_to_config(new_game, CONFIG_FOLDER + GAMES_CONFIG_FILE, false)) {
        showNewGameDialog = false;
        return false;
    }

    AddGameGuiCtx(game_ctx);
    return true;
}

void ImGuiGameboyX::ActionDeleteGames() {
    auto index = vector<int>();
    for (int i = 0; const auto & n : gamesSelected) {
        if (n) index.push_back(i);
        i++;
    }

    auto games_delete = DeleteGamesGuiCtx(index);

    if (games_delete.size() > 0) {
        delete_games_from_config(games_delete, CONFIG_FOLDER + GAMES_CONFIG_FILE);
    }

    InitGamesGuiCtx();

    deleteGames = false;
}

// ***** START/END GAME CONTROL *****
void ImGuiGameboyX::ActionStartGame(int _index) {
    gameState.pending_game_start = true;
    gameState.game_to_start = _index;
    firstInstruction = true;
}

void ImGuiGameboyX::ActionEndGame() {
    if (gameState.game_running) {
        for (int i = 0; i < gamesSelected.size(); i++) {
            gamesSelected[i] = false;
        }
        gamesSelected[gameState.game_to_start] = true;
    }
    gameState.pending_game_stop = false;

    // reset debug windows
    ResetDebugInstr();
    ResetMemInspector();
}

void ImGuiGameboyX::ActionRequestReset() {
    gameState.request_reset = true;
    debugLastProgramCounter = -1;
}

// ***** ACTIONS FOR DEBUG INSTRUCTION WINDOW *****

void ImGuiGameboyX::ActionBankSwitch() {
    machineInfo.program_buffer.SwitchBank(debugBankToSearch);
}

void ImGuiGameboyX::ActionDebugInstrJumpToAddr() {
    machineInfo.program_buffer.SearchAddress(debugAddrToSearch);
}

void ImGuiGameboyX::ActionScrollToCurrentPC() {
    //machineInfo.program_buffer.ScrollToAddress(debugInstrIndex.x, )
}

// ***** ACTIONS FOR MEMORY INSPECTOR WINDOW *****

/* ***********************************************************************************************************
    HELPER FUNCTIONS FOR MANAGING MEMBERS OF GUI OBJECT
*********************************************************************************************************** */

// ***** MANAGE GAME ENTRIES OF GUI OBJECT *****
void ImGuiGameboyX::AddGameGuiCtx(const game_info& _game_ctx) {
    games.push_back(_game_ctx);
    gamesSelected.clear();
    for (const auto& game : games) {
        gamesSelected.push_back(false);
    }
    gamesSelected.back() = true;
    gameSelectedIndex = gamesSelected.size() - 1;
}

vector<game_info> ImGuiGameboyX::DeleteGamesGuiCtx(const vector<int>& _index) {
    auto result = vector<game_info>();

    for (int i = _index.size() - 1; i >= 0; i--) {
        const game_info game_ctx = games[_index[i]];
        result.push_back(game_ctx);
        games.erase(games.begin() + _index[i]);
        gamesSelected.erase(gamesSelected.begin() + _index[i]);
    }

    if (result.size() > 0) {
        gameSelectedIndex = 0;
    }

    return result;
}

void ImGuiGameboyX::InitGamesGuiCtx() {
    for (const auto& n : games) {
        gamesSelected.push_back(false);
    }
}

// ***** UNIVERSIAL CHECKER FOR SCROLL ACTION RELATED TO ANY WINDOW WITH CUSTOM SCROLLABLE TABLE *****
void ImGuiGameboyX::DebugCheckScroll(ScrollableTableBase& _table_obj) {
    if ((ImGui::IsWindowHovered() || ImGui::IsWindowFocused())) {
        if (sdlScrollUp) {
            sdlScrollUp = false;
            if (sdlkShiftDown) { _table_obj.ScrollUpPage(); }
            else { _table_obj.ScrollUp(1); }
        }
        else if (sdlScrollDown) {
            sdlScrollDown = false;
            if (sdlkShiftDown) { _table_obj.ScrollDownPage(); }
            else { _table_obj.ScrollDown(1); }
        }
    }
}

// ***** HELPERS FOR MANAGING MEMBERS REPLATED TO DEBUG INSTRUCTION WINDOW *****
void ImGuiGameboyX::CurrentPCAutoScroll() {
    if (machineInfo.current_pc != debugLastProgramCounter) {
        debugLastProgramCounter = machineInfo.current_pc;
        debugInstrIndex.x = machineInfo.current_rom_bank;

        ActionScrollToCurrentPC();
    }
}

// ***** DEBUG INSTRUCTION PROCESS BREAKPOINT *****
bool ImGuiGameboyX::CheckBreakPoint() {
    if (debugCurrentBreakpointSet) {
        return debugInstrIndex == debugCurrentBreakpoint;
    }
    else {
        return false;
    }
}

void ImGuiGameboyX::SetBreakPoint(const Vec2& _current_index) {
    if (debugCurrentBreakpointSet) {
        if (debugCurrentBreakpoint == _current_index) {
            debugCurrentBreakpointSet = false;
        }
        else {
            debugCurrentBreakpoint = _current_index;
        }
    }
    else {
        debugCurrentBreakpoint = _current_index;
        debugCurrentBreakpointSet = true;
    }
}

/* ***********************************************************************************************************
    SET/RESET AT GAME START/STOP
*********************************************************************************************************** */
// ***** SETUP/RESET FUNCTIONS FOR DEBUG WINDOWS *****
void ImGuiGameboyX::ResetDebugInstr() {
    debugInstrIndex = Vec2(0, 0);
    //debugScrollStartIndex = Vec2(0, 0);
    //debugScrollEndIndex = Vec2(0, 0);
    debugAddrToSearch = 0;
    debugCurrentBreakpoint = Vec2(0, 0);
    debugCurrentBreakpointSet = false;
}

void ImGuiGameboyX::ResetMemInspector() {
    debugMemoryIndex.clear();
}

void ImGuiGameboyX::SetupMemInspectorIndex() {
    debugMemoryIndex.clear();

    int prev_type = -1;
    for (const auto& [type, num, size, base_ptr, ref] : machineInfo.debug_memory) {
        debugMemoryIndex.emplace_back(pair(0, std::vector<std::pair<Vec2, Vec2>>()));

        if (type.first == prev_type) { debugMemoryIndex.back().first = 1; }
        else { debugMemoryIndex.back().first = 0; }

        if (size >= DEBUG_MEMORY_LINES * DEBUG_MEMORY_ELEM_PER_LINE) {
            for (int i = 0; i < num; i++) {
                debugMemoryIndex.back().second.emplace_back(Vec2(0, 0), Vec2(0, DEBUG_MEMORY_LINES));
            }
        }
        else {
            for (int i = 0; i < num; i++) {
                debugMemoryIndex.back().second.emplace_back(Vec2(0, 0), Vec2(0, ceil((float)size / DEBUG_MEMORY_ELEM_PER_LINE)));
            }
        }

        prev_type = type.first;
    }
}

/* ***********************************************************************************************************
    LOG DATA IO
*********************************************************************************************************** */
// ***** WRITE EXECUTED INSTRUCTIONS + RESULTS TO LOG FILE *****
void ImGuiGameboyX::WriteInstructionLog() {
    /*
    if (machineInfo.current_instruction.compare("") != 0) {
        string output = "Executed: " + machineInfo.current_instruction + "\n" +
            "-> Debug GUI:" +
            get<2>(machineInfo.program_buffer[debugInstrIndex.x][debugInstrIndex.y]) + ": " +
            get<3>(machineInfo.program_buffer[debugInstrIndex.x][debugInstrIndex.y]);
        write_to_debug_log(output, LOG_FOLDER + games[gameSelectedIndex].title + DEBUG_INSTR_LOG, firstInstruction);
        firstInstruction = false;

        machineInfo.current_instruction = "";
    }
    */
}

/* ***********************************************************************************************************
    MAIN DIRECT COMMUNICATION
*********************************************************************************************************** */
game_info& ImGuiGameboyX::GetGameStartContext() {
    //game_running = true;
    //pending_game_start = false;
    return games[gameState.game_to_start];
}

void ImGuiGameboyX::GameStopped() {
    ActionEndGame();
}

void ImGuiGameboyX::GameStartCallback() {
    ResetDebugInstr();
    ResetMemInspector();

    SetupMemInspectorIndex();
}

/* ***********************************************************************************************************
    IMGUIGAMEBOYX SDL FUNCTIONS
*********************************************************************************************************** */
void ImGuiGameboyX::EventKeyDown(const SDL_Keycode& _key) {
    switch (_key) {
    case SDLK_a:
        sdlkADown = true;
        break;
    case SDLK_LSHIFT:
        sdlkShiftDown = true;
        break;
    case SDLK_LCTRL:
    case SDLK_RCTRL:
        sdlkCtrlDown = true;
        sdlkCtrlDownFirst = !sdlkADown;
        break;
    case SDLK_DELETE:
        sdlkDelDown = true;
        break;
    default:
        break;
    }
}

void ImGuiGameboyX::EventKeyUp(const SDL_Keycode& _key) {
    switch (_key) {
    case SDLK_a:
        sdlkADown = false;
        break;
    case SDLK_F10:
        showMainMenuBar = !showMainMenuBar;
        break;
    case SDLK_LSHIFT:
        sdlkShiftDown = false;
        break;
    case SDLK_LCTRL:
    case SDLK_RCTRL:
        sdlkCtrlDown = false;
        break;
    case SDLK_DELETE:
        sdlkDelDown = false;
        break;
    default:
        break;
    }
}

void ImGuiGameboyX::EventMouseWheel(const Sint32& _wheel_y) {
    if (_wheel_y > 0) {
        sdlScrollUp = true;
    }
    else if (_wheel_y < 0) {
        sdlScrollDown = true;
    }
}

void ImGuiGameboyX::ProcessMainMenuKeys() {
    if (sdlkCtrlDown) {
        if (sdlkADown && sdlkCtrlDownFirst) {
            for (int i = 0; i < gamesSelected.size(); i++) {
                gamesSelected[i] = true;
            }
        }
    }
    if (sdlkDelDown) {
        ActionDeleteGames();
        sdlkDelDown = false;
    }
}

/* ***********************************************************************************************************
    HELPER FUNCTIONS
*********************************************************************************************************** */
static void create_fs_hierarchy() {
    check_and_create_config_folders();
    check_and_create_config_files();
    check_and_create_log_folders();
    Cartridge::check_and_create_rom_folder();
}