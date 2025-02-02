#include "GameboyMEM.h"

#include "gameboy_defines.h"
#include "logger.h"
#include "format"
#include "GameboyGPU.h"
#include "GameboyCPU.h"
#include "BaseAPU.h"

#include <iostream>

using namespace std;

namespace Emulation {
    namespace Gameboy {
        /* ***********************************************************************************************************
            DEFINES
        *********************************************************************************************************** */
        #define LINE_NUM_HEX(x)     (((float)x / DEBUG_MEM_ELEM_PER_LINE) + ((float)(DEBUG_MEM_ELEM_PER_LINE - 1) / DEBUG_MEM_ELEM_PER_LINE))

        /* ***********************************************************************************************************
            CONSTRUCTOR
        *********************************************************************************************************** */
        GameboyMEM::GameboyMEM(std::shared_ptr<BaseCartridge> _cartridge) {
            machineCtx.battery_buffered = _cartridge->batteryBuffered;
            machineCtx.ram_present = _cartridge->ramPresent;
            machineCtx.timer_present = _cartridge->timerPresent;

            if (machineCtx.is_cgb) {
                graphics_ctx.dmg_bgp_color_palette[0] = CGB_DMG_COLOR_WHITE;
                graphics_ctx.dmg_bgp_color_palette[1] = CGB_DMG_COLOR_LIGHTGREY;
                graphics_ctx.dmg_bgp_color_palette[2] = CGB_DMG_COLOR_DARKGREY;
                graphics_ctx.dmg_bgp_color_palette[3] = CGB_DMG_COLOR_BLACK;
            } else {
                graphics_ctx.dmg_bgp_color_palette[0] = DMG_COLOR_WHITE_ALT;
                graphics_ctx.dmg_bgp_color_palette[1] = DMG_COLOR_LIGHTGREY_ALT;
                graphics_ctx.dmg_bgp_color_palette[2] = DMG_COLOR_DARKGREY_ALT;
                graphics_ctx.dmg_bgp_color_palette[3] = DMG_COLOR_BLACK_ALT;
                graphics_ctx.dmg_obp0_color_palette[0] = DMG_COLOR_WHITE_ALT;
                graphics_ctx.dmg_obp0_color_palette[1] = DMG_COLOR_LIGHTGREY_ALT;
                graphics_ctx.dmg_obp0_color_palette[2] = DMG_COLOR_DARKGREY_ALT;
                graphics_ctx.dmg_obp0_color_palette[3] = DMG_COLOR_BLACK_ALT;
                graphics_ctx.dmg_obp1_color_palette[0] = DMG_COLOR_WHITE_ALT;
                graphics_ctx.dmg_obp1_color_palette[1] = DMG_COLOR_LIGHTGREY_ALT;
                graphics_ctx.dmg_obp1_color_palette[2] = DMG_COLOR_DARKGREY_ALT;
                graphics_ctx.dmg_obp1_color_palette[3] = DMG_COLOR_BLACK_ALT;
            }

            InitMemory(_cartridge);

            GenerateMemoryTables();
        };

        GameboyMEM::~GameboyMEM() {
            if (!machineCtx.battery_buffered && machineCtx.ram_present) {
                for (auto& n : RAM_N) {
                    delete[] n;
                }
            }
        }

        void GameboyMEM::Init() {}

        /* ***********************************************************************************************************
            HARDWARE ACCESS
        *********************************************************************************************************** */
        machine_context* GameboyMEM::GetMachineContext() {
            return &machineCtx;
        }

        graphics_context* GameboyMEM::GetGraphicsContext() {
            return &graphics_ctx;
        }

        sound_context* GameboyMEM::GetSoundContext() {
            return &sound_ctx;
        }

        control_context* GameboyMEM::GetControlContext() {
            return &control_ctx;
        }

        void GameboyMEM::RequestInterrupts(const u8& _isr_flags) {
            IO[IF_ADDR - IO_OFFSET] |= _isr_flags;
        }

        /* ***********************************************************************************************************
            INITIALIZE MEMORY
        *********************************************************************************************************** */
        void GameboyMEM::InitMemory(std::shared_ptr<BaseCartridge> _cartridge) {
            romData = _cartridge->GetRom();

            machineCtx.is_cgb = romData[ROM_HEAD_CGBFLAG] & 0x80 ? true : false;
            machineCtx.cgb_compatibility = _cartridge->CheckCompatibilityMode();

            machineCtx.wram_bank_num = (machineCtx.is_cgb || machineCtx.cgb_compatibility ? 8 : 2);
            machineCtx.vram_bank_num = (machineCtx.is_cgb || machineCtx.cgb_compatibility ? 2 : 1);

            if (!ReadRomHeaderInfo(romData)) {
                LOG_ERROR("Couldn't acquire memory information");
                return;
            }

            AllocateMemory(_cartridge);

            if (_cartridge->CheckBootRom()) {
                if (_cartridge->ReadBootRom() && InitBootRom(_cartridge->GetBootRom(), romData)) {
                    LOG_INFO("[emu] boot ROM initialized");
                } else {
                    LOG_ERROR("[emu] Init boot ROM");
                }
                _cartridge->ClearBootRom();
            } else {
                if (!InitRom(romData)) {
                    LOG_ERROR("[emu] Init ROM");
                } else {
                    LOG_INFO("[emu] ROM initialized");
                    InitMemoryState();
                }
            }
        }

        bool GameboyMEM::InitRom(const vector<u8>& _vec_rom) {
            vector<u8>::const_iterator start = _vec_rom.begin() + ROM_0_OFFSET;
            vector<u8>::const_iterator end = _vec_rom.begin() + ROM_0_SIZE;
            ROM_0 = vector<u8>(start, end);

            for (int i = 0; i < machineCtx.rom_bank_num - 1; i++) {
                start = _vec_rom.begin() + ROM_N_OFFSET + i * ROM_N_SIZE;
                end = _vec_rom.begin() + ROM_N_OFFSET + i * ROM_N_SIZE + ROM_N_SIZE;

                ROM_N[i] = vector<u8>(start, end);
            }

            machineCtx.boot_rom_mapped = false;
            if (machineCtx.cgb_compatibility) {
                std::dynamic_pointer_cast<GameboyGPU>(BaseGPU::s_GetInstance())->SetHardwareMode(GB);
            }
            return true;
        }

        bool GameboyMEM::InitBootRom(const std::vector<u8>& _boot_rom, const std::vector<u8>& _vec_rom) {
            size_t boot_rom_size = _boot_rom.size();

            vector<u8>::const_iterator start = _boot_rom.begin();
            vector<u8>::const_iterator end = _boot_rom.end();
            std::copy(start, end, ROM_0.begin());

            start = _vec_rom.begin() + ROM_HEAD_ADDR;
            end = start + ROM_HEAD_SIZE;
            std::copy(start, end, ROM_0.begin() + ROM_HEAD_ADDR);

            machineCtx.boot_rom_mapped = true;
            return true;
        }

        /* ***********************************************************************************************************
            MANAGE ALLOCATED MEMORY
        *********************************************************************************************************** */
        void GameboyMEM::AllocateMemory(std::shared_ptr<BaseCartridge> _cartridge) {
            ROM_0 = vector<u8>(ROM_0_SIZE, 0);
            ROM_N = vector<vector<u8>>(machineCtx.rom_bank_num - 1);
            for (int i = 0; i < machineCtx.rom_bank_num - 1; i++) {
                ROM_N[i] = vector<u8>(ROM_N_SIZE, 0);
            }

            graphics_ctx.VRAM_N = vector<vector<u8>>(machineCtx.vram_bank_num);
            for (int i = 0; i < machineCtx.vram_bank_num; i++) {
                graphics_ctx.VRAM_N[i] = vector<u8>(VRAM_N_SIZE, 0);
            }

            if (machineCtx.ram_present && machineCtx.ram_bank_num > 0) {
                if (machineCtx.battery_buffered) {
                    auto file_name = Helpers::split_string(_cartridge->fileName, ".");
                    saveFile = Config::SAVE_FOLDER;

                    size_t j = file_name.size() - 1;
                    for (size_t i = 0; i < j; i++) {
                        saveFile += file_name[i];
                        if (i < j - 1) { saveFile += "."; }
                    }
                    saveFile += Config::SAVE_EXT;

                    u8* data = (u8*)mapper.GetMappedFile(saveFile.c_str(), machineCtx.ram_bank_num * RAM_N_SIZE);

                    RAM_N.clear();
                    for (int i = 0; i < machineCtx.ram_bank_num; i++) {
                        RAM_N.emplace_back(data + i * RAM_N_SIZE);

                        /* only for testing the mapping
                        for (int j = 0; j < RAM_N_SIZE / 0x10; j++) {
                            std::string s = "";
                            for (int k = 0; k < 0x10; k++) {
                                s += std::format("{:02x}", RAM_N[i][j * 0x10 + k]) + " ";
                            }
                            LOG_WARN(s);
                        }
                        */
                    }
                } else {
                    RAM_N = vector<u8*>(machineCtx.ram_bank_num);
                    for (int i = 0; i < machineCtx.ram_bank_num; i++) {
                        RAM_N[i] = new u8[RAM_N_SIZE];
                    }
                }
            }

            WRAM_0 = vector<u8>(WRAM_0_SIZE, 0);
            WRAM_N = vector<vector<u8>>(machineCtx.wram_bank_num - 1);
            for (int i = 0; i < machineCtx.wram_bank_num - 1; i++) {
                WRAM_N[i] = vector<u8>(WRAM_N_SIZE, 0);
            }

            graphics_ctx.OAM = vector<u8>(OAM_SIZE, 0);

            IO = vector<u8>(IO_SIZE, 0);

            HRAM = vector<u8>(HRAM_SIZE, 0);
        }

