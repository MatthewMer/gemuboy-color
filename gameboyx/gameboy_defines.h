#pragma once
/* ***********************************************************************************************************
    DESCRIPTION
*********************************************************************************************************** */
/*
*	all the gameboy defines related to the hardware, specific addresses, offsets, etc.
*/

#define GB_PLAYER_COUNT                 1

/* ***********************************************************************************************************
    FREQUENCIES
*********************************************************************************************************** */
#define DIV_FREQUENCY                   16384                                                       // Hz
#define BASE_CLOCK_CPU                  (4.f * pow(2, 20))                                          // Hz
#define DISPLAY_FREQUENCY               60                                                          // Hz
#define TICKS_PER_MC                    4                                                           // clock cycles per machine cycle

/* ***********************************************************************************************************
    PROCESSOR AND ROM DEFINES
*********************************************************************************************************** */
// ROM HEADER *****
#define ROM_HEAD_ADDR				    0x0100
#define ROM_HEAD_SIZE				    0x0050

#define ROM_HEAD_LOGO                   0x0104
#define ROM_HEAD_TITLE				    0x0134
#define ROM_HEAD_CGBFLAG			    0x0143				// until here; 0x80 = supports CGB, 0xC0 = CGB only		(bit 6 gets ignored)
#define ROM_HEAD_NEWLIC				    0x0144				// 2 char ascii; only if oldlic = 0x33
#define ROM_HEAD_SGBFLAG			    0x0146				// 0x03 = en, else not
#define ROM_HEAD_HW_TYPE			    0x0147				// hardware on cartridge
#define ROM_HEAD_ROMSIZE			    0x0148
#define ROM_HEAD_RAMSIZE			    0x0149
#define ROM_HEAD_DEST				    0x014A				// 0x00 = japan, 0x01 = overseas
#define ROM_HEAD_OLDLIC				    0x014B
#define ROM_HEAD_VERSION			    0x014C
#define ROM_HEAD_CHKSUM				    0x014D
#define ROM_HEAD_END                    0x014F

#define ROM_HEAD_TITLE_SIZE_CGB		    15
#define ROM_HEAD_TITLE_SIZE_GB		    16

// EXTERNAL HARDWARE *****
#define ROM_BASE_SIZE                   0x8000              // 32 KiB
#define RAM_BASE_SIZE                   0x2000              // 8 KiB 

// SM83 MEMORY MAPPING *****
#define ROM_0_OFFSET                    0x0000
#define ROM_0_SIZE                      0x4000

#define ROM_N_OFFSET                    0x4000
#define ROM_N_SIZE                      0x4000

#define VRAM_N_OFFSET                   0x8000              // CGB mode: switchable (0-1)
#define VRAM_N_SIZE                     0x2000

#define RAM_N_OFFSET                    0xA000              // external (GameboyCartridge)
#define RAM_N_SIZE                      0x2000

#define WRAM_0_OFFSET                   0xC000
#define WRAM_0_SIZE                     0x1000

#define WRAM_N_OFFSET                   0xD000              // CGB mode: switchable (1-7)
#define WRAM_N_SIZE                     0x1000

#define MIRROR_WRAM_OFFSET              0xE000              // not used (mirror of WRAM 0)
#define MIRROR_WRAM_SIZE                0x1E00

#define OAM_OFFSET                      0xFE00              // object attribute memory
#define OAM_SIZE                        0x00A0

#define NOT_USED_MEMORY_OFFSET          0xFEA0              // prohibited
#define NOT_USED_MEMORY_SIZE            0x0060

#define IO_OFFSET                       0xFF00              // peripherals
#define IO_SIZE                         0x0080

#define HRAM_OFFSET                     0xFF80              // stack, etc.
#define HRAM_SIZE                       0x007F

#define IE_OFFSET                       0xFFFF

/* ***********************************************************************************************************
    IO REGISTER DEFINES
*********************************************************************************************************** */
// joypad
#define JOYP_ADDR                       0xFF00              // Mixed

// serial
#define SERIAL_DATA                     0xFF01
#define SERIAL_CTRL                     0xFF02
#define SERIAL_CTRL_MASK                0x83
#define SERIAL_CTRL_CLOCK               0x01
#define SERIAL_CTRL_SPEED               0x02
#define SERIAL_CTRL_ENABLE              0x80
#define SERIAL_HIGH_SPEED_BIT           0x08
#define SERIAL_NORMAL_SPEED_BIT         0x01

