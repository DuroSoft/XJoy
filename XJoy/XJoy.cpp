#include "stdafx.h"
#include "Windows.h"
#include "ViGEmClient.h"
#include "hidapi.h"
#include <iostream>
#include <sstream>
#include <string>
#include <csignal>
#include <tuple>
#include <unordered_map>
#include "hidapi_log.h"
#include <vector>
#include <regex>

#pragma comment(lib, "SetupAPI")

bool enable_traffic_dump = false;

const unsigned short NINTENDO = 0x057e;

const unsigned short SWITCH = 0x2000;
const unsigned short JOYCON_L = 0x2006;
const unsigned short JOYCON_R = 0x2007;
const unsigned short PRO_CONTROLLER = 0x2009;
const unsigned short CHARGING_GRIP = 0x200e;

const std::string GRAY = "#828282";
const std::string HORI_RED = "#F62200";
const std::string NEON_RED = "#FF3C28";
const std::string NEON_BLUE = "#0AB9E6";
const std::string NEON_YELLOW = "#E6FF00";
const std::string NEON_GREEN = "#1EDC00";
const std::string NEON_PINK = "#FF3278";
const std::string BLUE = "#4655E6";
const std::string NEON_PURPLE = "#B400F5";
const std::string NEON_ORANGE = "#FAA005";

const std::string ANIMAL_CROSSING_BLUE = "#96F5F5";
const std::string ANIMAL_CROSSING_GREEN = "#82FF92";


const int XBOX_ANALOG_MIN = -32768;
const int XBOX_ANALOG_MAX = 32767;
const int XBOX_ANALOG_DIAG_MAX = round(XBOX_ANALOG_MAX * 0.5 * sqrt(2.0));
const int XBOX_ANALOG_DIAG_MIN = round(XBOX_ANALOG_MIN * 0.5 * sqrt(2.0));

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

u8 timming_byte;

#pragma pack(push, 1)


struct brcm_hdr {
	u8 cmd;
	u8 timer;
	u8 rumble_l[4];
	u8 rumble_r[4];
};

struct brcm_cmd_01 {
	u8 subcmd;
	union {
		struct {
			u32 offset;
			u8 size;
		} spi_data;

		struct {
			u8 arg1;
			u8 arg2;
		} subcmd_arg;

		struct {
			u8  mcu_cmd;
			u8  mcu_subcmd;
			u8  mcu_mode;
		} subcmd_21_21;

		struct {
			u8  mcu_cmd;
			u8  mcu_subcmd;
			u8  no_of_reg;
			u16 reg1_addr;
			u8  reg1_val;
			u16 reg2_addr;
			u8  reg2_val;
			u16 reg3_addr;
			u8  reg3_val;
			u16 reg4_addr;
			u8  reg4_val;
			u16 reg5_addr;
			u8  reg5_val;
			u16 reg6_addr;
			u8  reg6_val;
			u16 reg7_addr;
			u8  reg7_val;
			u16 reg8_addr;
			u8  reg8_val;
			u16 reg9_addr;
			u8  reg9_val;
		} subcmd_21_23_04;

		struct {
			u8  mcu_cmd;
			u8  mcu_subcmd;
			u8  mcu_ir_mode;
			u8  no_of_frags;
			u16 mcu_major_v;
			u16 mcu_minor_v;
		} subcmd_21_23_01;
	};
};

#pragma pack(pop)
#define DATA_BUFFER_SIZE 49
#define OUT_BUFFER_SIZE 49
u8 data[DATA_BUFFER_SIZE];
u16 stick_cal[14];
u8 global_counter[2] = { 0,0 };

bool kill_threads = false;

enum JOYCON_REGION {
  LEFT_DPAD,
  LEFT_ANALOG,
  LEFT_AUX,
  RIGHT_BUTTONS,
  RIGHT_ANALOG,
  RIGHT_AUX
};

enum JOYCON_BUTTON {
  L_DPAD_LEFT = 1,            // left dpad
  L_DPAD_DOWN = 2,
  L_DPAD_UP = 4,
  L_DPAD_RIGHT = 8,
  L_DPAD_SL = 16,
  L_DPAD_SR = 32,
  L_ANALOG_LEFT = 4,          // left 8-way analog
  L_ANALOG_UP_LEFT = 5,
  L_ANALOG_UP = 6,
  L_ANALOG_UP_RIGHT = 7,
  L_ANALOG_RIGHT = 0,
  L_ANALOG_DOWN_RIGHT = 1,
  L_ANALOG_DOWN = 2,
  L_ANALOG_DOWN_LEFT = 3,
  L_ANALOG_NONE = 8,
  L_SHOULDER = 64,            // left aux area
  L_TRIGGER = 128,
  L_CAPTURE = 32,
  L_MINUS = 1,
  L_STICK = 4,
  R_BUT_A = 1,                // right buttons area
  R_BUT_B = 4,
  R_BUT_Y = 8,
  R_BUT_X = 2,
  R_BUT_SL = 16,
  R_BUT_SR = 32,
  R_SHOULDER = 64,            // right aux area
  R_TRIGGER = 128,
  R_HOME = 16,
  R_PLUS = 2,
  R_STICK = 8,
  R_ANALOG_LEFT = 0,          // right 8-way analog
  R_ANALOG_UP_LEFT = 1,
  R_ANALOG_UP = 2,
  R_ANALOG_UP_RIGHT = 3,
  R_ANALOG_RIGHT = 4,
  R_ANALOG_DOWN_RIGHT = 5,
  R_ANALOG_DOWN = 6,
  R_ANALOG_DOWN_LEFT = 7,
  R_ANALOG_NONE = 8
};

std::unordered_map<JOYCON_BUTTON, XUSB_BUTTON> button_mappings;

