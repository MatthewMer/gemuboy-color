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
#include "guigameboyx_config.h"
#include "io_config.h"

using namespace std;

/* ***********************************************************************************************************
    PROTOTYPES
*********************************************************************************************************** */
static void create_fs_hierarchy();

/* ***********************************************************************************************************
    IMGUIGAMEBOYX FUNCTIONS
*********************************************************************************************************** */
ImGuiGameboyX* ImGuiGameboyX::instance = nullptr;

ImGuiGameboyX* ImGuiGameboyX::getInstance(message_buffer& _msg_buffer, game_status& _game_status) {
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }

    instance = new ImGuiGameboyX(_msg_buffer, _game_status);
    return instance;
}

void ImGuiGameboyX::resetInstance() {
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }
    instance = nullptr;
}

ImGuiGameboyX::ImGuiGameboyX(message_buffer& _msg_buffer, game_status& _game_status) : msgBuffer(_msg_buffer), gameStatus(_game_status){
    NFD_Init();
    create_fs_hierarchy();
    if (read_games_from_config(this->games, CONFIG_FOLDER + GAMES_CONFIG_FILE)) {
        InitGamesGuiCtx();
    }

    InitDebugOutputVectors();
}

/* ***********************************************************************************************************
    GUI PROCESS ENTRY POINT
*********************************************************************************************************** */
void ImGuiGameboyX::ProcessGUI() {
    IM_ASSERT(ImGui::GetCurrentContext() != nullptr && "Missing dear imgui context. Refer to examples app!");

    if (showMainMenuBar) ShowMainMenuBar();
    if (msgBuffer.instruction_buffer_enabled) ShowDebugInstructions();
    if (showWinAbout) ShowWindowAbout();
    if (msgBuffer.track_hardware_info) ShowHardwareInfo();

    if (!gameStatus.game_running) {
        // gui elements
        if (showGameSelect) ShowGameSelect();
        if (showNewGameDialog) ShowNewGameDialog();
        
        // actions
        if (deleteGames) ActionDeleteGames();
        if (gameStatus.pending_game_start) {
            ClearOutput();
            ActionStartGame(gamesPrevIndex);
        }

        // special funktions
        ActionProcessSpecialKeys();
    }
}

