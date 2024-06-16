#include "GameboyAPU.h"
#include "gameboy_defines.h"

#include <mutex>

using namespace std;

const float CH_1_2_PWM_SIGNALS[4][8] = {
	{1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, -1.f},
	{-1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, -1.f},
	{-1.f, 1.f, 1.f, 1.f, 1.f, -1.f, -1.f, -1.f},
	{1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, 1.f},
};


void GameboyAPU::ProcessAPU(const int& _ticks) {
	if (soundCtx->apuEnable) {
		for (int i = 0; i < _ticks; i++) {
			envelopeSweepCounter++;
			soundLengthCounter++;
			ch1SamplingRateCounter++;

			if (envelopeSweepCounter == ENVELOPE_SWEEP_TICK_RATE) {
				envelopeSweepCounter = 0;
				
				if (soundCtx->ch1Enable.load()) {
					ch1EnvelopeSweep();
				}
				if (soundCtx->ch2Enable.load()) {
					ch2EnvelopeSweep();
				}
				if (soundCtx->ch4Enable.load()) {
					ch4EnvelopeSweep();
				}
			}

			if (soundLengthCounter == SOUND_LENGTH_TICK_RATE) {
				soundLengthCounter = 0;
				
				if (soundCtx->ch1Enable.load()) {
					ch1TickLengthTimer();
				}
				if (soundCtx->ch2Enable.load()) {
					ch2TickLengthTimer();
				}
				if (soundCtx->ch3Enable.load()) {
					ch3TickLengthTimer();
				}
				if (soundCtx->ch4Enable.load()) {
					ch4TickLengthTimer();
				}
			}

			if (ch1SamplingRateCounter == CH1_FREQU_SWEEP_RATE) {
				ch1SamplingRateCounter = 0;

				if (soundCtx->ch1Enable.load()) {
					// frequency sweep
					ch1PeriodSweep();
				}
			}
		}

		TickLFSR(_ticks);
	}
}

void GameboyAPU::TickLFSR(const int& _ticks) {
	if (soundCtx->ch4Enable.load()) {
		for (int i = 0; i < _ticks * CH_4_LFSR_TICKS_PER_APU_TICK; i++) {
			ch4LFSRTickCounter += soundCtx->ch4LFSRStep;

			if (ch4LFSRTickCounter > 1.f) {
				ch4LFSRTickCounter -= 1.f;

				// tick LFSR
				u16 lfsr = soundCtx->ch4LFSR;
				int next = (lfsr & 0x01) ^ ((lfsr >> 1) & 0x01);

				switch (soundCtx->ch4LFSRWidth7Bit) {
				case true:
					lfsr = (lfsr & ~(CH_4_LFSR_BIT_7 | CH_4_LFSR_BIT_15)) | ((next << 7) | (next << 15));
					break;
				case false:
					lfsr = (lfsr & ~CH_4_LFSR_BIT_15) | (next << 15);
					break;
				}

				lfsr >>= 1;

				int write_cursor = ch4WriteCursor.load();

				if (write_cursor != ch4ReadCursor.load()) {
					unique_lock<mutex> lock_lfsr_buffer(mutLFSR);
					switch (lfsr & 0x0001) {
					case 0x0000:
						ch4LFSRSamples[write_cursor] = -1.f;
						break;
					case 0x0001:
						ch4LFSRSamples[write_cursor] = 1.f;
						break;
					}
					lock_lfsr_buffer.unlock();

					++write_cursor %= CH_4_LFSR_BUFFER_SIZE;
					ch4WriteCursor.store(write_cursor);
				}

				soundCtx->ch4LFSR = lfsr;

				//if(ch4LFSRSamples[ch4LFSRWriteCursor] != .0f)
				//LOG_INFO(ch4LFSRSamples[ch4LFSRWriteCursor], ";", lfsr);
			}
		}
	}
}