std::tuple<JOYCON_REGION, JOYCON_BUTTON> string_to_joycon_button(std::string input) {
  if(input == "L_DPAD_LEFT") return std::make_tuple(LEFT_DPAD, L_DPAD_LEFT);
  if(input == "L_DPAD_DOWN") return std::make_tuple(LEFT_DPAD, L_DPAD_DOWN);
  if(input == "L_DPAD_UP") return std::make_tuple(LEFT_DPAD, L_DPAD_UP);
  if(input == "L_DPAD_RIGHT") return std::make_tuple(LEFT_DPAD, L_DPAD_RIGHT);
  if(input == "L_DPAD_SL") return std::make_tuple(LEFT_DPAD, L_DPAD_SL);
  if(input == "L_DPAD_SR") return std::make_tuple(LEFT_DPAD, L_DPAD_SR);
  if(input == "L_ANALOG_LEFT") return std::make_tuple(LEFT_ANALOG, L_ANALOG_LEFT);
  if(input == "L_ANALOG_UP_LEFT") return std::make_tuple(LEFT_ANALOG, L_ANALOG_UP_LEFT);
  if(input == "L_ANALOG_UP") return std::make_tuple(LEFT_ANALOG, L_ANALOG_UP);
  if(input == "L_ANALOG_UP_RIGHT") return std::make_tuple(LEFT_ANALOG, L_ANALOG_UP_RIGHT);
  if(input == "L_ANALOG_RIGHT") return std::make_tuple(LEFT_ANALOG, L_ANALOG_RIGHT);
  if(input == "L_ANALOG_DOWN_RIGHT") return std::make_tuple(LEFT_ANALOG, L_ANALOG_DOWN_RIGHT);
  if(input == "L_ANALOG_DOWN") return std::make_tuple(LEFT_ANALOG, L_ANALOG_DOWN);
  if(input == "L_ANALOG_DOWN_LEFT") return std::make_tuple(LEFT_ANALOG, L_ANALOG_DOWN_LEFT);
  if(input == "L_ANALOG_NONE") return std::make_tuple(LEFT_ANALOG, L_ANALOG_NONE);
  if(input == "L_SHOULDER") return std::make_tuple(LEFT_AUX, L_SHOULDER);
  if(input == "L_TRIGGER") return std::make_tuple(LEFT_AUX, L_TRIGGER);
  if(input == "L_CAPTURE") return std::make_tuple(LEFT_AUX, L_CAPTURE);
  if(input == "L_MINUS") return std::make_tuple(LEFT_AUX, L_MINUS);
  if(input == "L_STICK") return std::make_tuple(LEFT_AUX, L_STICK);
  if(input == "R_BUT_A") return std::make_tuple(RIGHT_BUTTONS, R_BUT_A);
  if(input == "R_BUT_B") return std::make_tuple(RIGHT_BUTTONS, R_BUT_B);
  if(input == "R_BUT_Y") return std::make_tuple(RIGHT_BUTTONS, R_BUT_Y);
  if(input == "R_BUT_X") return std::make_tuple(RIGHT_BUTTONS, R_BUT_X);
  if(input == "R_BUT_SL") return std::make_tuple(RIGHT_BUTTONS, R_BUT_SL);
  if(input == "R_BUT_SR") return std::make_tuple(RIGHT_BUTTONS, R_BUT_SR);
  if(input == "R_SHOULDER") return std::make_tuple(RIGHT_AUX, R_SHOULDER);
  if(input == "R_TRIGGER") return std::make_tuple(RIGHT_AUX, R_TRIGGER);
  if(input == "R_HOME") return std::make_tuple(RIGHT_AUX, R_HOME);
  if(input == "R_PLUS") return std::make_tuple(RIGHT_AUX, R_PLUS);
  if(input == "R_STICK") return std::make_tuple(RIGHT_AUX, R_STICK);
  if(input == "R_ANALOG_LEFT") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_LEFT);
  if(input == "R_ANALOG_UP_LEFT") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_UP_LEFT);
  if(input == "R_ANALOG_UP") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_UP);
  if(input == "R_ANALOG_UP_RIGHT") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_UP_RIGHT);
  if(input == "R_ANALOG_RIGHT") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_RIGHT);
  if(input == "R_ANALOG_DOWN_RIGHT") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_DOWN_RIGHT);
  if(input == "R_ANALOG_DOWN") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_DOWN);
  if(input == "R_ANALOG_DOWN_LEFT") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_DOWN_LEFT);
  if(input == "R_ANALOG_NONE") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_NONE);
  throw "invalid joycon button: " + input;
}

XUSB_BUTTON string_to_xbox_button(std::string input) {
  if(input == "XUSB_GAMEPAD_DPAD_UP") return XUSB_GAMEPAD_DPAD_UP;
  if(input == "XUSB_GAMEPAD_DPAD_DOWN") return XUSB_GAMEPAD_DPAD_DOWN;
  if(input == "XUSB_GAMEPAD_DPAD_LEFT") return XUSB_GAMEPAD_DPAD_LEFT;
  if(input == "XUSB_GAMEPAD_DPAD_RIGHT") return XUSB_GAMEPAD_DPAD_RIGHT;
  if(input == "XUSB_GAMEPAD_START") return XUSB_GAMEPAD_START;
  if(input == "XUSB_GAMEPAD_BACK") return XUSB_GAMEPAD_BACK;
  if(input == "XUSB_GAMEPAD_LEFT_THUMB") return XUSB_GAMEPAD_LEFT_THUMB;
  if(input == "XUSB_GAMEPAD_RIGHT_THUMB") return XUSB_GAMEPAD_RIGHT_THUMB;
  if(input == "XUSB_GAMEPAD_LEFT_SHOULDER") return XUSB_GAMEPAD_LEFT_SHOULDER;
  if(input == "XUSB_GAMEPAD_RIGHT_SHOULDER") return XUSB_GAMEPAD_RIGHT_SHOULDER;
  if(input == "XUSB_GAMEPAD_GUIDE") return XUSB_GAMEPAD_GUIDE;
  if(input == "XUSB_GAMEPAD_A") return XUSB_GAMEPAD_A;
  if(input == "XUSB_GAMEPAD_B") return XUSB_GAMEPAD_B;
  if(input == "XUSB_GAMEPAD_X") return XUSB_GAMEPAD_X;
  if(input == "XUSB_GAMEPAD_Y") return XUSB_GAMEPAD_Y;
  throw "invalid xbox button: " + input;
}

std::string joycon_button_to_string(JOYCON_REGION region, JOYCON_BUTTON button) {
  switch(region) {
    case LEFT_DPAD:
      switch(button) {
        case L_DPAD_LEFT: return "L_DPAD_LEFT";
        case L_DPAD_DOWN: return "L_DPAD_DOWN";
        case L_DPAD_UP: return "L_DPAD_UP";
        case L_DPAD_RIGHT: return "L_DPAD_RIGHT";
        case L_DPAD_SL: return "L_DPAD_SL";
        case L_DPAD_SR: return "L_DPAD_SR";
      }
    case LEFT_ANALOG:
      switch(button) {
        case L_ANALOG_LEFT: return "L_ANALOG_LEFT";
        case L_ANALOG_UP_LEFT: return "L_ANALOG_UP_LEFT";
        case L_ANALOG_UP: return "L_ANALOG_UP";
        case L_ANALOG_UP_RIGHT: return "L_ANALOG_UP_RIGHT";
        case L_ANALOG_RIGHT: return "L_ANALOG_RIGHT";
        case L_ANALOG_DOWN_RIGHT: return "L_ANALOG_DOWN_RIGHT";
        case L_ANALOG_DOWN: return "L_ANALOG_DOWN";
        case L_ANALOG_DOWN_LEFT: return "L_ANALOG_DOWN_LEFT";
        case L_ANALOG_NONE: return "L_ANALOG_NONE";
      }
    case LEFT_AUX:
      switch(button) {
        case L_SHOULDER: return "L_SHOULDER";
        case L_TRIGGER: return "L_TRIGGER";
        case L_CAPTURE: return "L_CAPTURE";
        case L_MINUS: return "L_MINUS";
        case L_STICK: return "L_STICK";
      }
    case RIGHT_BUTTONS:
      switch(button) {
        case R_BUT_A: return "R_BUT_A";
        case R_BUT_B: return "R_BUT_B";
        case R_BUT_Y: return "R_BUT_Y";
        case R_BUT_X: return "R_BUT_X";
        case R_BUT_SL: return "R_BUT_SL";
        case R_BUT_SR: return "R_BUT_SR";
      }
    case RIGHT_AUX:
      switch(button) {
        case R_SHOULDER: return "R_SHOULDER";
        case R_TRIGGER: return "R_TRIGGER";
        case R_HOME: return "R_HOME";
        case R_PLUS: return "R_PLUS";
        case R_STICK: return "R_STICK";
      }
    case RIGHT_ANALOG:
      switch(button) {
        case R_ANALOG_LEFT: return "R_ANALOG_LEFT";
        case R_ANALOG_UP_LEFT: return "R_ANALOG_UP_LEFT";
        case R_ANALOG_UP: return "R_ANALOG_UP";
        case R_ANALOG_UP_RIGHT: return "R_ANALOG_UP_RIGHT";
        case R_ANALOG_RIGHT: return "R_ANALOG_RIGHT";
        case R_ANALOG_DOWN_RIGHT: return "R_ANALOG_DOWN_RIGHT";
        case R_ANALOG_DOWN: return "R_ANALOG_DOWN";
        case R_ANALOG_DOWN_LEFT: return "R_ANALOG_DOWN_LEFT";
        case R_ANALOG_NONE: return "R_ANALOG_NONE";
      }
    default:
      throw "invalid region";
  }
  throw "invalid button";
}