// timers
#define DIV_ADDR                        0xFF04              // RW
#define TIMA_ADDR                       0xFF05              // RW
#define TMA_ADDR                        0xFF06              // RW
#define TAC_ADDR                        0xFF07              // RW
#define TAC_CLOCK_SELECT                0x03
#define TAC_CLOCK_ENABLE                0x04

// interrupt flags
#define IF_ADDR                         0xFF0F              // RW

#define IRQ_VBLANK                      0x01
#define IRQ_LCD_STAT                    0x02
#define IRQ_TIMER                       0x04
#define IRQ_SERIAL                      0x08
#define IRQ_JOYPAD                      0x10

#define ISR_VBLANK_HANDLER_ADDR         0x40
#define ISR_LCD_STAT_HANDLER_ADDR       0x48
#define ISR_TIMER_HANDLER_ADDR          0x50
#define ISR_SERIAL_HANDLER_ADDR         0x58
#define ISR_JOYPAD_HANDLER_ADDR         0x60

// sound
#define NR10_ADDR                       0xFF10              // RW
#define NR11_ADDR                       0xFF11              // Mixed
#define NR12_ADDR                       0xFF12              // RW
#define NR13_ADDR                       0xFF13              // W
#define NR14_ADDR                       0xFF14              // Mixed

#define NR21_ADDR                       0xFF16              // Mixed
#define NR22_ADDR                       0xFF17              // RW
#define NR23_ADDR                       0xFF18              // W
#define NR24_ADDR                       0xFF19              // Mixed

#define NR30_ADDR                       0xFF1A              // RW
#define NR31_ADDR                       0xFF1B              // W
#define NR32_ADDR                       0xFF1C              // RW
#define NR33_ADDR                       0xFF1D              // W
#define NR34_ADDR                       0xFF1E              // Mixed

#define NR41_ADDR                       0xFF20              // W
#define NR42_ADDR                       0xFF21              // RW
#define NR43_ADDR                       0xFF22              // RW
#define NR44_ADDR                       0xFF23              // Mixed

#define NR50_ADDR                       0xFF24              // RW
#define NR51_ADDR                       0xFF25              // RW
#define NR52_ADDR                       0xFF26              // Mixed

#define WAVE_RAM_ADDR                   0xFF30              // RW
#define WAVE_RAM_SIZE                   0x10

// LCD
#define LCDC_ADDR                       0xFF40              // RW
#define STAT_ADDR                       0xFF41              // Mixed
#define SCY_ADDR                        0xFF42              // RW
#define SCX_ADDR                        0xFF43              // RW
#define LY_ADDR                         0xFF44              // R
#define LYC_ADDR                        0xFF45              // RW

// OAM DMA
#define OAM_DMA_ADDR                    0xFF46              // RW
#define OAM_DMA_SOURCE_MAX              0xDF
#define OAM_DMA_LENGTH                  0xA0

// BG/OBJ PALETTES
#define BGP_ADDR                        0xFF47              // RW
#define OBP0_ADDR                       0xFF48              // RW
#define OBP1_ADDR                       0xFF49              // RW

#define BANK_ADDR                       0xFF50              // only for boot ROM

// WINDOW POS
#define WY_ADDR                         0xFF4A              // RW
#define WX_ADDR                         0xFF4B              // RW

// SPEED SWITCH
#define CGB_SPEED_SWITCH_ADDR           0xFF4D              // Mixed, affects: CPU 1MHz -> 2.1MHz, timer/divider, OAM DMA(not needed)
#define SPEED                           0x80
#define NORMAL_SPEED                    0x00
#define SET_SPEED                       0x80
#define PREPARE_SPEED_SWITCH            0x01

// VRAM BANK SELECT
#define CGB_VRAM_SELECT_ADDR            0xFF4F              // RW

// VRAM DMA
#define CGB_HDMA1_ADDR                  0xFF51              // W
#define CGB_HDMA2_ADDR                  0xFF52              // W
#define CGB_HDMA3_ADDR                  0xFF53              // W
#define CGB_HDMA4_ADDR                  0xFF54              // W
#define CGB_HDMA5_ADDR                  0xFF55              // RW
#define DMA_MODE_BIT                    0x80
#define DMA_MODE_GENERAL                0x00
#define DMA_MODE_HBLANK                 0x80

#define CGB_IR_ADDR                     0xFF56