void GameboyAPU::ch1PeriodSweep() {
	if (soundCtx->ch1SweepPace != 0) {
		ch1PeriodSweepCounter++;

		if (ch1PeriodSweepCounter >= soundCtx->ch1SweepPace) {
			ch1PeriodSweepCounter = 0;

			bool writeback = true;

			int period = soundCtx->ch1Period;
			switch (soundCtx->ch1SweepDirSubtract) {
			case true:
				period = soundCtx->ch1Period - (soundCtx->ch1Period >> soundCtx->ch1SweepPeriodStep);

				if (period < 1) {
					writeback = false;
				}
				break;
			case false:
				period = soundCtx->ch1Period + (soundCtx->ch1Period >> soundCtx->ch1SweepPeriodStep);

				if (period > CH_1_2_3_PERIOD_FLIP - 1) {
					writeback = false;
					soundCtx->ch1Enable.store(false);
					memInstance->GetIO(NR52_ADDR) &= ~CH_1_ENABLE;
				}
				break;
			}

			if (writeback) {
				memInstance->GetIO(NR13_ADDR) = period & CH_1_2_PERIOD_LOW;
				u8& nr14 = memInstance->GetIO(NR14_ADDR);
				nr14 = (nr14 & ~CH_1_2_3_PERIOD_HIGH) | ((period >> 8) & CH_1_2_3_PERIOD_HIGH);

				soundCtx->ch1Period = period;
				soundCtx->ch1SamplingRate.store((float)(pow(2, 20) / (CH_1_2_3_PERIOD_FLIP - soundCtx->ch1Period)));
				//LOG_INFO("sweep: fs = ", soundCtx->ch1SamplingRate.load() / pow(2, 3), "; dir: ", soundCtx->ch1SweepDirSubtract ? "sub" : "add");
			}
		}
	}
}

void GameboyAPU::ch1TickLengthTimer() {
	if (soundCtx->ch1LengthEnable) {
		if (soundCtx->ch1LengthAltered) {
			ch1LengthCounter = soundCtx->ch1LengthTimer;

			soundCtx->ch1LengthAltered = false;
		}

		ch1LengthCounter++;
		if (ch1LengthCounter == CH_LENGTH_TIMER_THRESHOLD) {
			soundCtx->ch1Enable.store(false);
			memInstance->GetIO(NR52_ADDR) &= ~CH_1_ENABLE;
		}
		//LOG_INFO("length: l = ", ch1LengthCounter, "; ch on: ", soundCtx->ch1Enable ? "true" : "false");
	}
}

void GameboyAPU::ch1EnvelopeSweep() {
	if (soundCtx->ch1EnvelopePace != 0) {
		ch1EnvelopeSweepCounter++;
		if (ch1EnvelopeSweepCounter >= soundCtx->ch1EnvelopePace) {
			ch1EnvelopeSweepCounter = 0;

			switch (soundCtx->ch1EnvelopeIncrease) {
			case true:
				if (soundCtx->ch1EnvelopeVolume < 0xF) { 
					soundCtx->ch1EnvelopeVolume++; 
					soundCtx->ch1Volume.store((float)soundCtx->ch1EnvelopeVolume / 0xF);
				}
				break;
			case false:
				if (soundCtx->ch1EnvelopeVolume > 0x0) { 
					--soundCtx->ch1EnvelopeVolume;
					soundCtx->ch1Volume.store((float)soundCtx->ch1EnvelopeVolume / 0xF);
				}
				break;
			}
			//LOG_INFO("envelope: x = ", soundCtx->ch1EnvelopeVolume);
		}
	}
}

void GameboyAPU::ch2TickLengthTimer() {
	if (soundCtx->ch2LengthEnable) {
		if (soundCtx->ch2LengthAltered) {
			ch2LengthCounter = soundCtx->ch2LengthTimer;

			soundCtx->ch2LengthAltered = false;
		}

		ch2LengthCounter++;
		if (ch2LengthCounter == CH_LENGTH_TIMER_THRESHOLD) {
			soundCtx->ch2Enable.store(false);
			memInstance->GetIO(NR52_ADDR) &= ~CH_2_ENABLE;
		}
		//LOG_INFO("length: l = ", ch1LengthCounter, "; ch on: ", soundCtx->ch1Enable ? "true" : "false");
	}
}

void GameboyAPU::ch2EnvelopeSweep() {
	if (soundCtx->ch2EnvelopePace != 0) {
		ch2EnvelopeSweepCounter++;
		if (ch2EnvelopeSweepCounter >= soundCtx->ch2EnvelopePace) {
			ch2EnvelopeSweepCounter = 0;

			switch (soundCtx->ch2EnvelopeIncrease) {
			case true:
				if (soundCtx->ch2EnvelopeVolume < 0xF) {
					soundCtx->ch2EnvelopeVolume++;
					soundCtx->ch2Volume.store((float)soundCtx->ch2EnvelopeVolume / 0xF);
				}
				break;
			case false:
				if (soundCtx->ch2EnvelopeVolume > 0x0) {
					--soundCtx->ch2EnvelopeVolume;
					soundCtx->ch2Volume.store((float)soundCtx->ch2EnvelopeVolume / 0xF);
				}
				break;
			}
			//LOG_INFO("envelope: x = ", soundCtx->ch1EnvelopeVolume);
		}
	}
}