        /* ***********************************************************************************************************
            INIT MEMORY STATE
        *********************************************************************************************************** */
        void GameboyMEM::InitMemoryState() {   
            machineCtx.IE = INIT_CGB_IE;
            IO[IF_ADDR - IO_OFFSET] = INIT_CGB_IF;

            IO[BGP_ADDR - IO_OFFSET] = INIT_BGP;
            IO[OBP0_ADDR - IO_OFFSET] = INIT_OBP0;
            IO[OBP1_ADDR - IO_OFFSET] = INIT_OBP1;

            SetVRAMBank(INIT_VRAM_BANK);
            SetWRAMBank(INIT_WRAM_BANK);

            IO[STAT_ADDR - IO_OFFSET] = INIT_CGB_STAT;
            IO[LY_ADDR - IO_OFFSET] = INIT_CGB_LY;
            SetLCDCValues(INIT_CGB_LCDC);
            SetLCDSTATValues(INIT_CGB_STAT);    

            if (machineCtx.is_cgb) {
                IO[DIV_ADDR - IO_OFFSET] = INIT_CGB_DIV >> 8; 
                machineCtx.div_low_byte = INIT_CGB_DIV & 0xFF;
            }
            else { 
                IO[DIV_ADDR - IO_OFFSET] = INIT_DIV >> 8; 
                machineCtx.div_low_byte = INIT_DIV & 0xFF;
            }

            IO[TIMA_ADDR - IO_OFFSET] = INIT_TIMA;
            IO[TMA_ADDR - IO_OFFSET] = INIT_TMA;
            IO[TAC_ADDR - IO_OFFSET] = INIT_TAC;
            SetControlValues(0xFF);
            ProcessTAC();
        }

        /* ***********************************************************************************************************
            ROM HEADER DATA FOR MEMORY
        *********************************************************************************************************** */
        bool GameboyMEM::ReadRomHeaderInfo(const std::vector<u8>& _vec_rom) {
            if (_vec_rom.size() < ROM_HEAD_ADDR + ROM_HEAD_SIZE) { return false; }

            u8 value = _vec_rom[ROM_HEAD_ROMSIZE];

            if (value != 0x52 && value != 0x53 && value != 0x54) {                          // TODO: these values are used for special variations
                int total_rom_size = ROM_BASE_SIZE * (1 << _vec_rom[ROM_HEAD_ROMSIZE]);
                machineCtx.rom_bank_num = total_rom_size >> 14;
            }
            else {
                return false;
            }

            // get ram info
            value = _vec_rom[ROM_HEAD_RAMSIZE];
            switch (value) {
            case RAM_BANK_NUM_0_VAL:
                machineCtx.ram_bank_num = 0;
                break;
            case RAM_BANK_NUM_1_VAL:
                machineCtx.ram_bank_num = 1;
                break;
            case RAM_BANK_NUM_4_VAL:
                machineCtx.ram_bank_num = 4;
                break;
                // not allowed
            case RAM_BANK_NUM_8_VAL:
                machineCtx.ram_bank_num = 8;
                return false;
                break;
                // not allowed
            case RAM_BANK_NUM_16_VAL:
                machineCtx.ram_bank_num = 16;
                return false;
                break;
            default:
                return false;
                break;
            }

            return true;
        }

        /* ***********************************************************************************************************
            MEMORY ACCESS
        *********************************************************************************************************** */
        // read *****
        u8 GameboyMEM::ReadROM_0(const u16& _addr) {
            return ROM_0[_addr];
        }

        // overload for MBC1
        u8 GameboyMEM::ReadROM_0(const u16& _addr, const int& _bank) {
            if (_bank == 0) { return ROM_0[_addr]; }
            else { return ROM_N[_bank - 1][_addr]; }
        }

        u8 GameboyMEM::ReadROM_N(const u16& _addr) {
            return ROM_N[machineCtx.rom_bank_selected][_addr - ROM_N_OFFSET];
        }

        u8 GameboyMEM::ReadVRAM_N(const u16& _addr) {
            if (graphics_ctx.ppu_enable && graphics_ctx.mode == PPU_MODE_3) {
                return 0xFF;
            } else {
                return graphics_ctx.VRAM_N[IO[CGB_VRAM_SELECT_ADDR - IO_OFFSET]][_addr - VRAM_N_OFFSET];
            }
        }

        u8 GameboyMEM::ReadRAM_N(const u16& _addr) {
            return RAM_N[machineCtx.ram_bank_selected][_addr - RAM_N_OFFSET];
        }

        u8 GameboyMEM::ReadWRAM_0(const u16& _addr) {
            return WRAM_0[_addr - WRAM_0_OFFSET];
        }

        u8 GameboyMEM::ReadWRAM_N(const u16& _addr) {
            return WRAM_N[machineCtx.wram_bank_selected][_addr - WRAM_N_OFFSET];
        }

        u8 GameboyMEM::ReadOAM(const u16& _addr) {
            if (graphics_ctx.ppu_enable && (graphics_ctx.mode == PPU_MODE_3 || graphics_ctx.mode == PPU_MODE_2)) {
                return 0xFF;
            } else {
                return graphics_ctx.OAM[_addr - OAM_OFFSET];
            }
        }

        u8 GameboyMEM::ReadIO(const u16& _addr) {
            return ReadIORegister(_addr);
        }

        u8 GameboyMEM::ReadHRAM(const u16& _addr) {
            return HRAM[_addr - HRAM_OFFSET];
        }

        u8 GameboyMEM::ReadIE() {
            return machineCtx.IE;
        }

        // write *****
        void GameboyMEM::WriteVRAM_N(const u8& _data, const u16& _addr) {
            if (graphics_ctx.ppu_enable && graphics_ctx.mode == PPU_MODE_3) {
                return;
            } else {
                graphics_ctx.VRAM_N[machineCtx.vram_bank_selected][_addr - VRAM_N_OFFSET] = _data;
            }
        }

        void GameboyMEM::WriteRAM_N(const u8& _data, const u16& _addr) {
            RAM_N[machineCtx.ram_bank_selected][_addr - RAM_N_OFFSET] = _data;
        }

        void GameboyMEM::WriteWRAM_0(const u8& _data, const u16& _addr) {
            WRAM_0[_addr - WRAM_0_OFFSET] = _data;
        }

        void GameboyMEM::WriteWRAM_N(const u8& _data, const u16& _addr) {
            WRAM_N[machineCtx.wram_bank_selected][_addr - WRAM_N_OFFSET] = _data;
        }

        void GameboyMEM::WriteOAM(const u8& _data, const u16& _addr) {
            if (graphics_ctx.ppu_enable && (graphics_ctx.mode == PPU_MODE_3 || graphics_ctx.mode == PPU_MODE_2)) {
                return;
            } else {
                graphics_ctx.OAM[_addr - OAM_OFFSET] = _data;
            }
        }

        void GameboyMEM::WriteIO(const u8& _data, const u16& _addr) {
            WriteIORegister(_data, _addr);
        }

