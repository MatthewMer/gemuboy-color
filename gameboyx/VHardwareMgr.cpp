#include "VHardwareMgr.h"

#include "logger.h"


using namespace std;

/* ***********************************************************************************************************
    (DE)INIT
*********************************************************************************************************** */
VHardwareMgr* VHardwareMgr::instance = nullptr;

VHardwareMgr* VHardwareMgr::getInstance() {
    if (instance == nullptr) {
        instance = new VHardwareMgr();
    }

    return instance;
}

void VHardwareMgr::resetInstance() {
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }
}

VHardwareMgr::~VHardwareMgr() {
    BaseCPU::resetInstance();
    BaseCTRL::resetInstance();
    BaseGPU::resetInstance();
    BaseAPU::resetInstance();
}

/* ***********************************************************************************************************
    RUN HARDWARE
*********************************************************************************************************** */
u8 VHardwareMgr::InitHardware(BaseCartridge* _cartridge, virtual_graphics_settings& _virt_graphics_settings, emulation_settings& _emu_settings) {
    errors = 0x00;

    if (_cartridge == nullptr) {
        errors |= VHWMGR_ERR_CART_NULL;
    } else {
        if (_cartridge->ReadRom()) {
            cart_instance = _cartridge;

            core_instance = BaseCPU::getInstance(cart_instance);
            graphics_instance = BaseGPU::getInstance(cart_instance, _virt_graphics_settings);
            sound_instance = BaseAPU::getInstance(cart_instance);
            control_instance = BaseCTRL::getInstance(cart_instance);

            if (cart_instance != nullptr &&
                core_instance != nullptr &&
                graphics_instance != nullptr &&
                sound_instance != nullptr &&
                control_instance != nullptr) {

                core_instance->SetInstances();
                mmu_instance = BaseMMU::getInstance();
                memory_instance = BaseMEM::getInstance();

                // returns the time per frame in ns
                timePerFrame = graphics_instance->GetDelayTime();

                InitMembers(_emu_settings);
                buffering = (int)_virt_graphics_settings.buffering;

                hardwareThread = thread([this]() -> void { ProcessHardware(); });
                if (!hardwareThread.joinable()) {
                    errors |= VHWMGR_ERR_INIT_THREAD;
                } else {
                    LOG_INFO("[emu] hardware for ", cart_instance->title, " initialized");
                }
            } else {
                errors |= VHWMGR_ERR_INIT_HW;
            }
        } else {
            errors |= VHWMGR_ERR_READ_ROM;
            _cartridge->ClearRom();
        }
    }

    if (errors) {
        string title;
        if (errors | VHWMGR_ERR_CART_NULL) {
            title = N_A;
        } else {
            title = cart_instance->title;
        }
        LOG_ERROR("[emu] starting game ", title);
        resetInstance();
    }

    return errors;
}

void VHardwareMgr::ShutdownHardware() {
    running.store(false);
    if (hardwareThread.joinable()) {
        hardwareThread.join();
    }

    BaseCTRL::resetInstance();
    BaseAPU::resetInstance();
    BaseGPU::resetInstance();
    BaseCPU::resetInstance();
    
    string title;
    if (cart_instance == nullptr) {
        title = N_A;
    } else {
        title = cart_instance->title;
    }

    LOG_INFO("[emu] hardware for ", title, " stopped");
}

u8 VHardwareMgr::ResetHardware() {
    errors = 0x00;



    return errors;
}

void VHardwareMgr::ProcessHardware() {
    unique_lock<mutex> lock_hardware(mutHardware, defer_lock);
    unique_lock<mutex> lock_keys(mutKeys, defer_lock);
  
    while (running.load()) {
        if (next.load()) {
            for (int i = 0; i < buffering; i++) {
                if (debugEnable.load()) {

                    if (!pauseExecution.load()) {
                        lock_keys.lock();
                        ProcessKeys();
                        lock_keys.unlock();

                        lock_hardware.lock();
                        core_instance->RunCycle();
                        lock_hardware.unlock();

                        pauseExecution.store(true);
                    }
                } else if (CheckDelay()) {
                    for (int j = 0; j < emulationSpeed.load(); j++) {
                        lock_keys.lock();
                        ProcessKeys();
                        lock_keys.unlock();

                        lock_hardware.lock();
                        core_instance->RunCycles();
                        lock_hardware.unlock();
                    }
                }
                CheckFpsAndClock();
            }
            next.store(false);
        }
    }
}