std::string vigem_error_to_string(VIGEM_ERROR err) {
  switch(err) {
    case VIGEM_ERROR_NONE: return "none";
    case VIGEM_ERROR_ALREADY_CONNECTED: return "already connected";
    case VIGEM_ERROR_BUS_ACCESS_FAILED: return "bus access failed";
    case VIGEM_ERROR_BUS_ALREADY_CONNECTED: return "bus already connected";
    case VIGEM_ERROR_BUS_NOT_FOUND: return "bus not found";
    case VIGEM_ERROR_BUS_VERSION_MISMATCH: return "bus version mismatch";
    case VIGEM_ERROR_CALLBACK_ALREADY_REGISTERED: return "callback already registered";
    case VIGEM_ERROR_CALLBACK_NOT_FOUND: return "callback not found";
    case VIGEM_ERROR_INVALID_TARGET: return "invalid target";
    case VIGEM_ERROR_NO_FREE_SLOT: return "no free slot";
    case VIGEM_ERROR_REMOVAL_FAILED: return "removal failed";
    case VIGEM_ERROR_TARGET_NOT_PLUGGED_IN: return "target not plugged in";
    case VIGEM_ERROR_TARGET_UNINITIALIZED: return "target uninitialized";
    default: return "none";
  }
}

void subcomm(hid_device* joycon, u8* in, u8 len, u8 comm, u8 get_response, u8 is_left) {
  u8 buf[OUT_BUFFER_SIZE] = { 0 };
  buf[0] = 0x1;
  buf[1] = global_counter[is_left];
  buf[2] = 0x0;
  buf[3] = 0x1;
  buf[4] = 0x40;
  buf[5] = 0x40;
  buf[6] = 0x0;
  buf[7] = 0x1;
  buf[8] = 0x40;
  buf[9] = 0x40;
  buf[10] = comm;
  for(int i = 0; i < len; ++i) {
    buf[11 + i] = in[i];
  }
  if(global_counter[is_left] == 0xf) global_counter[is_left] = 0;
  else ++global_counter[is_left];
  //for(int i = 0; i < 15; ++i) {
  //  printf("%x ", buf[i]);
  //}
  //printf("\n");
  hid_write(joycon, buf, OUT_BUFFER_SIZE);
  if(get_response) {
    int n = hid_read_timeout(joycon, data, DATA_BUFFER_SIZE, 50);

    /*printf("response: ");
    for(int i = 0; i < 35; ++i) {
      printf("%x ", data[i]);
    }
    printf("\n");

    if(data[14] != comm) {
      printf("subcomm return fail\n");
    }
    else printf("subcomm return correct\n");
    */
  }
}

u8* read_spi(hid_device *jc, u8 addr1, u8 addr2, int len, u8 is_left) {
  u8 buf[] = { addr2, addr1, 0x00, 0x00, (u8)len };
  int tries = 0;
  do {
    ++tries;
    subcomm(jc, buf, 5, 0x10, 1, is_left);
  } while (tries < 10 && !(data[15] == addr2 && data[16] == addr1));
  return data + 20;
}

void get_stick_cal(hid_device* jc, u8 is_left) {
  // dump calibration data
  u8* out = read_spi(jc, 0x80, is_left ? 0x12 : 0x1d, 9, is_left);
  u8 found = 0;
  for(int i = 0; i < 9; ++i) {
    if(out[i] != 0xff) {
      // User calibration data found
      std::cout << "warning: user calibration data not found" << std::endl;
      found = 1;
      break;
    }
  }
  if(!found) {
    std::cout << "warning: user calibration data not found" << std::endl;
    out = read_spi(jc, 0x60, is_left ? 0x3d : 0x46, 9, is_left);
  }
  stick_cal[is_left ? 4 : 7]  = ((out[7] << 8) & 0xf00) | out[6]; // X Min below center
  stick_cal[is_left ? 5 : 8]  = ((out[8] << 4) | (out[7] >> 4));  // Y Min below center
  stick_cal[is_left ? 0 : 9]  = ((out[1] << 8) & 0xf00) | out[0]; // X Max above center
  stick_cal[is_left ? 1 : 10] = ((out[2] << 4) | (out[1] >> 4));  // Y Max above center
  stick_cal[is_left ? 2 : 11] = ((out[4] << 8) & 0xf00 | out[3]); // X Center
  stick_cal[is_left ? 3 : 12] = ((out[5] << 4) | (out[4] >> 4));  // Y Center
  out = read_spi(jc, 0x60, is_left ? 0x86 : 0x98, 9, is_left);
  stick_cal[is_left ? 6 : 13] = ((out[4] << 8) & 0xF00 | out[3]); // Deadzone
}

void setup_joycon(hid_device *jc, u8 leds, bool is_left) {
  u8 send_buf = 0x3f;
  u8 left = is_left ? 1 : 0;
  subcomm(jc, &send_buf, 1, 0x3, 1, left);
  get_stick_cal(jc, left);
/*  TODO: improve bluetooth pairing
  send_buf = 0x1;
  subcomm(jc, &send_buf, 1, 0x1, 1, left);
  subcomm(jc, &send_buf, 1, 0x1, 1, left);
  send_buf = 0x2;
  subcomm(jc, &send_buf, 1, 0x1, 1, left);
  send_buf = 0x3;
  subcomm(jc, &send_buf, 1, 0x1, 1, left);*/
  send_buf = leds;
  subcomm(jc, &send_buf, 1, 0x30, 1, left);
  send_buf = 0x30;
  subcomm(jc, &send_buf, 1, 0x3, 1, left);
}

void inc_timming() {
	timming_byte++;
}

int set_led_busy(hid_device* handle, const unsigned short product_id) {
	int res;
	u8 buf[49];
	memset(buf, 0, sizeof(buf));
	auto hdr = (brcm_hdr *)buf;
	auto pkt = (brcm_cmd_01 *)(hdr + 1);
	hdr->cmd = 0x01;
	hdr->timer = timming_byte & 0xF;
	inc_timming();
	pkt->subcmd = 0x30;
	pkt->subcmd_arg.arg1 = 0x81;
	res = hid_write(handle, buf, sizeof(buf));
	res = hid_read_timeout(handle, buf, 0, 64);

	//Set breathing HOME Led
	if (product_id != JOYCON_L) {
		memset(buf, 0, sizeof(buf));
		hdr = (brcm_hdr *)buf;
		pkt = (brcm_cmd_01 *)(hdr + 1);
		hdr->cmd = 0x01;
		hdr->timer = timming_byte & 0xF;
		inc_timming();
		pkt->subcmd = 0x38;
		pkt->subcmd_arg.arg1 = 0x28;
		pkt->subcmd_arg.arg2 = 0x20;
		buf[13] = 0xF2;
		buf[14] = buf[15] = 0xF0;
		res = hid_write(handle, buf, sizeof(buf));
		res = hid_read_timeout(handle, buf, 0, 64);
	}

	return 0;
}