/* ***********************************************************************************************************
    GUI BUILDER
*********************************************************************************************************** */
void ImGuiGameboyX::ShowMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        
        if (ImGui::BeginMenu("Games")) {
            if (gameStatus.game_running) {
                ImGui::MenuItem("Stop game", nullptr, &gameStatus.pending_game_stop);
            }
            else {
                ImGui::MenuItem("Add new game", nullptr, &showNewGameDialog);
                ImGui::MenuItem("Remove game(s)", nullptr, &deleteGames);
                ImGui::MenuItem("Start game", nullptr, &gameStatus.pending_game_start);
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
            ImGui::MenuItem("Instruction Execution", nullptr, &msgBuffer.instruction_buffer_enabled);
            ImGui::MenuItem("Hardware Info", nullptr, &msgBuffer.track_hardware_info);
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
    CopyInstructionBuffer();

    const ImGuiWindowFlags win_flags =
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse;

    ImGui::SetNextWindowSize({ 700, 396 });
    if (ImGui::Begin("Instruction execution", &msgBuffer.instruction_buffer_enabled, win_flags)) {
        for (int i = 0; i < DEBUG_ALLOWED_INSTRUCTION_OUTPUT_SIZE; i++) {
            ImGui::TextUnformatted(instructionOutput[i].c_str());
        }

        if (gameStatus.game_running && !msgBuffer.auto_run) {
            if (ImGui::Button("Next Instruction")) {
                msgBuffer.pause_execution = false;
            }
        }
        else {
            ImGui::BeginDisabled();
            ImGui::Button("Next Instruction");
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto run", &msgBuffer.auto_run);
        ImGui::SameLine();
        ImGui::Checkbox("Send to *_instructions.log", &msgBuffer.instruction_buffer_log);
    }
    ImGui::End();
}

void ImGuiGameboyX::ShowHardwareInfo() {
    const ImGuiWindowFlags win_flags =
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse;

    ImGui::SetNextWindowSize({ 300, 300 });

    if (ImGui::Begin("Hardware Info", &msgBuffer.track_hardware_info, win_flags)) {
        ImGui::TextUnformatted("Base Clock:");
        ImGui::TextUnformatted((to_string(msgBuffer.current_frequency) + " MHz").c_str());

        ImGui::TextUnformatted("ROM Banks:");
        ImGui::TextUnformatted(to_string(msgBuffer.rom_bank_num).c_str());
        ImGui::TextUnformatted("Selected:");
        ImGui::TextUnformatted(to_string(msgBuffer.rom_bank_selected).c_str());

        ImGui::TextUnformatted("RAM Banks:");
        ImGui::TextUnformatted(to_string(msgBuffer.ram_bank_num).c_str());
        ImGui::TextUnformatted("Selected:");
        ImGui::TextUnformatted(to_string(msgBuffer.ram_bank_selected).c_str());

        ImGui::TextUnformatted("WRAM Banks:");
        ImGui::TextUnformatted(to_string(msgBuffer.wram_bank_num).c_str());
        ImGui::TextUnformatted("Selected:");
        ImGui::TextUnformatted(to_string(msgBuffer.wram_bank_selected).c_str());
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

                ImGuiSelectableFlags sel_flags = ImGuiSelectableFlags_SpanAllColumns;
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
                        if (i <= gamesPrevIndex) {
                            lower = i;
                            upper = gamesPrevIndex;
                        }
                        else {
                            lower = gamesPrevIndex;
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
                        gamesPrevIndex = i;
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

void ImGuiGameboyX::ActionProcessSpecialKeys() {
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

void ImGuiGameboyX::ActionStartGame(int _index) {
    gameStatus.pending_game_start = true;
    gameStatus.game_to_start = _index;
    firstInstruction = true;
}

void ImGuiGameboyX::ActionEndGame() {
    if (gameStatus.game_running) {
        for (int i = 0; i < gamesSelected.size(); i++) {
            gamesSelected[i] = false;
        }
        gamesSelected[gameStatus.game_to_start] = true;
    }
    gameStatus.pending_game_stop = false;
}

/* ***********************************************************************************************************
    HELPER FUNCTIONS FOR MANAGING MEMBERS OF GUI OBJECT
*********************************************************************************************************** */
void ImGuiGameboyX::AddGameGuiCtx(const game_info& _game_ctx) {
    games.push_back(_game_ctx);
    gamesSelected.clear();
    for (const auto& game : games) {
        gamesSelected.push_back(false);
    }
    gamesSelected.back() = true;
    gamesPrevIndex = gamesSelected.size() - 1;
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
        gamesPrevIndex = 0;
    }

    return result;
}

void ImGuiGameboyX::InitGamesGuiCtx() {
    for (const auto& n : games) {
        gamesSelected.push_back(false);
    }
}

void ImGuiGameboyX::CopyInstructionBuffer() {
    if (msgBuffer.instruction_buffer.compare("") != 0) {
        instructionOutput.push_back(msgBuffer.instruction_buffer);
        if (msgBuffer.instruction_buffer_log) {
            write_to_debug_log(msgBuffer.instruction_buffer, LOG_FOLDER + games[gamesPrevIndex].title + DEBUG_INSTR_LOG, firstInstruction);
            firstInstruction = false;
        }
        msgBuffer.instruction_buffer = "";
    }

    while (instructionOutput.size() > DEBUG_ALLOWED_INSTRUCTION_OUTPUT_SIZE) {
        instructionOutput.erase(instructionOutput.begin());
    }
}

void ImGuiGameboyX::ClearOutput() {
    for (auto& n : instructionOutput) {
        n = "";
    }
}

void ImGuiGameboyX::InitDebugOutputVectors() {
    for (int i = 0; i < DEBUG_ALLOWED_INSTRUCTION_OUTPUT_SIZE; i++) {
        instructionOutput.emplace_back("");
    }
}

/* ***********************************************************************************************************
    COMMUNICATION WITH MAIN
*********************************************************************************************************** */
game_info& ImGuiGameboyX::GetGameStartContext() {
    //game_running = true;
    //pending_game_start = false;
    return games[gameStatus.game_to_start];
}

void ImGuiGameboyX::GameStopped() {
    ActionEndGame();
}

/* ***********************************************************************************************************
    IMGUIGAMEBOYX SDL FUNCTIONS
*********************************************************************************************************** */
void ImGuiGameboyX::KeyDown(const SDL_Keycode& _key) {
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

void ImGuiGameboyX::KeyUp(const SDL_Keycode& _key) {
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

/* ***********************************************************************************************************
    HELPER FUNCTIONS
*********************************************************************************************************** */
static void create_fs_hierarchy() {
    check_and_create_config_folders();
    check_and_create_config_files();
    check_and_create_log_folders();
    Cartridge::check_and_create_rom_folder();
}