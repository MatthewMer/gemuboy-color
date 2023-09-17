#pragma once

#include <queue>
#include <string>

// TODO: fifo for debug info (GUI)
struct message_buffer {
	std::string instruction_buffer = "";
	bool instruction_buffer_enabled = false;
	bool pause_execution = true;
	bool auto_run = false;
	bool instruction_buffer_log = true;
};

struct game_status {
	bool game_running = false;
	bool pending_game_start = false;
	bool pending_game_stop = false;
	int game_to_start = 0;
};