int silence_input_report(hid_device* handle) {
	int res;
	u8 buf[49];
	int error_reading = 0;

	bool loop = true;

	while (1) {
		memset(buf, 0, sizeof(buf));
		auto hdr = (brcm_hdr *)buf;
		auto pkt = (brcm_cmd_01 *)(hdr + 1);
		hdr->cmd = 1;
		hdr->timer = timming_byte & 0xF;
		inc_timming();
		pkt->subcmd = 0x03;
		pkt->subcmd_arg.arg1 = 0x3f;
		res = hid_write(handle, buf, sizeof(buf));
		int retries = 0;
		while (1) {
			res = hid_read_timeout(handle, buf, sizeof(buf), 64);
			if (*(u16*)&buf[0xD] == 0x0380) {
				loop = false;
				break;
			}

			retries++;
			if (retries > 8 || res == 0)
				break;
		}
		if (!loop) {
			break;
		}
		error_reading++;
		if (error_reading > 4)
			break;
	}

	return 0;
}

std::string get_sn(u32 offset, const u16 read_len, hid_device* handle) {
	int res;
	int error_reading = 0;
	u8 buf[49];
	std::string sn = "";
	bool loop = true;
	while (1) {
		memset(buf, 0, sizeof(buf));
		auto hdr = (brcm_hdr *)buf;
		auto pkt = (brcm_cmd_01 *)(hdr + 1);
		hdr->cmd = 1;
		hdr->timer = timming_byte & 0xF;
		inc_timming();
		pkt->subcmd = 0x10;
		pkt->spi_data.offset = offset;
		pkt->spi_data.size = read_len;
		res = hid_write(handle, buf, sizeof(buf));

		int retries = 0;
		while (1) {
			res = hid_read_timeout(handle, buf, sizeof(buf), 64);
			if ((*(u16*)&buf[0xD] == 0x1090) && (*(uint32_t*)&buf[0xF] == offset)) {
				loop = false;
				break;
			}
			retries++;
			if (retries > 8 || res == 0)
				break;
		}

		if (!loop) {
			break;
		}

		error_reading++;
		if (error_reading > 20)
			return "Error!";
	}
	
	if (res >= 0x14 + read_len) {
		for (int i = 0; i < read_len; i++) {
			if (buf[0x14 + i] != 0x000) {
				sn += buf[0x14 + i];
			}
			else
				sn += "";
		}
	}
	else {
		return "Error!";
	}
	return sn;
}

int get_battery(u8* test_buf, hid_device* handle) {
	int res;
	u8 buf[49];

	int error_reading = 0;
	bool loop = true;
	while (1) {
		memset(buf, 0, sizeof(buf));
		auto hdr = (brcm_hdr *)buf;
		auto pkt = (brcm_cmd_01 *)(hdr + 1);
		hdr->cmd = 1;
		hdr->timer = timming_byte & 0xF;
		timming_byte++;
		pkt->subcmd = 0x50;
		res = hid_write(handle, buf, sizeof(buf));
		int retries = 0;
		while (1) {
			res = hid_read_timeout(handle, buf, sizeof(buf), 64);
			if (*(u16*)&buf[0xD] == 0x50D0) {
				loop = false;
				break;
			}

			retries++;
			if (retries > 8 || res == 0)
				break;
		}

		if (!loop) {
			break;
		}
		error_reading++;
		if (error_reading > 20)
			break;
	}

	test_buf[0] = buf[0x2];
	test_buf[1] = buf[0xF];
	test_buf[2] = buf[0x10];

	return 0;
}

std::string get_battery_str(hid_device* handle) {
	unsigned char batt_info[3];
	memset(batt_info, 0, sizeof(batt_info));

	get_battery(batt_info, handle);

	int batt_percent = 0;
	int batt = ((u8)batt_info[0] & 0xF0) >> 4;

	// Calculate aproximate battery percent from regulated voltage
	u16 batt_volt = (u8)batt_info[1] + ((u8)batt_info[2] << 8);
	if (batt_volt < 0x560)
		batt_percent = 1;
	else if (batt_volt > 0x55F && batt_volt < 0x5A0) {
		batt_percent = ((batt_volt - 0x60) & 0xFF) / 7.0f + 1;
	}
	else if (batt_volt > 0x59F && batt_volt < 0x5E0) {
		batt_percent = ((batt_volt - 0xA0) & 0xFF) / 2.625f + 11;
	}
	else if (batt_volt > 0x5DF && batt_volt < 0x618) {
		batt_percent = (batt_volt - 0x5E0) / 1.8965f + 36;
	}
	else if (batt_volt > 0x617 && batt_volt < 0x658) {
		batt_percent = ((batt_volt - 0x18) & 0xFF) / 1.8529f + 66;
	}
	else if (batt_volt > 0x657)
		batt_percent = 100;

	char buf[20];
	sprintf_s(buf, "%.2fV - %d%%", (batt_volt * 2.5) / 1000, batt_percent);
	return buf;
}

int get_spi_data(u32 offset, const u16 read_len, u8 *test_buf, hid_device* handle) {
	int res;
	u8 buf[49];
	int error_reading = 0;
	while (1) {
		memset(buf, 0, sizeof(buf));
		auto hdr = (brcm_hdr *)buf;
		auto pkt = (brcm_cmd_01 *)(hdr + 1);
		hdr->cmd = 1;
		hdr->timer = timming_byte & 0xF;
		timming_byte++;
		pkt->subcmd = 0x10;
		pkt->spi_data.offset = offset;
		pkt->spi_data.size = read_len;
		res = hid_write(handle, buf, sizeof(buf));

		int retries = 0;
		bool loop = true;
		while (1) {
			res = hid_read_timeout(handle, buf, sizeof(buf), 64);
			if ((*(u16*)&buf[0xD] == 0x1090) && (*(uint32_t*)&buf[0xF] == offset)) {
				loop = false;
				break;
			}

			retries++;
			if (retries > 8 || res == 0)
				break;
		}
		if (!loop) {
			break;
		}
		error_reading++;
		if (error_reading > 20)
			return 1;
	}
	
	if (res >= 0x14 + read_len) {
		for (int i = 0; i < read_len; i++) {
			test_buf[i] = buf[0x14 + i];
		}
	}

	return 0;
}

unsigned char* get_colors(hid_device* handle) {
	unsigned char* spiColors = (unsigned char*)malloc(12);
	memset(spiColors, 0, 12); 

	int res = get_spi_data(0x6050, 12, spiColors, handle);

	return spiColors;
}

int get_device_info(u8* test_buf, hid_device* handle) {
	int res;
	u8 buf[49];
	int error_reading = 0;
	bool loop = true;
	while (1) {
		memset(buf, 0, sizeof(buf));
		auto hdr = (brcm_hdr *)buf;
		auto pkt = (brcm_cmd_01 *)(hdr + 1);
		hdr->cmd = 1;
		hdr->timer = timming_byte & 0xF;
		timming_byte++;
		pkt->subcmd = 0x02;
		res = hid_write(handle, buf, sizeof(buf));
		int retries = 0;
		while (1) {
			res = hid_read_timeout(handle, buf, sizeof(buf), 64);
			if (*(u16*)&buf[0xD] == 0x0282) {
				loop = false;
				break;
			}

			retries++;
			if (retries > 8 || res == 0)
				break;
		}
		if (!loop) {
			break;
		}
		error_reading++;
		if (error_reading > 20)
			break;
	}
	for (int i = 0; i < 0xA; i++) {
		test_buf[i] = buf[0xF + i];
	}

	return 0;
}