// BG/OBJ PALETTES SPECS
#define BCPS_BGPI_ADDR                  0xFF68              // RW
#define BCPD_BGPD_ADDR                  0xFF69              // RW
#define OCPS_OBPI_ADDR                  0xFF6A              // RW
#define OCPD_OBPD_ADDR                  0xFF6B              // RW

#define CGB_COLOR_CHANNEL_MASK      0x1F

// OBJECT PRIORITY MODE
#define CGB_OBJ_PRIO_ADDR               0xFF6C              // RW
#define PRIO_MODE                       0x01
#define PRIO_OAM                        0x00
#define PRIO_COORD                      0x01

#define DMG_OBJ_PRIO_MODE               0x01
#define CGB_OBJ_PRIO_MODE               0x00

// WRAM BANK SELECT
#define CGB_WRAM_SELECT_ADDR            0xFF70              // RW

// DIGITAL AUDIO OUT
#define PCM12_ADDR                      0xFF76              // R
#define PCM34_ADDR                      0xFF77              // R

/* ***********************************************************************************************************
    CONTROL DEFINES
*********************************************************************************************************** */
#define JOYP_SELECT_MASK                0x30
#define JOYP_BUTTON_MASK                0x0F

#define JOYP_SELECT_BUTTONS             0x20
#define JOYP_SELECT_DPAD                0x10

#define JOYP_START_DOWN                 0x08
#define JOYP_SELECT_UP                  0x04
#define JOYP_B_LEFT                     0x02
#define JOYP_A_RIGHT                    0x01

#define JOYP_RESET_BUTTONS              0x0F

/* ***********************************************************************************************************
    GRAPHICS DEFINES
*********************************************************************************************************** */
// LCD CONTROL
#define PPU_LCDC_ENABLE                 0x80
#define PPU_LCDC_WIN_TILEMAP            0x40
#define PPU_LCDC_WIN_ENABLE             0x20
#define PPU_LCDC_WINBG_TILEDATA         0x10
#define PPU_LCDC_BG_TILEMAP             0x08
#define PPU_LCDC_OBJ_SIZE               0x04
#define PPU_LCDC_OBJ_ENABLE             0x02
#define PPU_LCDC_WINBG_EN_PRIO          0x01

// LCD STAT
#define PPU_STAT_MODE                   0x03
#define PPU_STAT_LYC_FLAG               0x04
#define PPU_STAT_MODE0_EN               0x08                // isr STAT hblank
#define PPU_STAT_MODE1_EN               0x10                // isr STAT vblank
#define PPU_STAT_MODE2_EN               0x20                // isr STAT oam
#define PPU_STAT_LYC_SOURCE             0x40                // isr STAT LYC == LY

#define PPU_STAT_WRITEABLE_BITS         0xF8

#define PPU_MODE_0                      0x00
#define PPU_MODE_1                      0x01
#define PPU_MODE_2                      0x02
#define PPU_MODE_3                      0x03

// lcd
#define LCD_SCANLINES_VBLANK            144
#define LCD_SCANLINES_TOTAL             154

// VRAM Tile data
#define PPU_VRAM_TILE_SIZE              16                  // Bytes
#define PPU_TILE_SIZE_SCANLINE          2
#define PPU_TILE_SIZE_X                 8
#define PPU_TILE_SIZE_Y                 8
#define PPU_TILE_SIZE_Y_16              16
#define PPU_VRAM_BASEPTR_8000           0x8000
#define PPU_VRAM_BASEPTR_8800           0x9000

// tile maps
#define PPU_TILE_MAP0                   0x9800
#define PPU_TILE_MAP1                   0x9C00

#define BG_ATTR_PALETTE_CGB             0x07
#define BG_ATTR_VRAM_BANK_CGB           0x08
#define BG_ATTR_FLIP_HORIZONTAL         0x20
#define BG_ATTR_FLIP_VERTICAL           0x40
#define BG_ATTR_OAM_PRIORITY            0x80

#define PPU_OBJ_ATTRIBUTES              0xFE00
#define PPU_OBJ_ATTR_NUM                40
#define PPU_OBJ_ATTR_BYTES              4

#define OBJ_ATTR_Y                      0
#define OBJ_ATTR_X                      1
#define OBJ_ATTR_INDEX                  2
#define OBJ_ATTR_FLAGS                  3

#define OBJ_ATTR_PALETTE_CGB            0x07
#define OBJ_ATTR_VRAM_BANK_CGB          0x08
#define OBJ_ATTR_PALETTE_DMG            0x10
#define OBJ_ATTR_X_FLIP                 0x20
#define OBJ_ATTR_Y_FLIP                 0x40
#define OBJ_ATTR_PRIO                   0x80

