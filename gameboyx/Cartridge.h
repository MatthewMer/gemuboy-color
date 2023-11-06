#pragma once
/* ***********************************************************************************************************
	DESCRIPTION
*********************************************************************************************************** */
/*
*	The Cartridge source contains the class related to everythig with the actual ROM data of the dumped cartridge.
*	The VHardwareMgr creates at first an Instance of this class and reads the ROM dump to a vector based on the
*	information stored in the game_info Struct in game_info.h and reads basic information from it.
*	That's literally the only purpose of this class.
*/

/* ***********************************************************************************************************
	INCLUDES
*********************************************************************************************************** */
#include <vector>

#include "defs.h"
#include "game_info.h"

/* ***********************************************************************************************************
	CLASSES
*********************************************************************************************************** */
class Cartridge
{
public:
	// singleton instance access
	static Cartridge* getInstance(const game_info& _game_ctx);
	static Cartridge* getInstance();
	static void resetInstance();

	// clone/assign protection
	Cartridge(Cartridge const&) = delete;
	Cartridge(Cartridge&&) = delete;
	Cartridge& operator=(Cartridge const&) = delete;
	Cartridge& operator=(Cartridge&&) = delete;

	// static members
	static bool read_basic_header_info(game_info& _game_info, std::vector<u8>& _vec_rom);
	static bool read_rom_to_buffer(const game_info& _game_info, std::vector<u8>& _vec_rom);
	static bool copy_rom_to_rom_folder(game_info& _game_info, std::vector<u8>& _vec_rom, const std::string& _new_file_path);
	static bool read_new_game(game_info& _game_ctx, const std::string& _path_to_rom);
	static void check_and_create_rom_folder();

	// getter
	constexpr const game_info& GetGameInfo() const {
		return gameCtx;
	}

	constexpr const std::vector<u8>& GetRomVector() const {
		return vecRom;
	}

	constexpr const bool& GetIsCgb() const {
		return isCgb;
	}

private:
	// constructor
	static Cartridge* instance;
	explicit Cartridge(const game_info& _game_ctx) : gameCtx(_game_ctx) {
		ReadData();
	};
	// destructor
	~Cartridge() = default;

	// basic game context and read buffer
	std::vector<u8> vecRom = std::vector<u8>();
	const game_info& gameCtx;

	bool isCgb = false;

	// member functions
	bool ReadData();
	bool ReadHeaderConsole();
};