std::string get_mac(hid_device* handle) {
	unsigned char device_info[10];
	memset(device_info, 0, sizeof(device_info));

	get_device_info(device_info, handle);

	char buf[20];
	//Firmware
	//sprintf_s(buf, "%02X.%02X",
	//	device_info[0], device_info[1]);

	sprintf_s(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
		device_info[4], device_info[5], device_info[6], device_info[7], device_info[8], device_info[9]);
	return buf;
}

hid_device* initialize_left_joycon(std::string* mac) {
  hid_device *left_joycon = NULL;
  int counter = 1;

  struct hid_device_info *left_joycon_info = hid_enumerate(NINTENDO, JOYCON_L);
  if (mac != NULL) {
	  do {
		  hid_device * joycon = hid_open(NINTENDO, JOYCON_L, left_joycon_info->serial_number);

		  std::string temp_mac = get_mac(joycon);
		  if (mac->compare(temp_mac) == 0) {
			  break;
		  }
		  left_joycon_info = left_joycon_info->next;
	  } while (left_joycon_info != NULL);
  } else {
    while (left_joycon_info->next != NULL) {
      counter++;
      left_joycon_info = left_joycon_info->next;
    }
  }

  if(left_joycon_info == NULL) {
    std::cout << " => could not find left Joy-Con" << std::endl;
	return left_joycon;
	/*hid_exit();
    vigem_free(client);
    std::cout << "press [ENTER] to exit" << std::endl;
    getchar();
    exit(1);
	*/
  }

  std::cout << " => found left Joy-Con" << std::endl;

  left_joycon = hid_open(NINTENDO, JOYCON_L, left_joycon_info->serial_number);
  if(left_joycon != NULL)
    std::cout << " => successfully connected to left Joy-Con" << std::endl;
  else {
    std::cout << " => could not connect to left Joy-Con" << std::endl;
	return left_joycon;
    /*hid_exit();
    vigem_free(client);
    std::cout << "press [ENTER] to exit" << std::endl;
    getchar();
    exit(1);
	*/
  }

  setup_joycon(left_joycon, counter, true);
  return left_joycon;
}

hid_device* initialize_right_joycon(std::string* mac) {
  hid_device *right_joycon = NULL;

  int counter = 1;

  struct hid_device_info *right_joycon_info = hid_enumerate(NINTENDO, JOYCON_R);
  if (mac != NULL) {
	  do {
		  hid_device * joycon = hid_open(NINTENDO, JOYCON_R, right_joycon_info->serial_number);

		  std::string temp_mac = get_mac(joycon);
		  if (mac->compare(temp_mac) == 0) {
			  break;
		  }
		  right_joycon_info = right_joycon_info->next;
	  } while (right_joycon_info != NULL);

  } else {
	  while (right_joycon_info->next != NULL) {
		  counter++;
		  right_joycon_info = right_joycon_info->next;
	  }
  }

  if(right_joycon_info == NULL) {
    std::cout << " => could not find right Joy-Con" << std::endl;
	return right_joycon;
    /*hid_exit();
    vigem_free(client);
    std::cout << "press [ENTER] to exit" << std::endl;
    getchar();
    exit(1);*/
  }

  std::cout << " => found right Joy-Con" << std::endl;
  right_joycon = hid_open(NINTENDO, JOYCON_R, right_joycon_info->serial_number);
  if(right_joycon != NULL)
    std::cout << " => successfully connected to right Joy-Con" << std::endl;
  else {
    std::cout << " => could not connect to right Joy-Con" << std::endl;
	return right_joycon;
	/*hid_exit();
    vigem_free(client);
    std::cout << "press [ENTER] to exit" << std::endl;
    getchar();
    exit(1);
	*/
  }
  setup_joycon(right_joycon, counter, false);

  return right_joycon;
}

class Xbox {
public:
	PVIGEM_CLIENT client;
	PVIGEM_TARGET target;
	XUSB_REPORT* report;
	USHORT left_buttons = 0;
	USHORT right_buttons = 0;
};

class Joystick {
public:
	std::string* mac;
	Xbox* xbox;
	HANDLE report_mutex;
};

Xbox* initialize_xbox() {
  Xbox* xbox = new Xbox();
  xbox->report = new XUSB_REPORT;
  xbox->client = vigem_alloc();
  
  std::cout << "initializing emulated Xbox 360 controller..." << std::endl;
  VIGEM_ERROR err = vigem_connect(xbox->client);
  if(err == VIGEM_ERROR_NONE) {
    std::cout << " => connected successfully" << std::endl;
  } else {
    std::cout << "connection error: " << vigem_error_to_string(err) << std::endl;
    vigem_free(xbox->client);
    std::cout << "press [ENTER] to exit" << std::endl;
    getchar();
    exit(1);
  }
  xbox->target = vigem_target_x360_alloc();
  vigem_target_add(xbox->client, xbox->target);
  XUSB_REPORT_INIT(xbox->report);
  std::cout << " => added target Xbox 360 Controller" << std::endl;
  std::cout << std::endl;
  return xbox;
}

void disconnect_exit(Xbox xbox) {
  hid_exit();
  vigem_target_remove(xbox.client, xbox.target);
  vigem_target_free(xbox.target);
  vigem_disconnect(xbox.client);
  vigem_free(xbox.client);
  exit(0);
}

void process_stick(XUSB_REPORT* report, bool is_left, uint8_t a, uint8_t b, uint8_t c) {
  u16 raw[] = { (uint16_t)(a | ((b & 0xf) << 8)), \
             (uint16_t)((b >> 4) | (c << 4)) };
  float s[] = { 0, 0 };
  u8 offset = is_left ? 0 : 7;
  for(u8 i = 0; i < 2; ++i) {
    s[i] = (raw[i] - stick_cal[i + 2 + offset]);
    if(abs(s[i]) < stick_cal[6 + offset]) s[i] = 0;  // inside deadzone
    else if(s[i] > 0) s[i] /= stick_cal[i + offset]; // axis is above center
    else s[i] /= stick_cal[i + 4 + offset];          // axis is below center
    if(s[i] > 1)  s[i] = 1;
    if(s[i] < -1) s[i] = -1;
    s[i] *= (XBOX_ANALOG_MAX);
  }
  
  if(is_left) {
    report->sThumbLX = (SHORT)s[0];
    report->sThumbLY = (SHORT)s[1];
  }
  else {
    report->sThumbRX = (SHORT)s[0];
    report->sThumbRY = (SHORT)s[1];
  }
}