void GameboyAPU::ch3TickLengthTimer() {
	if (soundCtx->ch3LengthEnable) {
		if (soundCtx->ch3LengthAltered) {
			ch3LengthCounter = soundCtx->ch3LengthTimer;

			soundCtx->ch3LengthAltered = false;
		}

		ch3LengthCounter++;
		if (ch3LengthCounter == CH_LENGTH_TIMER_THRESHOLD << 1) {
			soundCtx->ch3Enable.store(false);
			memInstance->GetIO(NR52_ADDR) &= ~CH_3_ENABLE;
		}
		//LOG_INFO("length: l = ", ch1LengthCounter, "; ch on: ", soundCtx->ch1Enable ? "true" : "false");
	}
}

void GameboyAPU::ch4EnvelopeSweep() {
	if (soundCtx->ch4EnvelopePace != 0) {
		ch4EnvelopeSweepCounter++;
		if (ch4EnvelopeSweepCounter >= soundCtx->ch4EnvelopePace) {
			ch4EnvelopeSweepCounter = 0;

			switch (soundCtx->ch4EnvelopeIncrease) {
			case true:
				if (soundCtx->ch4EnvelopeVolume < 0xF) {
					soundCtx->ch4EnvelopeVolume++;
					soundCtx->ch4Volume.store((float)soundCtx->ch4EnvelopeVolume / 0xF);
				}
				break;
			case false:
				if (soundCtx->ch4EnvelopeVolume > 0x0) {
					--soundCtx->ch4EnvelopeVolume;
					soundCtx->ch4Volume.store((float)soundCtx->ch4EnvelopeVolume / 0xF);
				}
				break;
			}
			//LOG_INFO("envelope: x = ", soundCtx->ch1EnvelopeVolume);
		}
	}
}

void GameboyAPU::ch4TickLengthTimer() {
	if (soundCtx->ch4LengthEnable) {
		if (soundCtx->ch4LengthAltered) {
			ch4LengthCounter = soundCtx->ch4LengthTimer;

			soundCtx->ch4LengthAltered = false;
		}

		ch4LengthCounter++;
		if (ch4LengthCounter == CH_LENGTH_TIMER_THRESHOLD) {
			soundCtx->ch4Enable.store(false);
			memInstance->GetIO(NR52_ADDR) &= ~CH_4_ENABLE;
		}
		//LOG_INFO("length: l = ", ch1LengthCounter, "; ch on: ", soundCtx->ch1Enable ? "true" : "false");
	}
}