        void GameboyMEM::WriteHRAM(const u8& _data, const u16& _addr) {
            HRAM[_addr - HRAM_OFFSET] = _data;
        }

        void GameboyMEM::WriteIE(const u8& _data) {
            machineCtx.IE = _data;
        }

        /* ***********************************************************************************************************
            IO PROCESSING
        *********************************************************************************************************** */
        u8 GameboyMEM::ReadIORegister(const u16& _addr) {
            // TODO: take registers with mixed access into account and further reading on return values when reading from specific locations
            switch (_addr) {
            case 0xFF03:
            case 0xFF08:
            case 0xFF09:
            case 0xFF0A:
            case 0xFF0B:
            case 0xFF0C:
            case 0xFF0D:
            case 0xFF0E:
            case 0xFF15:
            case 0xFF1F:
            case 0xFF27:
            case 0xFF28:
            case 0xFF29:
            case 0xFF2A:
            case 0xFF2B:
            case 0xFF2C:
            case 0xFF2D:
            case 0xFF2E:
            case 0xFF2F:
            case 0xFF4C:
            case 0xFF4E:
            case 0xFF50:
            case 0xFF57:
            case 0xFF58:
            case 0xFF59:
            case 0xFF5A:
            case 0xFF5B:
            case 0xFF5C:
            case 0xFF5D:
            case 0xFF5E:
            case 0xFF5F:
            case 0xFF60:
            case 0xFF61:
            case 0xFF62:
            case 0xFF63:
            case 0xFF64:
            case 0xFF65:
            case 0xFF66:
            case 0xFF67:
            case 0xFF6D:
            case 0xFF6E:
            case 0xFF6F:
            case 0xFF71:
            case 0xFF72:
            case 0xFF73:
            case 0xFF74:
            case 0xFF75:
            case CGB_HDMA1_ADDR:
            case CGB_HDMA2_ADDR:
            case CGB_HDMA3_ADDR:
            case CGB_HDMA4_ADDR:
                return 0xFF;
                break;
            default:
                if (_addr > 0xFF77) {
                    return 0xFF;
                }
                else if(machineCtx.is_cgb){
                    return IO[_addr - IO_OFFSET];
                }
                else {
                    switch (_addr) {
                    case CGB_SPEED_SWITCH_ADDR:
                    case CGB_VRAM_SELECT_ADDR:
                    case CGB_HDMA5_ADDR:
                    case CGB_IR_ADDR:
                    case BCPS_BGPI_ADDR:
                    case BCPD_BGPD_ADDR:
                    case OCPS_OBPI_ADDR:
                    case OCPD_OBPD_ADDR:
                    case CGB_OBJ_PRIO_ADDR:
                    case CGB_WRAM_SELECT_ADDR:
                        return 0xFF;
                        break;
                    default:
                        return IO[_addr - IO_OFFSET];
                        break;
                    }
                }
            }
        }

        void GameboyMEM::WriteIORegister(const u8& _data, const u16& _addr) {
            switch (_addr) {
            case JOYP_ADDR:
                SetControlValues(_data);
                break;
            case DIV_ADDR:
                IO[DIV_ADDR - IO_OFFSET] = 0x00;
                machineCtx.div_low_byte = 0x00;
                break;
            case TAC_ADDR:
                IO[TAC_ADDR - IO_OFFSET] = _data;
                ProcessTAC();
                break;
            case TIMA_ADDR:
                if (!machineCtx.tima_reload_cycle) {
                    IO[TIMA_ADDR - IO_OFFSET] = _data;
                    machineCtx.tima_overflow_cycle = false;
                }
                break;
            case IF_ADDR:
                machineCtx.tima_reload_if_write = machineCtx.tima_reload_cycle;
                graphics_ctx.vblank_if_write = true;
                IO[IF_ADDR - IO_OFFSET] = _data;
                break;
            case CGB_VRAM_SELECT_ADDR:
                SetVRAMBank(_data);
                break;
            case CGB_SPEED_SWITCH_ADDR:
                SwitchSpeed(_data);
                break;
            case LCDC_ADDR:
                SetLCDCValues(_data);
                break;
            case STAT_ADDR:
                SetLCDSTATValues(_data);
                break;
            case OAM_DMA_ADDR:
                OAM_DMA(_data);
                break;
                // mode
            case CGB_HDMA5_ADDR:
                VRAM_DMA(_data);
                break;
            case CGB_OBJ_PRIO_ADDR:
                SetObjPrio(_data);
                break;
            case CGB_WRAM_SELECT_ADDR:
                SetWRAMBank(_data);
                break;
                // DMG only
            case BGP_ADDR:
                IO[BGP_ADDR - IO_OFFSET] = _data;
                SetColorPaletteValues(_data, graphics_ctx.dmg_bgp_color_palette, graphics_ctx.cgb_bgp_color_palettes[0]);
                break;
                // DMG only
            case OBP0_ADDR:
                IO[OBP0_ADDR - IO_OFFSET] = _data;
                SetColorPaletteValues(_data, graphics_ctx.dmg_obp0_color_palette, graphics_ctx.cgb_obp_color_palettes[0]);
                break;
            case OBP1_ADDR:
                // DMG only
                IO[OBP1_ADDR - IO_OFFSET] = _data;
                SetColorPaletteValues(_data, graphics_ctx.dmg_obp1_color_palette, graphics_ctx.cgb_obp_color_palettes[1]);
                break;
            case BANK_ADDR:
                if (machineCtx.boot_rom_mapped) {
                    UnmapBootRom();
                }
                break;
            case BCPD_BGPD_ADDR:
                SetBGWINPaletteValues(_data);
                break;
            case OCPD_OBPD_ADDR:
                SetOBJPaletteValues(_data);
                break;
            case BCPS_BGPI_ADDR:
                SetBCPS(_data);
                break;
            case OCPS_OBPI_ADDR:
                SetOCPS(_data);
                break;
            case LY_ADDR:
                break;
            case NR52_ADDR:
                SetAPUMasterControl(_data);
                break;
            case NR51_ADDR:
                SetAPUChannelPanning(_data);
                break;
            case NR50_ADDR:
                SetAPUMasterVolume(_data);
                break;
            case NR10_ADDR:
                SetAPUCh1Sweep(_data);
                break;
            case NR11_ADDR:
                SetAPUCh12TimerDutyCycle(_data, &sound_ctx.ch_ctxs[0]);
                break;
            case NR12_ADDR:
                SetAPUCh124Envelope(_data, &sound_ctx.ch_ctxs[0]);
                break;
            case NR13_ADDR:
                SetAPUCh12PeriodLow(_data, &sound_ctx.ch_ctxs[0]);
                break;
            case NR14_ADDR:
                SetAPUCh12PeriodHighControl(_data, &sound_ctx.ch_ctxs[0]);
                break;
            case NR21_ADDR:
                SetAPUCh12TimerDutyCycle(_data, &sound_ctx.ch_ctxs[1]);
                break;
            case NR22_ADDR:
                SetAPUCh124Envelope(_data, &sound_ctx.ch_ctxs[1]);
                break;
            case NR23_ADDR:
                SetAPUCh12PeriodLow(_data, &sound_ctx.ch_ctxs[1]);
                break;
            case NR24_ADDR:
                SetAPUCh12PeriodHighControl(_data, &sound_ctx.ch_ctxs[1]);
                break;
            case NR30_ADDR:
                SetAPUCh3DACEnable(_data);
                break;
            case NR31_ADDR:
                SetAPUCh3Timer(_data);
                break;
            case NR32_ADDR:
                SetAPUCh3Volume(_data);
                break;
            case NR33_ADDR:
                SetAPUCh3PeriodLow(_data);
                break;
            case NR34_ADDR:
                SetAPUCh3PeriodHighControl(_data);
                break;
            case NR41_ADDR:
                SetAPUCh4Timer(_data);
                break;
            case NR42_ADDR:
                SetAPUCh124Envelope(_data, &sound_ctx.ch_ctxs[3]);
                break;
            case NR43_ADDR:
                SetAPUCh4FrequRandomness(_data);
                break;
            case NR44_ADDR:
                SetAPUCh4Control(_data);
                break;
            default:
                // Wave RAM
                if (WAVE_RAM_ADDR - 1 < _addr && _addr < WAVE_RAM_ADDR + WAVE_RAM_SIZE) {
                    SetAPUCh3WaveRam(_addr, _data);
                } else {
                    IO[_addr - IO_OFFSET] = _data;
                }
                break;
            }
        }