void process_button(Xbox* xbox, JOYCON_REGION region, JOYCON_BUTTON button) {
  if(!((region == LEFT_ANALOG && button == L_ANALOG_NONE) || (region == RIGHT_ANALOG && button == R_ANALOG_NONE)))
	std::cout << joycon_button_to_string(region, button) << std::endl;
  auto got = button_mappings.find(button);
  if(got != button_mappings.end()) {
    XUSB_BUTTON target = got->second;
    switch (region) {
      case LEFT_DPAD:
      case LEFT_AUX:
		  xbox->left_buttons = xbox->left_buttons | target;
        return;
      case LEFT_ANALOG:
      case RIGHT_ANALOG:
        break;
      case RIGHT_BUTTONS:
      case RIGHT_AUX:
		  xbox->right_buttons = xbox->right_buttons | target;
        return;
    }
  }
  switch(region) {
    case LEFT_DPAD:
      switch(button) {
        case L_DPAD_UP:
			xbox->left_buttons = xbox->left_buttons | XUSB_GAMEPAD_DPAD_UP;
          break;
        case L_DPAD_DOWN:
			xbox->left_buttons = xbox->left_buttons | XUSB_GAMEPAD_DPAD_DOWN;
          break;
        case L_DPAD_LEFT:
			xbox->left_buttons = xbox->left_buttons | XUSB_GAMEPAD_DPAD_LEFT;
          break;
        case L_DPAD_RIGHT:
			xbox->left_buttons = xbox->left_buttons | XUSB_GAMEPAD_DPAD_RIGHT;
          break;
        case L_DPAD_SL:
			xbox->left_buttons = xbox->left_buttons | XUSB_GAMEPAD_X;
          break;
        case L_DPAD_SR:
			xbox->left_buttons = xbox->left_buttons | XUSB_GAMEPAD_A;
          break;
      }
      break;
    case LEFT_ANALOG:
      switch(button) {
        case L_ANALOG_DOWN:
			xbox->report->sThumbLX = 0;
			xbox->report->sThumbLY = XBOX_ANALOG_MIN;
          break;
        case L_ANALOG_UP:
			xbox->report->sThumbLX = 0;
			xbox->report->sThumbLY = XBOX_ANALOG_MAX;
          break;
        case L_ANALOG_LEFT:
			xbox->report->sThumbLX = XBOX_ANALOG_MIN;
			xbox->report->sThumbLY = 0;
          break;
        case L_ANALOG_RIGHT:
			xbox->report->sThumbLX = XBOX_ANALOG_MAX;
			xbox->report->sThumbLY = 0;
          break;
        case L_ANALOG_DOWN_LEFT:
			xbox->report->sThumbLX = XBOX_ANALOG_DIAG_MIN;
			xbox->report->sThumbLY = XBOX_ANALOG_DIAG_MIN;
          break;
        case L_ANALOG_DOWN_RIGHT:
			xbox->report->sThumbLX = XBOX_ANALOG_DIAG_MAX;
			xbox->report->sThumbLY = XBOX_ANALOG_DIAG_MIN;
          break;
        case L_ANALOG_UP_LEFT:
			xbox->report->sThumbLX = XBOX_ANALOG_DIAG_MIN;
			xbox->report->sThumbLY = XBOX_ANALOG_DIAG_MAX;
          break;
        case L_ANALOG_UP_RIGHT:
			xbox->report->sThumbLX = XBOX_ANALOG_DIAG_MAX;
			xbox->report->sThumbLY = XBOX_ANALOG_DIAG_MAX;
          break;
        case L_ANALOG_NONE:
			xbox->report->sThumbLX = 0;
			xbox->report->sThumbLY = 0;
          break;
      }
      break;
    case LEFT_AUX:
      switch(button) {
        case L_SHOULDER:
			xbox->left_buttons = xbox->left_buttons | XUSB_GAMEPAD_LEFT_SHOULDER;
          break;
        case L_TRIGGER:
			xbox->report->bLeftTrigger = 255;
          break;
        case L_CAPTURE:
			xbox->left_buttons = xbox->left_buttons | XUSB_GAMEPAD_BACK;
          break;
        case L_MINUS:
			xbox->left_buttons = xbox->left_buttons | XUSB_GAMEPAD_BACK;
          break;
        case L_STICK:
			xbox->left_buttons = xbox->left_buttons | XUSB_GAMEPAD_LEFT_THUMB;
          break;
      }
      break;
    case RIGHT_ANALOG:
      switch(button) {
        case R_ANALOG_DOWN:
			xbox->report->sThumbRX = 0;
			xbox->report->sThumbRY = XBOX_ANALOG_MIN;
          break;
        case R_ANALOG_UP:
			xbox->report->sThumbRX = 0;
			xbox->report->sThumbRY = XBOX_ANALOG_MAX;
          break;
        case R_ANALOG_LEFT:
			xbox->report->sThumbRX = XBOX_ANALOG_MIN;
			xbox->report->sThumbRY = 0;
          break;
        case R_ANALOG_RIGHT:
			xbox->report->sThumbRX = XBOX_ANALOG_MAX;
			xbox->report->sThumbRY = 0;
          break;
        case R_ANALOG_DOWN_LEFT:
			xbox->report->sThumbRX = XBOX_ANALOG_DIAG_MIN;
			xbox->report->sThumbRY = XBOX_ANALOG_DIAG_MIN;
          break;
        case R_ANALOG_DOWN_RIGHT:
			xbox->report->sThumbRX = XBOX_ANALOG_DIAG_MAX;
			xbox->report->sThumbRY = XBOX_ANALOG_DIAG_MIN;
          break;
        case R_ANALOG_UP_LEFT:
			xbox->report->sThumbRX = XBOX_ANALOG_DIAG_MIN;
			xbox->report->sThumbRY = XBOX_ANALOG_DIAG_MAX;
          break;
        case R_ANALOG_UP_RIGHT:
			xbox->report->sThumbRX = XBOX_ANALOG_DIAG_MAX;
			xbox->report->sThumbRY = XBOX_ANALOG_DIAG_MAX;
          break;
        case R_ANALOG_NONE:
			xbox->report->sThumbRX = 0;
			xbox->report->sThumbRY = 0;
          break;
      }
      break;
    case RIGHT_AUX:
      switch(button) {
      case R_SHOULDER:
		  xbox->right_buttons = xbox->right_buttons | XUSB_GAMEPAD_RIGHT_SHOULDER;
        break;
      case R_TRIGGER:
		  xbox->report->bRightTrigger = 255;
        break;
      case R_HOME:
		  xbox->right_buttons = xbox->right_buttons | XUSB_GAMEPAD_START;
        break;
      case R_PLUS:
		  xbox->right_buttons = xbox->right_buttons | XUSB_GAMEPAD_START;
        break;
      case R_STICK:
		  xbox->right_buttons = xbox->right_buttons | XUSB_GAMEPAD_RIGHT_THUMB;
        break;
      }
      break;
    case RIGHT_BUTTONS:
      switch(button) {
        case R_BUT_B:
			xbox->right_buttons = xbox->right_buttons | XUSB_GAMEPAD_B;
          break;
        case R_BUT_A:
			xbox->right_buttons = xbox->right_buttons | XUSB_GAMEPAD_A;
          break;
        case R_BUT_Y:
			xbox->right_buttons = xbox->right_buttons | XUSB_GAMEPAD_Y;
          break;
        case R_BUT_X:
			xbox->right_buttons = xbox->right_buttons | XUSB_GAMEPAD_X;
          break;
        case R_BUT_SL:
			xbox->right_buttons = xbox->right_buttons | XUSB_GAMEPAD_B;
          break;
        case R_BUT_SR:
			xbox->right_buttons = xbox->right_buttons | XUSB_GAMEPAD_Y;
          break;
      }
      break;
  }
}

inline bool has_button(unsigned char data, JOYCON_BUTTON button) {
  return !!(data & button);
}

inline void region_part(Xbox* xbox, unsigned char data, JOYCON_REGION region, JOYCON_BUTTON button) {
  if(has_button(data, button))
    process_button(xbox, region, button);
}

