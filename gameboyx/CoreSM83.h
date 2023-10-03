#pragma once

#include "CoreBase.h"
#include "Cartridge.h"
#include "registers.h"
#include "defs.h"
#include "MemorySM83.h"

#include <vector>

/* ***********************************************************************************************************
	CLASSES FOR INSTRUCTION IN/OUTPUT POINTERS
*********************************************************************************************************** */
enum cgb_data_types {
	NO_DATA,
	BC,
	BC_ref,
	DE,
	DE_ref,
	HL,
	HL_ref,
	HL_INC_ref,
	HL_DEC_ref,
	SP,
	SP_r8,
	PC,
	AF,
	A,
	F,
	B,
	C,
	C_ref,
	D,
	E,
	H,
	L,
	d8,
	d16,
	a8,
	a8_ref,
	a16,
	a16_ref,
	r8
};

/* ***********************************************************************************************************
	CoreSM83 CLASS DECLARATION
*********************************************************************************************************** */
class CoreSM83 : protected CoreBase
{
public:
	friend class CoreBase;

	void RunCycles() override;
	void GetCurrentHardwareState(message_buffer& _msg_buffer) const override;
	void GetStartupHardwareInfo(message_buffer& _msg_buffer) const override;
	bool CheckNextFrame() override;
	u32 GetPassedClockCycles() override;
	int GetDisplayFrequency() const override;

	void GetCurrentMemoryLocation(message_buffer& _msg_buffer) const override;
	void InitMessageBufferProgram(std::vector<std::vector<std::tuple<int, int, std::string, std::string>>>& _program_buffer) override;
	void GetCurrentRegisterValues(std::vector<std::pair<std::string, std::string>>& _register_values) const  override;

private:
	// constructor
	CoreSM83(message_buffer& _msg_buffer);
	// destructor
	~CoreSM83() = default;

	// instruction data
	u8 opcode;
	u16 data;

	void RunCpu() override;
	void ExecuteInstruction() override;
	void ExecuteInterrupts() override;
	void ExecuteMachineCycles() override;

	u16 curPC;

	// internals
	gbc_registers Regs = gbc_registers();
	void InitCpu(const Cartridge& _cart_obj) override;
	void InitRegisterStates() override;

	// cpu states and checks
	bool halted = false;
	bool ime = false;
	bool opcodeCB = false;

	// ISR
	void isr_push(const u8& _isr_handler);

	// instruction members ****************
	// lookup table
	typedef void (CoreSM83::* instruction)();

	// current instruction context
	using instr_tuple = std::tuple < const u8, const instruction, const int, const std::string, const cgb_data_types, const cgb_data_types>;
	instr_tuple* instrPtr = nullptr;
	instruction functionPtr = nullptr;
	int machineCycles = 0;
	int currentMachineCycles = 0;
	int GetDelayTime() override;
	void WriteMessageBufferRomBank(std::vector<std::tuple<int, int, std::string, std::string>>& _program_buffer_bank, const int& _bank, const std::vector<u8>& _rom_bank);

	machine_state_context* machine_ctx;

	// basic instructions
	std::vector<instr_tuple> instrMap;
	void setupLookupTable();

	// CB instructions
	std::vector<instr_tuple> instrMapCB;
	void setupLookupTableCB();
	 
	// basic instruction set *****
	void NoInstruction();
	
	// cpu control
	void NOP();
	void STOP();
	void HALT();
	void CCF();
	void SCF();
	void DI();
	void EI();
	void CB();

	// load
	void LDfromAtoRef();
	void LDtoAfromRef();

	void LDd8();
	void LDd16();

	void LDCref();
	void LDH();
	void LDHa16();
	void LDSPa16();
	void LDSPHL();

	void LDtoB();
	void LDtoC();
	void LDtoD();
	void LDtoE();
	void LDtoH();
	void LDtoL();
	void LDtoHLref();
	void LDtoA();

	void LDHLSPr8();

	void PUSH();
	void POP();

	// arithmetic/logic
	void INC8();
	void INC16();
	void DEC8();
	void DEC16();
	void ADD8();
	void ADDHL();
	void ADDSPr8();
	void ADC();
	void SUB();
	void SBC();
	void DAA();
	void AND();
	void OR();
	void XOR();
	void CP();
	void CPL();

	// rotate and shift
	void RLCA();
	void RRCA();
	void RLA();
	void RRA();

	// jump
	void JP();
	void JR();
	void CALL();
	void RST();
	void RET();
	void RETI();

	// instruction helpers
	void jump_jp();
	void jump_jr();
	void call();
	void ret();

	// CB instruction set *****
	// shift/rotate
	void RLC();
	void RRC();
	void RL();
	void RR();
	void SLA();
	void SRA();
	void SWAP();
	void SRL();

	// bit test
	void BIT0();
	void BIT1();
	void BIT2();
	void BIT3();
	void BIT4();
	void BIT5();
	void BIT6();
	void BIT7();

	// reset bit
	void RES0();
	void RES1();
	void RES2();
	void RES3();
	void RES4();
	void RES5();
	void RES6();
	void RES7();

	// set bit
	void SET0();
	void SET1();
	void SET2();
	void SET3();
	void SET4();
	void SET5();
	void SET6();
	void SET7();
};