        u8& GameboyMEM::GetIO(const u16& _addr) {
            return IO[_addr - IO_OFFSET];
        }

        void GameboyMEM::SetIO(const u16& _addr, const u8& _data) {
            switch (_addr) {
            case IF_ADDR:
                graphics_ctx.vblank_if_write = true;
                break;
            }

            IO[_addr - IO_OFFSET] = _data;
        }

        const u8* GameboyMEM::GetBank(const MEM_TYPE& _type, const int& _bank) {
            switch (_type) {
            case MEM_TYPE::ROM0:
                return ROM_0.data();
                break;
            case MEM_TYPE::ROMn:
                return ROM_N[_bank].data();
                break;
            case MEM_TYPE::RAMn:
                return RAM_N[_bank];
                break;
            case MEM_TYPE::WRAM0:
                return WRAM_0.data();
                break;
            case MEM_TYPE::WRAMn:
                return WRAM_N[_bank].data();
                break;
            default:
                LOG_ERROR("[emu] GetBank: memory area access not implemented");
                return nullptr;
                break;
            }
        }

        /* ***********************************************************************************************************
            DMA
        *********************************************************************************************************** */
        void GameboyMEM::VRAM_DMA(const u8& _data) {
            if (machineCtx.is_cgb) {
                if (graphics_ctx.vram_dma) {
                    //LOG_WARN("HDMA5 while DMA");
                    graphics_ctx.vram_dma = false;
                    if (!(_data & 0x80)) {
                        IO[CGB_HDMA5_ADDR - IO_OFFSET] = 0x80 | _data;
                        return;
                    }
                }

                IO[CGB_HDMA5_ADDR - IO_OFFSET] = _data & 0x7F;

                u16 source_addr = ((u16)IO[CGB_HDMA1_ADDR - IO_OFFSET] << 8) | (IO[CGB_HDMA2_ADDR - IO_OFFSET] & 0xF0);
                u16 dest_addr = (((u16)(IO[CGB_HDMA3_ADDR - IO_OFFSET] & 0x1F)) << 8) | (IO[CGB_HDMA4_ADDR - IO_OFFSET] & 0xF0);

                graphics_ctx.vram_dma_ppu_en = graphics_ctx.ppu_enable;

                auto m_CoreInstance = std::dynamic_pointer_cast<GameboyCPU>(BaseCPU::s_GetInstance());

                if (_data & 0x80) {
                    // HBLANK DMA
                    m_CoreInstance->TickTimers();

                    if (source_addr < ROM_N_OFFSET) {
                        graphics_ctx.vram_dma_src_addr = source_addr;
                        graphics_ctx.vram_dma_mem = MEM_TYPE::ROM0;
                    } else if (source_addr < VRAM_N_OFFSET) {
                        graphics_ctx.vram_dma_src_addr = source_addr - ROM_N_OFFSET;
                        graphics_ctx.vram_dma_mem = MEM_TYPE::ROMn;
                    } else if (source_addr < RAM_N_OFFSET) {
                        LOG_ERROR("[emu] HDMA source address ", std::format("{:04x}", source_addr), " undefined copy");
                        return;
                    } else if (source_addr < WRAM_0_OFFSET) {
                        graphics_ctx.vram_dma_src_addr = source_addr - RAM_N_OFFSET;
                        graphics_ctx.vram_dma_mem = MEM_TYPE::RAMn;
                    } else if (source_addr < WRAM_N_OFFSET) {
                        graphics_ctx.vram_dma_src_addr = source_addr - WRAM_0_OFFSET;
                        graphics_ctx.vram_dma_mem = MEM_TYPE::WRAM0;
                    } else if (source_addr < MIRROR_WRAM_OFFSET) {
                        graphics_ctx.vram_dma_src_addr = source_addr - WRAM_N_OFFSET;
                        graphics_ctx.vram_dma_mem = MEM_TYPE::WRAMn;
                    } else {
                        graphics_ctx.vram_dma_src_addr = source_addr - (RAM_N_OFFSET + 0x4000);
                        graphics_ctx.vram_dma_mem = MEM_TYPE::RAMn;
                    }

                    graphics_ctx.vram_dma_dst_addr = dest_addr;
                    graphics_ctx.vram_dma = true;

                    if (graphics_ctx.mode == PPU_MODE_0 || !graphics_ctx.ppu_enable) {
                        std::dynamic_pointer_cast<GameboyGPU>(BaseGPU::s_GetInstance())->VRAMDMANextBlock();
                    }

                    //LOG_WARN("VRAM HBLANK DMA: ", graphics_ctx.dma_length * 0x10);
                } else {
                    // General purpose DMA
                    u8 blocks = (IO[CGB_HDMA5_ADDR - IO_OFFSET] & 0x7F) + 1;
                    u16 length = (u16)blocks * 0x10;

                    int machine_cycles = ((int)blocks * VRAM_DMA_MC_PER_BLOCK * machineCtx.currentSpeed) + 1;
                    for (int i = 0; i < machine_cycles; i++) {
                        m_CoreInstance->TickTimers();
                    }

                    if (source_addr < ROM_N_OFFSET) {
                        memcpy(&graphics_ctx.VRAM_N[machineCtx.vram_bank_selected][dest_addr], &ROM_0[source_addr], length);
                    } else if (source_addr < VRAM_N_OFFSET) {
                        memcpy(&graphics_ctx.VRAM_N[machineCtx.vram_bank_selected][dest_addr], &ROM_N[machineCtx.rom_bank_selected][source_addr - ROM_N_OFFSET], length);
                    } else if (source_addr < RAM_N_OFFSET) {
                        LOG_ERROR("[emu] HDMA source address ", std::format("{:04x}", source_addr), " undefined copy");
                        return;
                    } else if (source_addr < WRAM_0_OFFSET) {
                        memcpy(&graphics_ctx.VRAM_N[machineCtx.vram_bank_selected][dest_addr], &RAM_N[machineCtx.ram_bank_selected][source_addr - RAM_N_OFFSET], length);
                    } else if (source_addr < WRAM_N_OFFSET) {
                        memcpy(&graphics_ctx.VRAM_N[machineCtx.vram_bank_selected][dest_addr], &WRAM_0[source_addr - WRAM_0_OFFSET], length);
                    } else if (source_addr < MIRROR_WRAM_OFFSET) {
                        memcpy(&graphics_ctx.VRAM_N[machineCtx.vram_bank_selected][dest_addr], &WRAM_N[machineCtx.wram_bank_selected][source_addr - WRAM_N_OFFSET], length);
                    } else {
                        memcpy(&graphics_ctx.VRAM_N[machineCtx.vram_bank_selected][dest_addr], &RAM_N[machineCtx.ram_bank_selected][source_addr - (RAM_N_OFFSET + 0x4000)], length);
                    }

                    IO[CGB_HDMA5_ADDR - IO_OFFSET] = 0xFF;
                }
            }
        }

        void GameboyMEM::OAM_DMA(const u8& _data) {
            graphics_ctx.oam_dma = false;
    
            auto m_CoreInstance = std::dynamic_pointer_cast<GameboyCPU>(BaseCPU::s_GetInstance());
            m_CoreInstance->TickTimers();

            IO[OAM_DMA_ADDR - IO_OFFSET] = _data;
            u16 source_addr = (u16)IO[OAM_DMA_ADDR - IO_OFFSET] << 8;

            if (source_addr < ROM_N_OFFSET) {
                graphics_ctx.oam_dma_src_addr = source_addr;
                graphics_ctx.oam_dma_mem = MEM_TYPE::ROM0;
            } else if (source_addr < VRAM_N_OFFSET) {
                graphics_ctx.oam_dma_src_addr = source_addr - ROM_N_OFFSET;
                graphics_ctx.oam_dma_mem = MEM_TYPE::ROMn;
            } else if (source_addr < RAM_N_OFFSET) {
                LOG_ERROR("[emu] OAM DMA source address ", std::format("{:04x}", source_addr), " undefined copy");
                return;
            } else if (source_addr < WRAM_0_OFFSET) {
                graphics_ctx.oam_dma_src_addr = source_addr - RAM_N_OFFSET;
                graphics_ctx.oam_dma_mem = MEM_TYPE::RAMn;
            } else if (source_addr < WRAM_N_OFFSET) {
                graphics_ctx.oam_dma_src_addr = source_addr - WRAM_0_OFFSET;
                graphics_ctx.oam_dma_mem = MEM_TYPE::WRAM0;
            } else if (source_addr < MIRROR_WRAM_OFFSET) {
                graphics_ctx.oam_dma_src_addr = source_addr - WRAM_N_OFFSET;
                graphics_ctx.oam_dma_mem = MEM_TYPE::WRAMn;
            } else {
                LOG_ERROR("[emu] OAM DMA source address not implemented / allowed");
                return;
            }

            graphics_ctx.oam_dma = true;
            graphics_ctx.oam_dma_counter = 0;
        }