void process_left_joycon(Xbox* xbox) {
  xbox->report->bLeftTrigger = 0;
  xbox->left_buttons = 0;
  u8 offset = 0;
  u8 shift = 0;
  u8 offset2 = 0;
  if(data[0] == 0x30 || data[0] == 0x21) {
    // 0x30 input reports order the button status data differently
    // this approach is ugly, but doesn't require changing the enum
    offset = 2;
    offset2 = 3;
    shift = 1;
    process_stick(xbox->report, true, data[6], data[7], data[8]);
  }
  else {
    process_button(xbox, LEFT_ANALOG, (JOYCON_BUTTON)data[3]);
  }
  region_part(xbox, data[1 + offset*2]<<shift, LEFT_DPAD, L_DPAD_UP);
  region_part(xbox, data[1 + offset*2]<<shift, LEFT_DPAD, L_DPAD_DOWN);
  region_part(xbox, data[1 + offset*2]>>(shift*3), LEFT_DPAD, L_DPAD_LEFT);
  region_part(xbox, data[1 + offset*2]<<shift, LEFT_DPAD, L_DPAD_RIGHT);
  region_part(xbox, data[1 + offset*2]>>shift, LEFT_DPAD, L_DPAD_SL);
  region_part(xbox, data[1 + offset*2]<<shift, LEFT_DPAD, L_DPAD_SR);
  region_part(xbox, data[2 + offset2], LEFT_AUX, L_TRIGGER);
  region_part(xbox, data[2 + offset2], LEFT_AUX, L_SHOULDER);
  region_part(xbox, data[2 + offset], LEFT_AUX, L_CAPTURE);
  region_part(xbox, data[2 + offset], LEFT_AUX, L_MINUS);
  region_part(xbox, data[2 + offset]>>shift, LEFT_AUX, L_STICK);
  xbox->report->wButtons = xbox->right_buttons | xbox->left_buttons;
}

void process_right_joycon(Xbox* xbox) {
  xbox->report->bRightTrigger = 0;
  xbox->right_buttons = 0;
  u8 offset = 0;
  u8 shift = 0;
  u8 offset2 = 0;
  if(data[0] == 0x30 || data[0] == 0x21) {
    // 0x30 input reports order the button status data differently
    // this approach is ugly, but doesn't require changing the enum
    offset = 2;
    offset2 = 1;
    shift = 1;
    process_stick(xbox->report, false, data[9], data[10], data[11]);
  } else process_button(xbox, RIGHT_ANALOG, (JOYCON_BUTTON)data[3]);
  region_part(xbox, data[1 + offset]>>(shift*3), RIGHT_BUTTONS, R_BUT_A);
  region_part(xbox, data[1 + offset], RIGHT_BUTTONS, R_BUT_B);
  region_part(xbox, data[1 + offset], RIGHT_BUTTONS, R_BUT_X);
  region_part(xbox, data[1 + offset]<<(shift*3), RIGHT_BUTTONS, R_BUT_Y);
  region_part(xbox, data[1 + offset]>>shift, RIGHT_BUTTONS, R_BUT_SL);
  region_part(xbox, data[1 + offset]<<shift, RIGHT_BUTTONS, R_BUT_SR);
  region_part(xbox, data[2 + offset2], RIGHT_AUX, R_TRIGGER);
  region_part(xbox, data[2 + offset2], RIGHT_AUX, R_SHOULDER);
  region_part(xbox, data[2 + offset], RIGHT_AUX, R_HOME);
  region_part(xbox, data[2 + offset], RIGHT_AUX, R_PLUS);
  region_part(xbox, data[2 + offset]<<shift, RIGHT_AUX, R_STICK);
  xbox->report->wButtons = xbox->left_buttons | xbox->right_buttons;
}

void joycon_cleanup(hid_device *jc, u8 is_left) {
  u8 send_buf = 0x3f;
  subcomm(jc, &send_buf, 1, 0x3, 1, is_left);
}

DWORD WINAPI left_joycon_thread(__in LPVOID lpParameter) {
  Joystick* joystick = (Joystick*)lpParameter;
  WaitForSingleObject(joystick->report_mutex, INFINITE);
  std::cout << " => left Joy-Con thread started" << std::endl;
  hid_device* left_joycon = initialize_left_joycon(joystick->mac);
  ReleaseMutex(joystick->report_mutex);
  for(;;) {
    if(kill_threads) break;
    hid_read(left_joycon, data, DATA_BUFFER_SIZE);
    WaitForSingleObject(joystick->report_mutex, INFINITE);
    process_left_joycon(joystick->xbox);
	vigem_target_x360_update(joystick->xbox->client, joystick->xbox->target, *(joystick->xbox->report));
    ReleaseMutex(joystick->report_mutex);
  }
  joycon_cleanup(left_joycon, 1);
  return 0;
}

DWORD WINAPI right_joycon_thread(__in LPVOID lpParameter) {
  Joystick* joystick = (Joystick*)lpParameter;
  WaitForSingleObject(joystick->report_mutex, INFINITE);
  std::cout << " => right Joy-Con thread started" << std::endl;
  hid_device* right_joycon = initialize_right_joycon(joystick->mac);
  ReleaseMutex(joystick->report_mutex);
  for(;;) {
    if(kill_threads) break;
    hid_read(right_joycon, data, DATA_BUFFER_SIZE);
    WaitForSingleObject(joystick->report_mutex, INFINITE);
    process_right_joycon(joystick->xbox);
	vigem_target_x360_update(joystick->xbox->client, joystick->xbox->target, *(joystick->xbox->report));
    ReleaseMutex(joystick->report_mutex);
  }
  joycon_cleanup(right_joycon, 0);
  return 0;
}

void terminate(Xbox xbox, HANDLE left_thread, HANDLE right_thread) {
  kill_threads = true;
  Sleep(10);
  TerminateThread(left_thread, 0);
  TerminateThread(right_thread, 0);
  std::cout << "disconnecting and exiting..." << std::endl;
  disconnect_exit(xbox);
}

void exit_handler(int signum) {
  terminate();
  exit(signum);
}

std::string get_body_color(unsigned char* spiColors) {
	char color[10];
	sprintf_s(color, "#%02X%02X%02X", (u8)spiColors[0], (u8)spiColors[1], (u8)spiColors[2]);
	std::string col = color;
	if (ANIMAL_CROSSING_BLUE.compare(color) == 0) {
		col = "Animal crossing Blue";
	}
	else if (ANIMAL_CROSSING_GREEN.compare(color) == 0) {
		col = "Animal crossing Green";
	}
	else if (NEON_BLUE.compare(color) == 0) {
		col = "Neon Blue";
	}
	else if (NEON_RED.compare(color) == 0) {
		col = "Neon Red";
	}
	else if (HORI_RED.compare(color) == 0) {
		col = "HORI Red";
	}
	else if (GRAY.compare(color) == 0) {
		col = "Gray";
	}
	else if (NEON_YELLOW.compare(color) == 0) {
		col = "Neon Yellow";
	}
	else if (NEON_GREEN.compare(color) == 0) {
		col = "Neon Green";
	}
	else if (NEON_PINK.compare(color) == 0) {
		col = "Neon Pink";
	}
	else if (BLUE.compare(color) == 0) {
		col = "Blue";
	} else if (NEON_PURPLE.compare(color) == 0) {
		col = "Neon Purple";
	}
	else if (NEON_ORANGE.compare(color) == 0) {
		col = "Neon Orange";
	}

	return col;
}

void show_hidinfo(std::string prefix, const unsigned short product_id, wchar_t* hidserial) {
	hid_device * joycon = hid_open(NINTENDO, product_id, hidserial);
	if (joycon != NULL) {
		unsigned char* spiColors = get_colors(joycon);
		std::cout << "- "<< prefix << ", " << get_sn(0x6001, 0xF, joycon) << ", " << get_mac(joycon) << ", " << get_body_color(spiColors) << ", " << get_battery_str(joycon) << std::endl;
	}
}