#define PPU_PALETTE_RAM_SIZE_CGB        64

// screen
#define PPU_TILES_HORIZONTAL            20
#define PPU_TILES_VERTICAL              18
#define PPU_PIXELS_TILE_X               8
#define PPU_PIXELS_TILE_Y               8
#define PPU_SCREEN_X                    160
#define PPU_SCREEN_Y                    144
#define PPU_TILEMAP_SIZE_1D             32
#define PPU_TILEMAP_SIZE_1D_PIXELS      256
#define PPU_OBJ_PER_SCANLINE            10

#define LCD_ASPECT_RATIO                10.f/9.f

#define DMG_COLOR_WHITE                 0x9bbc0fFF
#define DMG_COLOR_LIGHTGREY             0x8bac0fFF
#define DMG_COLOR_DARKGREY              0x306230FF
#define DMG_COLOR_BLACK                 0x0f380fFF

// alternative color palette for DMG
#define DMG_COLOR_WHITE_ALT             0xf8e8f8FF
#define DMG_COLOR_LIGHTGREY_ALT         0xd0a8b0FF
#define DMG_COLOR_DARKGREY_ALT          0x787890FF
#define DMG_COLOR_BLACK_ALT             0x000000FF

#define CGB_DMG_COLOR_WHITE             0xffffffff
#define CGB_DMG_COLOR_LIGHTGREY         0xa9a9a9ff
#define CGB_DMG_COLOR_DARKGREY          0x545454ff
#define CGB_DMG_COLOR_BLACK             0x000000ff

#define PPU_DOTS_PER_FRAME              70224
#define PPU_DOTS_PER_SCANLINE           (PPU_DOTS_PER_FRAME / LCD_SCANLINES_TOTAL)
#define PPU_DOTS_MODE_2                 80
#define PPU_DOTS_PER_OAM_ENTRY          2
#define PPU_DOTS_MODE_2_3               376
#define PPU_DOTS_MODE_3_MIN             174     // 160 pixels + 6 clocks fill shift registers + 8 clocks shift out 8 pixels while fetching again
#define PPU_OBJ_TILE_FETCH_TICKS        6

#define PPU_CGB_PALETTE_INDEX_INC       0x80
#define PPU_CGB_RED                     0x001F
#define PPU_CGB_GREEN                   0x03E0
#define PPU_CGB_BLUE                    0x7C00

/* ***********************************************************************************************************
    AUDIO DEFINES
*********************************************************************************************************** */
#define APU_CHANNELS_NUM                4

#define APU_BASE_CLOCK                  512

#define DIV_APU_SINGLESPEED_BIT         0x10
#define DIV_APU_DOUBLESPEED_BIT         0x20

#define APU_ENABLE_BIT                  0x80

#define MASTER_VOLUME_RIGHT             0x07
#define MASTER_VOLUME_LEFT              0x70

#define PANNING_RIGHT_CH1               0
#define PANNING_RIGHT_CH2               1
#define PANNING_RIGHT_CH3               2
#define PANNING_RIGHT_CH4               3
#define PANNING_LEFT_CH1                4
#define PANNING_LEFT_CH2                5
#define PANNING_LEFT_CH3                6
#define PANNING_LEFT_CH4                7

#define APU_DIV_BIT_SINGLESPEED          0x10        // bit 4
#define APU_DIV_BIT_DOUBLESPEED          0x20        // bit 5

#define ENVELOPE_SWEEP_TICK_RATE        8
#define SOUND_LENGTH_TICK_RATE          2
#define CH1_FREQU_SWEEP_RATE            4

#define CH1_SWEEP_PACE                  0x70
#define CH1_SWEEP_DIR                   0x08
#define CH1_SWEEP_STEP                  0x07

#define CH_1_2_4_LENGTH_TIMER             0x3F
#define CH_1_2_DUTY_CYCLE               0xC0

#define CH_1_2_4_ENV_VOLUME             0xF0
#define CH_1_2_4_ENV_DIR                0x08
#define CH_1_2_4_ENV_PACE               0x07

#define CH_1_2_PERIOD_LOW               0xFF
#define CH_1_2_3_PERIOD_HIGH            0x07

#define CH_1_2_3_4_CTRL_TRIGGER         0x80
#define CH_1_2_3_4_CTRL_LENGTH_EN       0x40

#define CH_1_2_TIMER_OVERFLOW           0x40