        /* ***********************************************************************************************************
            TIMA CONTROL
        *********************************************************************************************************** */
        void GameboyMEM::ProcessTAC() {
            switch (IO[TAC_ADDR - IO_OFFSET] & TAC_CLOCK_SELECT) {
            case 0x00:
                machineCtx.timaDivMask = TIMA_DIV_BIT_9;
                break;
            case 0x01:
                machineCtx.timaDivMask = TIMA_DIV_BIT_3;
                break;
            case 0x02:
                machineCtx.timaDivMask = TIMA_DIV_BIT_5;
                break;
            case 0x03:
                machineCtx.timaDivMask = TIMA_DIV_BIT_7;
                break;
            }
        }

        /* ***********************************************************************************************************
            SPEED SWITCH
        *********************************************************************************************************** */
        void GameboyMEM::SwitchSpeed(const u8& _data) {
            if (machineCtx.is_cgb) {
                if (_data & PREPARE_SPEED_SWITCH) {
                    IO[CGB_SPEED_SWITCH_ADDR - IO_OFFSET] ^= SET_SPEED;
                    machineCtx.speed_switch_requested = true;
                }
            }
        }

        /* ***********************************************************************************************************
            CONTROLLER INPUT
        *********************************************************************************************************** */
        void GameboyMEM::SetControlValues(const u8& _data) {
            control_ctx.buttons_selected = (!(_data & JOYP_SELECT_BUTTONS)) ? true : false;
            control_ctx.dpad_selected = (!(_data & JOYP_SELECT_DPAD)) ? true : false;

            if (control_ctx.buttons_selected || control_ctx.dpad_selected) {
                u8 data = _data & JOYP_SELECT_MASK;
                if (control_ctx.buttons_selected) {
                    if (!control_ctx.start_pressed) {
                        data |= JOYP_START_DOWN;
                    }
                    if (!control_ctx.select_pressed) {
                        data |= JOYP_SELECT_UP;
                    }
                    if (!control_ctx.b_pressed) {
                        data |= JOYP_B_LEFT;
                    }
                    if (!control_ctx.a_pressed) {
                        data |= JOYP_A_RIGHT;
                    }
                }
                if (control_ctx.dpad_selected) {
                    if (!control_ctx.down_pressed) {
                        data |= JOYP_START_DOWN;
                    }
                    if (!control_ctx.up_pressed) {
                        data |= JOYP_SELECT_UP;
                    }
                    if (!control_ctx.left_pressed) {
                        data |= JOYP_B_LEFT;
                    }
                    if (!control_ctx.right_pressed) {
                        data |= JOYP_A_RIGHT;
                    }
                }

                IO[JOYP_ADDR - IO_OFFSET] = data | 0xC0;
            } else {
                IO[JOYP_ADDR - IO_OFFSET] = ((_data & JOYP_SELECT_MASK) | JOYP_RESET_BUTTONS) | 0xC0;
            }
        }

        void GameboyMEM::SetButton(const u8& _bit, const bool& _is_button) {
            if ((control_ctx.buttons_selected && _is_button) || (control_ctx.dpad_selected && !_is_button)) {
                IO[JOYP_ADDR - IO_OFFSET] &= ~_bit;
                RequestInterrupts(IRQ_JOYPAD);
            }
        }

        void GameboyMEM::UnsetButton(const u8& _bit, const bool& _is_button) {
            if ((control_ctx.buttons_selected && _is_button) || (control_ctx.dpad_selected && !_is_button)) {
                IO[JOYP_ADDR - IO_OFFSET] |= _bit;
            }
        }

        /* ***********************************************************************************************************
            OBJ PRIORITY MODE
        *********************************************************************************************************** */
        // not relevant, gets set by boot rom but we do the check for DMG or CGB on our own
        void GameboyMEM::SetObjPrio(const u8& _data) {
            switch (_data & PRIO_MODE) {
            case DMG_OBJ_PRIO_MODE:
                IO[CGB_OBJ_PRIO_ADDR - IO_OFFSET] |= DMG_OBJ_PRIO_MODE;
                break;
            case CGB_OBJ_PRIO_MODE:
                IO[CGB_OBJ_PRIO_ADDR - IO_OFFSET] &= ~DMG_OBJ_PRIO_MODE;
                break;
            }
        }

        /* ***********************************************************************************************************
            LCD AND PPU FUNCTIONALITY
        *********************************************************************************************************** */
        void GameboyMEM::SetLCDCValues(const u8& _data) {
            IO[LCDC_ADDR - IO_OFFSET] = _data;

            // bit 0
            if (_data & PPU_LCDC_WINBG_EN_PRIO) {
                graphics_ctx.bg_win_enable = true;              // DMG
                graphics_ctx.obj_prio = true;                   // CGB
            } else {
                graphics_ctx.bg_win_enable = false;
                graphics_ctx.obj_prio = false;
            }

            // bit 1
            if(_data & PPU_LCDC_OBJ_ENABLE) {
                graphics_ctx.obj_enable = true;
            } else {
                graphics_ctx.obj_enable = false;
            }

            // bit 2
            if (_data & PPU_LCDC_OBJ_SIZE) {
                graphics_ctx.obj_size_16 = true;
            } else {
                graphics_ctx.obj_size_16 = false;
            }

            // bit 3
            if (_data & PPU_LCDC_BG_TILEMAP) {
                graphics_ctx.bg_tilemap_offset = PPU_TILE_MAP1 - VRAM_N_OFFSET;
            } else {
                graphics_ctx.bg_tilemap_offset = PPU_TILE_MAP0 - VRAM_N_OFFSET;
            }

            // bit 4
            if (_data & PPU_LCDC_WINBG_TILEDATA) {
                graphics_ctx.bg_win_addr_mode_8000 = true;
            } else {
                graphics_ctx.bg_win_addr_mode_8000 = false;
            }

            // bit 5
            if (_data & PPU_LCDC_WIN_ENABLE) {
                graphics_ctx.win_enable = true;
            } else {
                graphics_ctx.win_enable = false;
            }

            // bit 6
            if (_data & PPU_LCDC_WIN_TILEMAP) {
                graphics_ctx.win_tilemap_offset = PPU_TILE_MAP1 - VRAM_N_OFFSET;
            } else {
                graphics_ctx.win_tilemap_offset = PPU_TILE_MAP0 - VRAM_N_OFFSET;
            }

            // bit 7
            if (_data & PPU_LCDC_ENABLE) {
                graphics_ctx.ppu_enable = true;
            } else {
                graphics_ctx.ppu_enable = false;
                IO[LY_ADDR - IO_OFFSET] = 0x00;

                auto m_GraphicsInstance = std::dynamic_pointer_cast<GameboyGPU>(BaseGPU::s_GetInstance());

                m_GraphicsInstance->SetMode(PPU_MODE_2);

                if (graphics_ctx.vram_dma_ppu_en) {
                    m_GraphicsInstance->VRAMDMANextBlock();
                    graphics_ctx.vram_dma_ppu_en = false;             // needs to be investigated if this is correct, docs state that the block transfer happens when ppu was enabled when hdma started and ppu gets disabled afterwards
                }
            }
        }

        void GameboyMEM::SetLCDSTATValues(const u8& _data) {
            u8 data = (IO[STAT_ADDR - IO_OFFSET] & ~PPU_STAT_WRITEABLE_BITS) | (_data & PPU_STAT_WRITEABLE_BITS);
            IO[STAT_ADDR - IO_OFFSET] = data;

            if (data & PPU_STAT_MODE0_EN) {
                graphics_ctx.mode_0_int_sel = true;
            } else {
                graphics_ctx.mode_0_int_sel = false;
            }

            if (data & PPU_STAT_MODE1_EN) {
                graphics_ctx.mode_1_int_sel = true;
            } else {
                graphics_ctx.mode_1_int_sel = false;
            }

            if (data & PPU_STAT_MODE2_EN) {
                graphics_ctx.mode_2_int_sel = true;
            } else {
                graphics_ctx.mode_2_int_sel = false;
            }

            if (data & PPU_STAT_LYC_SOURCE) {
                graphics_ctx.lyc_ly_int_sel = true;
            } else {
                graphics_ctx.lyc_ly_int_sel = false;
            }
        }