bool VHardwareMgr::CheckDelay() {
    timeFrameCur = high_resolution_clock::now();
    currentTimePerFrame = (u32)duration_cast<milliseconds>(timeFrameCur - timeFramePrev).count();

    if (currentTimePerFrame >= timePerFrame) {
        timeFramePrev = timeFrameCur;
        currentTimePerFrame = 0;

        return true;
    } else {
        return false;
    }
}

void VHardwareMgr::InitMembers(emulation_settings& _settings) {
    timeFramePrev = high_resolution_clock::now();
    timeFrameCur = high_resolution_clock::now();
    timeSecondPrev = high_resolution_clock::now();
    timeSecondCur = high_resolution_clock::now();

    running.store(true);
    debugEnable.store(_settings.debug_enabled);
    pauseExecution.store(_settings.pause_execution);
    emulationSpeed.store(_settings.emulation_speed);
    next.store(false);
}

void VHardwareMgr::CheckFpsAndClock() {
    timeSecondCur = high_resolution_clock::now();
    accumulatedTime += (u32)duration_cast<microseconds>(timeSecondCur - timeSecondPrev).count();
    timeSecondPrev = timeSecondCur;

    if (accumulatedTime > 1000000) {
        currentFrequency = ((float)core_instance->GetCurrentClockCycles() / accumulatedTime);
        currentFramerate = graphics_instance->GetFrames() / (accumulatedTime / (float)pow(10, 6));

        accumulatedTime = 0;
    }
}

void VHardwareMgr::ProcessKeys() {
    while (!keyMap.empty()) {
        pair<SDL_Keycode, SDL_EventType>& input = keyMap.front();

        switch (input.second) {
        case SDL_KEYDOWN:
            control_instance->SetKey(input.first);
            break;
        case SDL_KEYUP:
            control_instance->ResetKey(input.first);
            break;
        }

        keyMap.pop();
    }
}

/* ***********************************************************************************************************
    External interaction (Getters/Setters)
*********************************************************************************************************** */
void VHardwareMgr::GetFpsAndClock(int& _fps, float& _clock) {
    //unique_lock<mutex> lock_hardware(mutHardware);
    _clock = currentFrequency;
    _fps = (int)currentFramerate;
}

void VHardwareMgr::GetCurrentPCandBank(int& _pc, int& _bank) {
    //unique_lock<mutex> lock_hardware(mutHardware);
    core_instance->GetCurrentPCandBank(_pc, _bank);
}

void VHardwareMgr::EventKeyDown(SDL_Keycode& _key) {
    unique_lock<mutex> lock_keys(mutKeys);
    keyMap.push(pair(_key, SDL_KEYDOWN));
}

void VHardwareMgr::EventKeyUp(SDL_Keycode& _key) {
    unique_lock<mutex> lock_keys(mutKeys);
    keyMap.push(pair(_key, SDL_KEYUP));
}

void VHardwareMgr::SetDebugEnabled(const bool& _debug_enabled) {
    debugEnable.store(_debug_enabled);
}

void VHardwareMgr::SetPauseExecution(const bool& _pause_execution) {
    pauseExecution.store(_pause_execution);
}

void VHardwareMgr::SetEmulationSpeed(const int& _emulation_speed) {
    emulationSpeed.store(_emulation_speed);
}

void VHardwareMgr::Next() {
    next.store(true);
}

void VHardwareMgr::GetInstrDebugTable(Table<instr_entry>& _table) {
    //unique_lock<mutex> lock_hardware(mutHardware);
    core_instance->GetInstrDebugTable(_table);
}

void VHardwareMgr::GetInstrDebugTableTmp(Table<instr_entry>& _table) {
    //unique_lock<mutex> lock_hardware(mutHardware);
    core_instance->GetInstrDebugTableTmp(_table);
}

void VHardwareMgr::GetInstrDebugFlags(std::vector<reg_entry>& _reg_values, std::vector<reg_entry>& _flag_values, std::vector<reg_entry>& _misc_values) {
    //unique_lock<mutex> lock_hardware(mutHardware);
    core_instance->GetInstrDebugFlags(_reg_values, _flag_values, _misc_values);
}

void VHardwareMgr::GetHardwareInfo(std::vector<data_entry>& _hardware_info) {
    //unique_lock<mutex> lock_hardware(mutHardware);
    core_instance->GetHardwareInfo(_hardware_info);
}

void VHardwareMgr::GetMemoryDebugTables(std::vector<Table<memory_entry>>& _tables) {
    //unique_lock<mutex> lock_hardware(mutHardware);
    memory_instance->GetMemoryDebugTables(_tables);
}