void show_list(std::string name) {
	std::cout << "Available devices:" << std::endl;

	int left_counter = 0;
	struct hid_device_info *left_joycon_info = hid_enumerate(NINTENDO, JOYCON_L);
	do {
		if (left_joycon_info == NULL)
			break;

		show_hidinfo("Left Joycon ", JOYCON_L, left_joycon_info->serial_number);
		
		left_counter++;
		left_joycon_info = left_joycon_info->next;
	} while (left_joycon_info != NULL);

	int right_counter = 0;
	struct hid_device_info *right_joycon_info = hid_enumerate(NINTENDO, JOYCON_R);
	do {
		if (right_joycon_info == NULL)
			break;
		show_hidinfo("Right Joycon", JOYCON_R, right_joycon_info->serial_number);

		right_counter++;
		right_joycon_info = right_joycon_info->next;
	} while (right_joycon_info != NULL);

	if (left_counter == 0 && right_counter == 0)
		std::cout << "  No joycons available, sync with OS first" << std::endl;

}

void show_usage(std::string name) {
	            //         1         2         3         4         5         6         7         8
	            //12345678901234567890123456789012345678901234567890123456789012345678901234567890
	std::cout << "Configure joy cons" << std::endl << std::endl;
	std::cout << "XJoy [/?] [/L] [/V] [/P[[:]Args]]" << std::endl << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "/L	List existing joy cons" << std::endl;
	std::cout << "/V	Show XJoy version" << std::endl;
	std::cout << "/P	Pair by MAC" << std::endl;
	std::cout << "Args	Pair formed by -, Pairs separated by ," << std::endl;
	std::cout << "  	11:22:33:44:55:66-11:22:33:44:55:66,11:22:33:44:55:66-11:22:33:44:55:66" << std::endl;
	std::cout << "/?	Show this help" << std::endl << std::endl;
}

void show_version() {
	std::cout << "XJoy v0.2.0" << std::endl << std::endl;
}

bool isValidMac(std::string* mac) {
	return std::regex_match(*mac, std::regex("[a-fA-F0-9][a-fA-F0-9]:[a-fA-F0-9][a-fA-F0-9]:[a-fA-F0-9][a-fA-F0-9]:[a-fA-F0-9][a-fA-F0-9]:[a-fA-F0-9][a-fA-F0-9]:[a-fA-F0-9][a-fA-F0-9]"));
}

bool checkJoyconTypeByMac(std::string* mac, unsigned short type) {
	struct hid_device_info *joycon_info = hid_enumerate(NINTENDO, type);
	do {
		if (joycon_info == NULL)
			break;
		hid_device * joycon = hid_open(NINTENDO, type, joycon_info->serial_number);

		std::string temp_mac = get_mac(joycon);
		if (mac->compare(temp_mac) == 0) {
			return true;
		}
		joycon_info = joycon_info->next;
	} while (joycon_info != NULL);
	return false;
}


int pairing(std::string* l_mac, std::string* r_mac) {
	DWORD left_thread_id;
	DWORD right_thread_id;
	HANDLE left_thread;
	HANDLE right_thread;

	Xbox* xbox = initialize_xbox();

	Joystick* l_joystick = new Joystick();
	l_joystick->mac = l_mac;
	l_joystick->xbox = xbox;

	Joystick* r_joystick = new Joystick();
	r_joystick->mac = r_mac;
	r_joystick->xbox = xbox;

	std::cout << std::endl;
	std::cout << "initializing threads..." << std::endl;
	HANDLE report_mutex = CreateMutex(NULL, FALSE, NULL);
	if (report_mutex == NULL) {
		printf("CreateMutex error: %d\n", GetLastError());
		return 1;
	}

	l_joystick->report_mutex = report_mutex;
	r_joystick->report_mutex = report_mutex;

	std::cout << " => created report mutex" << std::endl;
	left_thread = CreateThread(0, 0, left_joycon_thread, l_joystick, 0, &left_thread_id);
	right_thread = CreateThread(0, 0, right_joycon_thread, r_joystick, 0, &right_thread_id);
	Sleep(500);
	std::cout << std::endl;
	//getchar();
	//terminate(xbox, left_thread, right_thread);
	return 0;
}

int pairMacs(std::string* l_mac, std::string* r_mac) {
	if (!isValidMac(l_mac)) {
		std::cout << "Invalid mac" << *l_mac << std::endl << std::endl;
		return 1;
	}
	else if (!isValidMac(r_mac)) {
		std::cout << "Invalid mac" << *r_mac << std::endl << std::endl;
		return 1;
	}
	else if (!checkJoyconTypeByMac(l_mac, JOYCON_L)) {
		std::cout << "Left joy con not found with mac" << *l_mac << std::endl << std::endl;
		return 1;
	}
	else if (!checkJoyconTypeByMac(r_mac, JOYCON_R)) {
		std::cout << "Right joy con not found with mac" << *r_mac << std::endl << std::endl;
		return 1;
	}
	else {
		pairing(l_mac, r_mac);
	}
	return 0;
}

int pairTuple(std::string pair) {
	std::cout << "Pair tuple: " << pair << std::endl;
	if (pair.empty()) {
		std::cout << "Add pair arguments" << std::endl;
		return 1;
	}
	else if (pair.find("-")) {
		std::istringstream f(pair);
		std::string s;
		std::vector<std::string> strings;
		while (getline(f, s, '-')) {
			strings.push_back(s);
		}
		if (strings.size() != 2) {
			std::cout << "Wrong number of macs:" << pair << std::endl;
			return 1;
		}
		return pairMacs(&strings[0], &strings[1]);
	}
	else {
		std::cout << "Unknow pair tuple args:" << pair << std::endl;
		return 1;
	}

	return 0;
}

int pair_joycons(std::string pairList) {
	std::cout << "Pair list args:" << pairList << std::endl;
	if (pairList.empty()) {
		std::cout << "Add pair arguments" << std::endl;
		return 1;
	}
	else if (pairList.find(",")) {
		std::istringstream f(pairList);
		std::string s;
		while (getline(f, s, ',')) {
			int result = pairTuple(s);
			if (result != 0) {
				std::cout << "Error pairing tuple" << s << std::endl;
				return result;
			}
		}
	}
	else {
		return pairTuple(pairList);
	}
	return 0;
}

int main(int argc, char *argv[]) {
  signal(SIGINT, exit_handler);

  hid_init();

  std::string name = argv[0];
  for (int i = 1; i < argc; ++i) {
	  std::string arg = argv[i];
	  if (arg == "/?") {
		  show_usage(name);
		  return 0;
	  }
	  else if (arg == "/V") {
		  show_version();
		  return 0;
	  }
	  else if (arg == "/L") {
		  show_list(name);
		  return 0;
	  }
	  else if (arg.rfind("/P", 0) == 0) {
		  std::string pairargs = arg.substr(2);
		  pair_joycons(pairargs);
		  std::cout << "press [ENTER] to exit" << std::endl;
          getchar();
		  //terminate(xbox, left_thread, right_thread);
		  return 0;
	  }
	  else {
		  std::cout << "Unknow option" << arg << std::endl;
		  return 1;
	  }
  }

  //Without option, pairs the last connected joycons
  return pairing(NULL, NULL);
}