        void GameboyMEM::SetColorPaletteValues(const u8& _data, u32* _color_palette, u32* _source_palette) {
            u8 colors = _data;

            // TODO: CGB is able to set different color palettes for DMG games
            if (machineCtx.is_cgb) {
                for (int i = 0; i < 4; i++) {
                    switch (colors & 0x03) {
                    case 0x00:
                        _color_palette[i] = CGB_DMG_COLOR_WHITE;
                        break;
                    case 0x01:
                        _color_palette[i] = CGB_DMG_COLOR_LIGHTGREY;
                        break;
                    case 0x02:
                        _color_palette[i] = CGB_DMG_COLOR_DARKGREY;
                        break;
                    case 0x03:
                        _color_palette[i] = CGB_DMG_COLOR_BLACK;
                        break;
                    }

                    colors >>= 2;
                }
            } else if (machineCtx.cgb_compatibility) {
                for (int i = 0; i < 4; i++) {
                    switch (colors & 0x03) {
                    case 0x00:
                        _color_palette[i] = _source_palette[0];
                        break;
                    case 0x01:
                        _color_palette[i] = _source_palette[1];
                        break;
                    case 0x02:
                        _color_palette[i] = _source_palette[2];
                        break;
                    case 0x03:
                        _color_palette[i] = _source_palette[3];
                        break;
                    }

                    colors >>= 2;
                }
            } else {
                for (int i = 0; i < 4; i++) {
                    switch (colors & 0x03) {
                    case 0x00:
                        _color_palette[i] = DMG_COLOR_WHITE_ALT;
                        break;
                    case 0x01:
                        _color_palette[i] = DMG_COLOR_LIGHTGREY_ALT;
                        break;
                    case 0x02:
                        _color_palette[i] = DMG_COLOR_DARKGREY_ALT;
                        break;
                    case 0x03:
                        _color_palette[i] = DMG_COLOR_BLACK_ALT;
                        break;
                    }

                    colors >>= 2;
                }
            }
        }

        void GameboyMEM::SetBGWINPaletteValues(const u8& _data) {
            u8 addr = IO[BCPS_BGPI_ADDR - IO_OFFSET] & 0x3F;

            if (!graphics_ctx.ppu_enable || graphics_ctx.mode != PPU_MODE_3) {
                graphics_ctx.cgb_bgp_palette_ram[addr] = _data;

                // set new palette value
                int ram_index = addr & 0xFE;                    // get index in color ram, first bit irrelevant because colors aligned as 2 bytes
                int palette_index = addr >> 3;                  // get index of palette (divide by 8 (divide by 2^3 -> shift by 3), because each palette has 4 colors @ 2 bytes = 8 bytes each)
                int color_index = (addr & 0x07) >> 1;           // get index of color within palette (lower 3 bit -> can address 8 byte; shift by 1 because each color has 2 bytes (divide by 2^1))

                // NOTE: CGB has RGB555 in little endian -> reverse order
                u16 rgb555_color = graphics_ctx.cgb_bgp_palette_ram[ram_index] | ((u16)graphics_ctx.cgb_bgp_palette_ram[ram_index + 1] << 8);
                u32 rgba8888_color = ((u32)(rgb555_color & PPU_CGB_RED) << 27) | ((u32)(rgb555_color & PPU_CGB_GREEN) << 14) | ((u32)(rgb555_color & PPU_CGB_BLUE) << 1) | 0xFF;        // additionally leftshift each value by 3 -> 5 bit to 8 bit 'scaling'

                graphics_ctx.cgb_bgp_color_palettes[palette_index][color_index] = rgba8888_color;
        
                /*
                LOG_WARN("------------");
                LOG_WARN("RAM: ", ram_index, "; Palette: ", palette_index, "; Color: ", color_index, "; Data: ", format("{:02x}", _data));
                for (int i = 0; i < 8; i++) {
                    LOG_INFO("Palette ", i, ": ", format("{:08x}", graphics_ctx.cgb_bgp_color_palettes[i][0]), format("{:08x}", graphics_ctx.cgb_bgp_color_palettes[i][1]), format("{:08x}", graphics_ctx.cgb_bgp_color_palettes[i][2]), format("{:08x}", graphics_ctx.cgb_bgp_color_palettes[i][3]));
                }
                */
            }

            if (graphics_ctx.bgp_increment) {
                addr++;
                IO[BCPS_BGPI_ADDR - IO_OFFSET] = (IO[BCPS_BGPI_ADDR - IO_OFFSET] & PPU_CGB_PALETTE_INDEX_INC) | (addr & 0x3F);
            }
        }

        void GameboyMEM::SetOBJPaletteValues(const u8& _data) {
            u8 addr = IO[OCPS_OBPI_ADDR - IO_OFFSET] & 0x3F;

            if (!graphics_ctx.ppu_enable || graphics_ctx.mode != PPU_MODE_3) {
                graphics_ctx.cgb_obp_palette_ram[addr] = _data;

                // set new palette value
                int ram_index = addr & 0xFE;
                int palette_index = addr >> 3;
                int color_index = (addr & 0x07) >> 1;

                u16 rgb555_color = graphics_ctx.cgb_obp_palette_ram[ram_index] | ((u16)graphics_ctx.cgb_obp_palette_ram[ram_index + 1] << 8);
                u32 rgba8888_color = ((u32)(rgb555_color & PPU_CGB_RED) << 27) | ((u32)(rgb555_color & PPU_CGB_GREEN) << 14) | ((u32)(rgb555_color & PPU_CGB_BLUE) << 1) | 0xFF;

                graphics_ctx.cgb_obp_color_palettes[palette_index][color_index] = rgba8888_color;
            }

            if (graphics_ctx.obp_increment) {
                addr++;
                IO[OCPS_OBPI_ADDR - IO_OFFSET] = (IO[OCPS_OBPI_ADDR - IO_OFFSET] & PPU_CGB_PALETTE_INDEX_INC) | (addr & 0x3F);
            }
        }

        void GameboyMEM::SetBCPS(const u8& _data) {
            IO[BCPS_BGPI_ADDR - IO_OFFSET] = _data;
            graphics_ctx.bgp_increment = (_data & PPU_CGB_PALETTE_INDEX_INC ? true : false);
        }

        void GameboyMEM::SetOCPS(const u8& _data) {
            IO[OCPS_OBPI_ADDR - IO_OFFSET] = _data;
            graphics_ctx.obp_increment = (_data & PPU_CGB_PALETTE_INDEX_INC ? true : false);
        }

        /* ***********************************************************************************************************
            MISC BANK SELECT
        *********************************************************************************************************** */
        void GameboyMEM::SetVRAMBank(const u8& _data) {
            if (machineCtx.is_cgb || machineCtx.cgb_compatibility) {
                IO[CGB_VRAM_SELECT_ADDR - IO_OFFSET] = _data & 0x01;
                machineCtx.vram_bank_selected = IO[CGB_VRAM_SELECT_ADDR - IO_OFFSET];
            }
        }

        void GameboyMEM::SetWRAMBank(const u8& _data) {
            if (machineCtx.is_cgb || machineCtx.cgb_compatibility) {
                IO[CGB_WRAM_SELECT_ADDR - IO_OFFSET] = _data & 0x07;
                if (IO[CGB_WRAM_SELECT_ADDR - IO_OFFSET] == 0) IO[CGB_WRAM_SELECT_ADDR - IO_OFFSET] = 1;

                machineCtx.wram_bank_selected = IO[CGB_WRAM_SELECT_ADDR - IO_OFFSET] - 1; 
            }
        }

        /* ***********************************************************************************************************
            APU CONTROL
        *********************************************************************************************************** */
        // TODO: reset registers
        void GameboyMEM::SetAPUMasterControl(const u8& _data) {
            IO[NR52_ADDR - IO_OFFSET] = (IO[NR52_ADDR - IO_OFFSET] & ~APU_ENABLE_BIT) | (_data & APU_ENABLE_BIT);

            sound_ctx.apuEnable = _data & APU_ENABLE_BIT ? true : false;
        }