// filles each vector with samples of the virtual channels and sets the virtual sampling rates for each channel (power of 2)
/* sample order :
* 1. front right
* 2. rear right
* 3. rear left
* 4. front left
*/
void GameboyAPU::SampleAPU(std::vector<std::vector<std::complex<float>>>& _data, const int& _samples, const int& _sampling_rate) {
	bool right = soundCtx->outRightEnabled.load();
	bool left = soundCtx->outLeftEnabled.load();
	bool vol_right = soundCtx->masterVolumeRight.load();
	bool vol_left = soundCtx->masterVolumeLeft.load();

	// channel 1 & 2
	bool ch1_enable = soundCtx->ch1Enable.load();
	float ch1_virt_sample_step = soundCtx->ch1SamplingRate.load() / _sampling_rate;
	int ch1_wave_index = soundCtx->ch1DutyCycleIndex.load();
	bool ch1_right = soundCtx->ch1Right.load();
	bool ch1_left = soundCtx->ch1Left.load();
	float ch1_vol = soundCtx->ch1Volume.load();

	float ch1_amp;
	if (ch1_left || ch1_right) {
		if (ch1_left && ch1_right) {
			ch1_amp = 1.f;
		} else {
			ch1_amp = 2.f;
		}
	}

	bool ch2_enable = soundCtx->ch2Enable.load();
	float ch2_virt_sample_step = soundCtx->ch2SamplingRate.load() / _sampling_rate;
	int ch2_wave_index = soundCtx->ch2DutyCycleIndex.load();
	bool ch2_right = soundCtx->ch2Right.load();
	bool ch2_left = soundCtx->ch2Left.load();
	float ch2_vol = soundCtx->ch2Volume.load();

	float ch2_amp;
	if (ch2_left || ch2_right) {
		if (ch2_left && ch2_right) {
			ch2_amp = 1.f;
		} else {
			ch2_amp = 2.f;
		}
	}

	bool ch3_enable = soundCtx->ch3Enable.load();
	float ch3_virt_sample_step = soundCtx->ch3SamplingRate.load() / _sampling_rate;
	bool ch3_right = soundCtx->ch3Right.load();
	bool ch3_left = soundCtx->ch3Left.load();
	float ch3_vol = soundCtx->ch3Volume.load();

	float ch3_amp;
	if (ch3_left || ch3_right) {
		if (ch3_left && ch3_right) {
			ch3_amp = 1.f;
		} else {
			ch3_amp = 2.f;
		}
	}

	float ch4_virt_sample_step = soundCtx->ch4SamplingRate.load() / _sampling_rate;
	bool ch4_right = soundCtx->ch4Right.load();
	bool ch4_left = soundCtx->ch4Left.load();
	float ch4_vol = soundCtx->ch4Volume.load();

	float ch4_amp;

	if (ch4_left || ch4_right) {
		if (ch4_left && ch4_right) {
			ch4_amp = 1.f;
		} else {
			ch4_amp = 2.f;
		}
	}

	unique_lock<mutex> lock_wave_ram(soundCtx->mutWaveRam, std::defer_lock);
	unique_lock<mutex> lock_lfsr_buffer(mutLFSR, std::defer_lock);

	for (int i = 0; i < _samples; i++) {
		float sample_0 = .0f;	// front-left
		float sample_1 = .0f;	// front-right
		float sample_2 = .0f;	// rear-left
		float sample_3 = .0f;	// rear-right

		if (ch1_enable) {
			ch1VirtSamples += ch1_virt_sample_step;
			while (ch1VirtSamples > 1.f) {
				ch1VirtSamples -= 1.f;
				++ch1SampleCount %= 8;
			}


			if (ch1_right) {
				sample_1 += CH_1_2_PWM_SIGNALS[ch1_wave_index][ch1SampleCount] * ch1_vol * ch1_amp;
			}
			if (ch1_left) {
				sample_0 += CH_1_2_PWM_SIGNALS[ch1_wave_index][ch1SampleCount] * ch1_vol * ch1_amp;
			}
		}

		if (ch2_enable) {
			ch2VirtSamples += ch2_virt_sample_step;
			while (ch2VirtSamples > 1.f) {
				ch2VirtSamples -= 1.f;
				++ch2SampleCount %= 8;
			}

			if (ch2_right) {
				sample_1 += CH_1_2_PWM_SIGNALS[ch2_wave_index][ch2SampleCount] * ch2_vol * ch2_amp;
			}
			if (ch2_left) {
				sample_0 += CH_1_2_PWM_SIGNALS[ch2_wave_index][ch2SampleCount] * ch2_vol * ch2_amp;
			}
		}

		if (ch3_enable) {
			ch3VirtSamples += ch3_virt_sample_step;
			while (ch3VirtSamples > 1.f) {
				ch3VirtSamples -= 1.f;
				++ch3SampleCount %= 32;
			}

			lock_wave_ram.lock();
			if (ch3_right) {
				sample_3 += soundCtx->waveRam[ch3SampleCount] * ch3_vol * ch3_amp;
			}
			if (ch3_left) {
				sample_2 += soundCtx->waveRam[ch3SampleCount] * ch3_vol * ch3_amp;
			}
			lock_wave_ram.unlock();
		}

		{
			lock_lfsr_buffer.lock();
			ch4VirtSamples += ch4_virt_sample_step;
			while (ch4VirtSamples > 1.f) {
				ch4VirtSamples -= 1.f;

				int read_cursor = ch4ReadCursor.load();

				// (0 - 1) % 10 = -1 % 10 and is defined as -1 * 10 + 9, where the added value is the remainder (remainder is by definition always positiv)
				if (read_cursor != (ch4WriteCursor.load() - 1) % CH_4_LFSR_BUFFER_SIZE) {
					ch4LFSRSamples[read_cursor] = .0f;
					++read_cursor %= CH_4_LFSR_BUFFER_SIZE;
					ch4ReadCursor.store(read_cursor);
				}
			}

			if (ch4_right) {
				sample_3 += ch4LFSRSamples[ch4ReadCursor] * ch4_vol * ch4_amp;
			}
			if (ch4_left) {
				sample_2 += ch4LFSRSamples[ch4ReadCursor] * ch4_vol * ch4_amp;
			}
			lock_lfsr_buffer.unlock();
		}

		_data[0][i].real(sample_1 * vol_right * .05f);		// front-right
		_data[1][i].real(sample_3 * vol_right * .05f);		// rear-right
		_data[2][i].real(sample_2 * vol_left * .05f);		// rear-left
		_data[3][i].real(sample_0 * vol_left * .05f);		// front-left
	}
}