#define CH_1_2_3_PERIOD_FLIP            0x800
#define CH_1_2_PERIOD_CLOCK             0x20000
#define CH_1_2_PERIOD_THRESHOLD         0x7FF

#define CH_LENGTH_TIMER_THRESHOLD       64

#define CH_3_DAC                        0x80

#define CH_3_VOLUME                     0x60

#define CH_3_WAVERAM_BUFFER_SIZE        8192

#define CH_3_MAX_SAMPLING_RATE          (BASE_CLOCK_CPU / 2)

#define CH_4_CLOCK_SHIFT                0xF0
#define CH_4_LFSR_WIDTH                 0x08
#define CH_4_CLOCK_DIVIDER              0x07

#define CH_4_LFSR_BIT_7                 0x0080
#define CH_4_LFSR_BIT_15                0x8000

#define CH_1_ENABLE                     0x01
#define CH_2_ENABLE                     0x02
#define CH_3_ENABLE                     0x04
#define CH_4_ENABLE                     0x08

#define CH_4_LFSR_INIT_VALUE            0xFFFF

#define CH_4_LFSR_MAX_SAMPL_RATE        (pow(2, 18) / (.5f * pow(2, 0)))

#define CH_4_LFSR_BUFFER_SIZE           16384

#define CH_4_LFSR_TICKS_PER_APU_TICK    ((int)(BASE_CLOCK_CPU / APU_BASE_CLOCK))

/* ***********************************************************************************************************
    REGISTERS INITIAL STATES
*********************************************************************************************************** */
// initial register values (CGB in CGB and DMG mode)
#define INIT_CGB_AF                     0x1180
#define INIT_CGB_BC                     0x0000
#define INIT_CGB_DE                     0xFF56
#define INIT_CGB_HL                     0x000D

#define INIT_DMG_AF                     0x0100
#define INIT_DMG_BC                     0x0014
#define INIT_DMG_DE                     0x0000
#define INIT_DMG_HL                     0xC060

#define INIT_SP                         0xFFFE
#define INIT_PC                         0x0100

// misc registers (IO)
// ISR
#define INIT_CGB_IE                     0x00
#define INIT_CGB_IF                     0xE1

// graphics
#define INIT_BGP                        0xFC
#define INIT_OBP0                       0xFF
#define INIT_OBP1                       0xFF

// bank selects
#define INIT_WRAM_BANK                  0x01
#define INIT_VRAM_BANK                  0x00

// initial memory states
#define INIT_CGB_LCDC                   0x91
#define INIT_CGB_STAT                   0x81
#define INIT_CGB_LY                     0x90

// timer/divider
#define INIT_DIV                        0xABCC
#define INIT_CGB_DIV                    0x1EA0

#define INIT_TAC                        0xF8
#define INIT_TIMA                       0x00
#define INIT_TMA                        0x00

/* ***********************************************************************************************************
    TIMERS
*********************************************************************************************************** */
#define TIMA_DIV_BIT_9                  0x0200
#define TIMA_DIV_BIT_3                  0x0008
#define TIMA_DIV_BIT_5                  0x0020
#define TIMA_DIV_BIT_7                  0x0080

/* ***********************************************************************************************************
    DMA
*********************************************************************************************************** */
#define VRAM_DMA_MC_PER_BLOCK           8

/* ***********************************************************************************************************
    HRADWARE DEFINES (HEADER)
*********************************************************************************************************** */
#define RAM_BANK_NUM_0_VAL              0
#define RAM_BANK_NUM_1_VAL              2
#define RAM_BANK_NUM_4_VAL              3
#define RAM_BANK_NUM_8_VAL              5
#define RAM_BANK_NUM_16_VAL             4

#define RAM_BANK_MASK_0_1               0x00
#define RAM_BANK_MASK_4                 0x03
#define RAM_BANK_MASK_8                 0x07
#define RAM_BANK_MASK_16                0x0F

namespace Emulation {
    namespace Gameboy {
        enum MEM_TYPE : int {
            ROM0 = 0,
            ROMn = 1,
            VRAM = 2,
            RAMn = 3,
            WRAM0 = 4,
            WRAMn = 5,
            OAM = 6,
            IO = 7,
            HRAM = 8,
            IE = 9
        };

        /* ***********************************************************************************************************
            FILES
        *********************************************************************************************************** */
        inline const std::string DMG_BOOT_ROM = "dmg_boot.bin";
        inline const std::string CGB_BOOT_ROM = "cgb_boot.bin";
    }
}