        void GameboyMEM::SetAPUChannelPanning(const u8& _data) {
            IO[NR51_ADDR - IO_OFFSET] = _data;

            for (u8 mask = 0x01; auto & n : sound_ctx.ch_ctxs) {
                n.right.store(_data & mask ? true : false);
                n.left.store(_data & (mask << 4) ? true : false);
                mask <<= 1;
            }
        }

        void GameboyMEM::SetAPUMasterVolume(const u8& _data) {
            IO[NR50_ADDR - IO_OFFSET] = _data;

            sound_ctx.masterVolumeRight.store((float)VOLUME_MAP.at(_data & MASTER_VOLUME_RIGHT));
            sound_ctx.masterVolumeLeft.store((float)VOLUME_MAP.at((_data & MASTER_VOLUME_LEFT) >> 4));
            sound_ctx.outRightEnabled.store(_data & 0x08 ? true : false);
            sound_ctx.outLeftEnabled.store(_data & 0x80 ? true : false);
        }

        void GameboyMEM::SetAPUCh1Sweep(const u8& _data) {
            auto* ch_ctx = &sound_ctx.ch_ctxs[0];
            auto* period_ctx = static_cast<ch_ext_period*>(ch_ctx->exts[PERIOD].get());
            IO[ch_ctx->regs.nrX0 - IO_OFFSET] = _data;

            period_ctx->sweep_pace = (_data & CH1_SWEEP_PACE) >> 4;
            period_ctx->sweep_dir_subtract = (_data & CH1_SWEEP_DIR ? true : false);
            period_ctx->sweep_period_step = _data & CH1_SWEEP_STEP;
        }

        void GameboyMEM::SetAPUCh12TimerDutyCycle(const u8& _data, channel_context* _ch_ctx) {
            auto* pwm_ctx = static_cast<ch_ext_pwm*>(_ch_ctx->exts[PWM].get());
            IO[_ch_ctx->regs.nrX1 - IO_OFFSET] = _data;

            pwm_ctx->duty_cycle_index.store((_data & CH_1_2_DUTY_CYCLE) >> 6);
            _ch_ctx->length_timer = _data & CH_1_2_4_LENGTH_TIMER;
            _ch_ctx->length_altered = true;
        }

        void GameboyMEM::SetAPUCh124Envelope(const u8& _data, channel_context* _ch_ctx) {
            auto* env_ctx = static_cast<ch_ext_envelope*>(_ch_ctx->exts[ENVELOPE].get());
            IO[_ch_ctx->regs.nrX2 - IO_OFFSET] = _data;

            env_ctx->envelope_volume = (_data & CH_1_2_4_ENV_VOLUME) >> 4;
            env_ctx->envelope_increase = (_data & CH_1_2_4_ENV_DIR ? true : false);
            env_ctx->envelope_pace = _data & CH_1_2_4_ENV_PACE;

            _ch_ctx->volume.store((float)env_ctx->envelope_volume / 0xF);

            _ch_ctx->dac = _data & 0xF8 ? true : false;
            if (!_ch_ctx->dac) {
                _ch_ctx->enable.store(false);
                IO[NR52_ADDR - IO_OFFSET] &= ~_ch_ctx->enable_bit;
            }
        }

        void GameboyMEM::SetAPUCh12PeriodLow(const u8& _data, channel_context* _ch_ctx) {
            IO[_ch_ctx->regs.nrX3 - IO_OFFSET] = _data;

            _ch_ctx->period = _data | (((u16)IO[_ch_ctx->regs.nrX4 - IO_OFFSET] & CH_1_2_3_PERIOD_HIGH) << 8);
            _ch_ctx->sampling_rate.store((float)(pow(2, 20) / (CH_1_2_3_PERIOD_FLIP - _ch_ctx->period)));
        }

        void GameboyMEM::SetAPUCh12PeriodHighControl(const u8& _data, channel_context* _ch_ctx) {
            IO[_ch_ctx->regs.nrX4 - IO_OFFSET] = _data;

            if (_data & CH_1_2_3_4_CTRL_TRIGGER && _ch_ctx->dac) {
                _ch_ctx->enable.store(true);
                IO[NR52_ADDR - IO_OFFSET] |= _ch_ctx->enable_bit;
            }
            _ch_ctx->length_enable = _data & CH_1_2_3_4_CTRL_LENGTH_EN ? true : false;

            _ch_ctx->period = (((u16)_data & CH_1_2_3_PERIOD_HIGH) << 8) | IO[_ch_ctx->regs.nrX3 - IO_OFFSET];
            _ch_ctx->sampling_rate.store((float)(pow(2, 20) / (CH_1_2_3_PERIOD_FLIP - _ch_ctx->period)));
        }

        void GameboyMEM::SetAPUCh3DACEnable(const u8& _data) {
            auto* ch_ctx = &sound_ctx.ch_ctxs[2];
            IO[NR30_ADDR - IO_OFFSET] = _data;

            ch_ctx->dac = _data & CH_3_DAC ? true : false;
            if (!ch_ctx->dac) {
                ch_ctx->enable.store(false);
                IO[NR52_ADDR - IO_OFFSET] &= ~CH_3_ENABLE;
            }
        }

        void GameboyMEM::SetAPUCh3Timer(const u8& _data) {
            auto* ch_ctx = &sound_ctx.ch_ctxs[2];
            IO[NR31_ADDR - IO_OFFSET] = _data;

            ch_ctx->length_timer = _data;
            ch_ctx->length_altered = true;
        }

        void GameboyMEM::SetAPUCh3Volume(const u8& _data) {
            static int change_counter = 0;
            static int prev_chunk_id = BaseAPU::s_GetInstance()->GetChunkId();
            static int cur_chunk_id = prev_chunk_id;

            auto* ch_ctx = &sound_ctx.ch_ctxs[2];
            IO[NR32_ADDR - IO_OFFSET] = _data;

            switch (_data & CH_3_VOLUME) {
            case 0x00:
                ch_ctx->volume.store(VOLUME_MAP.at(8));
                break;
            case 0x20:
                ch_ctx->volume.store(VOLUME_MAP.at(7));
                break;
            case 0x40:
                ch_ctx->volume.store(VOLUME_MAP.at(3));
                break;
            case 0x60:
                ch_ctx->volume.store(VOLUME_MAP.at(1));
                break;
            }

            if (ch_ctx->use_current_state) {
                cur_chunk_id = BaseAPU::s_GetInstance()->GetChunkId();
                if (cur_chunk_id != prev_chunk_id) {
                    prev_chunk_id = cur_chunk_id;
                    change_counter = 0;
                }

                ++change_counter;
                if (change_counter > 1) {
                    ch_ctx->use_current_state = false;
                    change_counter = 0;
                }
            }
        }

        void GameboyMEM::SetAPUCh3PeriodLow(const u8& _data) {
            auto* ch_ctx = &sound_ctx.ch_ctxs[2];
            IO[NR33_ADDR - IO_OFFSET] = _data;

            ch_ctx->period = _data | (((u16)IO[NR34_ADDR - IO_OFFSET] & CH_1_2_3_PERIOD_HIGH) << 8);
            ch_ctx->sampling_rate.store((float)(pow(2, 21) / (CH_1_2_3_PERIOD_FLIP - ch_ctx->period)));
        }

        void GameboyMEM::SetAPUCh3PeriodHighControl(const u8& _data) {
            auto* ch_ctx = &sound_ctx.ch_ctxs[2];
            IO[NR34_ADDR - IO_OFFSET] = _data;

            if (_data & CH_1_2_3_4_CTRL_TRIGGER && ch_ctx->dac) {
                ch_ctx->enable.store(true);
                IO[NR52_ADDR - IO_OFFSET] |= CH_3_ENABLE;
            }
            ch_ctx->length_enable = _data & CH_1_2_3_4_CTRL_LENGTH_EN ? true : false;

            ch_ctx->period = (((u16)_data & CH_1_2_3_PERIOD_HIGH) << 8) | IO[NR33_ADDR - IO_OFFSET];
            ch_ctx->sampling_rate.store((float)(pow(2, 21) / (CH_1_2_3_PERIOD_FLIP - ch_ctx->period)));
        }

        void GameboyMEM::SetAPUCh3WaveRam(const u16& _addr, const u8& _data) {
            IO[_addr - IO_OFFSET] = _data;

            auto* ch_ctx = &sound_ctx.ch_ctxs[2];
            auto* wave_ctx = static_cast<ch_ext_waveram*>(ch_ctx->exts[WAVE_RAM].get());

            int index = (_addr - WAVE_RAM_ADDR) << 1;
            unique_lock<mutex> lock_wave_ram(wave_ctx->mut_wave_ram);
            wave_ctx->wave_ram[index] = ((float)(_data >> 4) / 0x7) - 1.f;
            wave_ctx->wave_ram[index + 1] = ((float)(_data & 0xF) / 0x7) - 1.f;
        }

        void GameboyMEM::SetAPUCh4Timer(const u8& _data) {
            auto* ch_ctx = &sound_ctx.ch_ctxs[3];
            IO[NR41_ADDR - IO_OFFSET] = _data;

            ch_ctx->length_timer = _data & CH_1_2_4_LENGTH_TIMER;
            ch_ctx->length_altered = true;
        }

        void GameboyMEM::SetAPUCh4FrequRandomness(const u8& _data) {
            IO[NR43_ADDR - IO_OFFSET] = _data;

            auto* ch_ctx = &sound_ctx.ch_ctxs[3];
            auto* lfsr_ctx = static_cast<ch_ext_lfsr*>(ch_ctx->exts[LFSR].get());

            lfsr_ctx->lfsr_width_7bit = _data & CH_4_LFSR_WIDTH ? true : false;

            int ch4_clock_shift = (_data & CH_4_CLOCK_SHIFT) >> 4;
            int t = _data & CH_4_CLOCK_DIVIDER;
            float ch4_clock_divider = t ? (float)t : .5f;
            ch_ctx->sampling_rate.store((float)(pow(2, 18) / (ch4_clock_divider * pow(2, ch4_clock_shift))));

            lfsr_ctx->lfsr_step = (float)(ch_ctx->sampling_rate.load() / BASE_CLOCK_CPU);
        }

        void GameboyMEM::SetAPUCh4Control(const u8& _data) {
            IO[NR44_ADDR - IO_OFFSET] = _data;

            auto* ch_ctx = &sound_ctx.ch_ctxs[3];
            auto* lfsr_ctx = static_cast<ch_ext_lfsr*>(ch_ctx->exts[LFSR].get());

            if (_data & CH_1_2_3_4_CTRL_TRIGGER && ch_ctx->dac) {
                lfsr_ctx->lfsr = CH_4_LFSR_INIT_VALUE;
                ch_ctx->enable.store(true);
                IO[NR52_ADDR - IO_OFFSET] |= CH_4_ENABLE;
            }
            ch_ctx->length_enable = _data & CH_1_2_3_4_CTRL_LENGTH_EN ? true : false;
        }

        /* ***********************************************************************************************************
            SERIAL (SPI)
        *********************************************************************************************************** */
        void GameboyMEM::SetSerialData(const u8& _data) {
            IO[SERIAL_DATA - IO_OFFSET] = _data;
        }

        void GameboyMEM::SetSerialControl(const u8& _data) {
            u8& ctrl = IO[SERIAL_CTRL - IO_OFFSET];
            ctrl = (ctrl & ~SERIAL_CTRL_MASK) | (_data & SERIAL_CTRL_MASK);

            serial_ctx.master = ctrl & SERIAL_CTRL_CLOCK ? true : false;
            serial_ctx.transfer_requested = ctrl & SERIAL_CTRL_ENABLE ? true : false;
            if (ctrl & SERIAL_CTRL_SPEED) {
                serial_ctx.div_low_byte = true;
                serial_ctx.div_bit = SERIAL_HIGH_SPEED_BIT;
            } else {
                serial_ctx.div_low_byte = false;
                serial_ctx.div_bit = SERIAL_NORMAL_SPEED_BIT;
            }
        }

        /* ***********************************************************************************************************
            UNMAP BOOT ROM (AFTER FINISHING BOOT SEQUENCE)
        *********************************************************************************************************** */
        void GameboyMEM::UnmapBootRom() {
            InitRom(romData);
            romData.clear();
        }

        /* ***********************************************************************************************************
            MEMORY DEBUGGER
        *********************************************************************************************************** */
        void GameboyMEM::FillMemoryTable(std::vector<std::tuple<int, memory_entry>>& _table_section, u8* _bank_data, const int& _offset, const size_t& _size) {
            auto data = memory_entry();

            int size = (int)_size;
            int line_num = (int)LINE_NUM_HEX(size);
            int index;

            _table_section = memory_type_table(line_num);

            for (int i = 0; i < line_num; i++) {
                auto table_entry = std::tuple<int, memory_entry>();

                index = i * DEBUG_MEM_ELEM_PER_LINE;
                get<0>(table_entry) = _offset + index;

                data = memory_entry();
                get<MEM_ENTRY_ADDR>(data) = format("{:04x}", _offset + index);
                get<MEM_ENTRY_LEN>(data) = size - index > DEBUG_MEM_ELEM_PER_LINE - 1 ? DEBUG_MEM_ELEM_PER_LINE : size % DEBUG_MEM_ELEM_PER_LINE;
                get<MEM_ENTRY_REF>(data) = &_bank_data[index];

                get<1>(table_entry) = data;

                _table_section[i] = table_entry;
            }
        }

        void GameboyMEM::GenerateMemoryTables() {
            // access for memory inspector
            memoryTables.clear();

            // ROM
            {
                // ROM 0
                memoryTables.emplace_back();
                auto& memory_type_tables = memoryTables.back();
                memory_type_tables.SetMemoryType("ROM");

                memory_type_tables.emplace_back();
                auto& content = memory_type_tables.back();
                FillMemoryTable(content, ROM_0.data(), ROM_0_OFFSET, ROM_0.size());

                // ROM n
                for (auto& n : ROM_N) {
                    memory_type_tables.emplace_back();
                    auto& content = memory_type_tables.back();
                    FillMemoryTable(content, n.data(), ROM_0_OFFSET, n.size());
                }
            }

            // VRAM
            {
                memoryTables.emplace_back();
                auto& memory_type_tables = memoryTables.back();
                memory_type_tables.SetMemoryType("VRAM");

                for (auto& n : graphics_ctx.VRAM_N) {
                    memory_type_tables.emplace_back();
                    auto& content = memory_type_tables.back();
                    FillMemoryTable(content, n.data(), VRAM_N_OFFSET, n.size());
                }
            }

            // RAM
            if (machineCtx.ram_bank_num > 0) {
                memoryTables.emplace_back();
                auto& memory_type_tables = memoryTables.back();
                memory_type_tables.SetMemoryType("RAM");

                for (auto& n : RAM_N) {
                    memory_type_tables.emplace_back();
                    auto& content = memory_type_tables.back();
                    FillMemoryTable(content, n, RAM_N_OFFSET, RAM_N_SIZE);
                }
            }

            // WRAM
            {
                memoryTables.emplace_back();
                auto& memory_type_tables = memoryTables.back();
                memory_type_tables.SetMemoryType("WRAM");

                memory_type_tables.emplace_back();
                auto& content = memory_type_tables.back();
                FillMemoryTable(content, WRAM_0.data(), WRAM_0_OFFSET, WRAM_0.size());

                // WRAM n
                for (auto& n : WRAM_N) {
                    memory_type_tables.emplace_back();
                    auto& content = memory_type_tables.back();
                    FillMemoryTable(content, n.data(), WRAM_N_OFFSET, n.size());
                }
            }

            // OAM
            {
                memoryTables.emplace_back();
                auto& memory_type_tables = memoryTables.back();
                memory_type_tables.SetMemoryType("OAM");

                memory_type_tables.emplace_back();
                auto& content = memory_type_tables.back();
                FillMemoryTable(content, graphics_ctx.OAM.data(), OAM_OFFSET, graphics_ctx.OAM.size());
            }

            // IO
            {
                memoryTables.emplace_back();
                auto& memory_type_tables = memoryTables.back();
                memory_type_tables.SetMemoryType("IO");

                memory_type_tables.emplace_back();
                auto& content = memory_type_tables.back();
                FillMemoryTable(content, IO.data(), IO_OFFSET, IO.size());
            }

            // HRAM
            {
                memoryTables.emplace_back();
                auto& memory_type_tables = memoryTables.back();
                memory_type_tables.SetMemoryType("HRAM");

                memory_type_tables.emplace_back();
                auto& content = memory_type_tables.back();
                FillMemoryTable(content, HRAM.data(), HRAM_OFFSET, HRAM.size());
            }
        }
    }
}