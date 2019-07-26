#include "stdafx.h"
#include "Windows.h"
#include "ViGEmClient.h"
#include "hidapi.h"
#include <iostream>
#include <string>
#include <csignal>
#include <tuple>
#include <unordered_map>
#include "Yaml.hpp"


#define u8 uint8_t
#define u16 uint16_t

const unsigned short NINTENDO = 1406; // 0x057e
const unsigned short JOYCON_L = 8198; // 0x2006
const unsigned short JOYCON_R = 8199; // 0x2007
const int XBOX_ANALOG_MIN = -32768;
const int XBOX_ANALOG_MAX = 32767;
const int XBOX_ANALOG_DIAG_MAX = round(XBOX_ANALOG_MAX * 0.5 * sqrt(2.0));
const int XBOX_ANALOG_DIAG_MIN = round(XBOX_ANALOG_MIN * 0.5 * sqrt(2.0));

#define DATA_BUFFER_SIZE 49
#define OUT_BUFFER_SIZE 49
u8 data[DATA_BUFFER_SIZE];
u16 stick_cal[14];
u8 global_counter[2] = { 0,0 };

PVIGEM_CLIENT client = vigem_alloc();
hid_device *left_joycon = NULL;
hid_device *right_joycon = NULL;
PVIGEM_TARGET target;
XUSB_REPORT report;
int res;
HANDLE left_thread;
DWORD left_thread_id;
HANDLE right_thread;
DWORD right_thread_id;
HANDLE report_mutex;
USHORT left_buttons = 0;
USHORT right_buttons = 0;
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

void setup_joycon(hid_device *jc, u8 leds, u8 is_left) {
  u8 send_buf = 0x3f;
  subcomm(jc, &send_buf, 1, 0x3, 1, is_left);
  get_stick_cal(jc, is_left);
/*  TODO: improve bluetooth pairing
  send_buf = 0x1;
  subcomm(jc, &send_buf, 1, 0x1, 1, is_left);
  send_buf = 0x2;
  subcomm(jc, &send_buf, 1, 0x1, 1, is_left);
  send_buf = 0x3;
  subcomm(jc, &send_buf, 1, 0x1, 1, is_left);*/
  send_buf = leds;
  subcomm(jc, &send_buf, 1, 0x30, 1, is_left);
  send_buf = 0x30;
  subcomm(jc, &send_buf, 1, 0x3, 1, is_left);
}

void initialize_left_joycon() {
  struct hid_device_info *left_joycon_info = hid_enumerate(NINTENDO, JOYCON_L);
  if(left_joycon_info != NULL) std::cout << " => found left Joy-Con" << std::endl;
  else {
    std::cout << " => could not find left Joy-Con" << std::endl;
    hid_exit();
    vigem_free(client);
    std::cout << "press [ENTER] to exit" << std::endl;
    getchar();
    exit(1);
  }
  left_joycon = hid_open(NINTENDO, JOYCON_L, left_joycon_info->serial_number);
  if(left_joycon != NULL) std::cout << " => successfully connected to left Joy-Con" << std::endl;
  else {
    std::cout << " => could not connect to left Joy-Con" << std::endl;
    hid_exit();
    vigem_free(client);
    std::cout << "press [ENTER] to exit" << std::endl;
    getchar();
    exit(1);
  }

  setup_joycon(left_joycon, 0x1, 1);
}

void initialize_right_joycon() {
  struct hid_device_info *right_joycon_info = hid_enumerate(NINTENDO, JOYCON_R);
  if(right_joycon_info != NULL) std::cout << " => found right Joy-Con" << std::endl;
  else {
    std::cout << " => could not find right Joy-Con" << std::endl;
    hid_exit();
    vigem_free(client);
    std::cout << "press [ENTER] to exit" << std::endl;
    getchar();
    exit(1);
  }
  right_joycon = hid_open(NINTENDO, JOYCON_R, right_joycon_info->serial_number);
  if(right_joycon != NULL) std::cout << " => successfully connected to right Joy-Con" << std::endl;
  else {
    std::cout << " => could not connect to right Joy-Con" << std::endl;
    hid_exit();
    vigem_free(client);
    std::cout << "press [ENTER] to exit" << std::endl;
    getchar();
    exit(1);
  }
  setup_joycon(right_joycon, 0x1, 0);



}

void initialize_xbox() {
  std::cout << "initializing emulated Xbox 360 controller..." << std::endl;
  VIGEM_ERROR err = vigem_connect(client);
  if(err == VIGEM_ERROR_NONE) {
    std::cout << " => connected successfully" << std::endl;
  } else {
    std::cout << "connection error: " << vigem_error_to_string(err) << std::endl;
    vigem_free(client);
    std::cout << "press [ENTER] to exit" << std::endl;
    getchar();
    exit(1);
  }
  target = vigem_target_x360_alloc();
  vigem_target_add(client, target);
  XUSB_REPORT_INIT(&report);
  std::cout << " => added target Xbox 360 Controller" << std::endl;
  std::cout << std::endl;
}

void disconnect_exit() {
  hid_exit();
  vigem_target_remove(client, target);
  vigem_target_free(target);
  vigem_disconnect(client);
  vigem_free(client);
  exit(0);
}

void process_stick(bool is_left, uint8_t a, uint8_t b, uint8_t c) {
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
    report.sThumbLX = (SHORT)s[0];
    report.sThumbLY = (SHORT)s[1];
  }
  else {
    report.sThumbRX = (SHORT)s[0];
    report.sThumbRY = (SHORT)s[1];
  }
}

void process_button(JOYCON_REGION region, JOYCON_BUTTON button) {
  if(!((region == LEFT_ANALOG && button == L_ANALOG_NONE) || (region == RIGHT_ANALOG && button == R_ANALOG_NONE)))
  std::cout << joycon_button_to_string(region, button) << " ";
  auto got = button_mappings.find(button);
  if(got != button_mappings.end()) {
    XUSB_BUTTON target = got->second;
    switch (region) {
      case LEFT_DPAD:
      case LEFT_AUX:
        left_buttons = left_buttons | target;
        return;
      case LEFT_ANALOG:
      case RIGHT_ANALOG:
        break;
      case RIGHT_BUTTONS:
      case RIGHT_AUX:
        right_buttons = right_buttons | target;
        return;
    }
  }
  switch(region) {
    case LEFT_DPAD:
      switch(button) {
        case L_DPAD_UP:
          left_buttons = left_buttons | XUSB_GAMEPAD_DPAD_UP;
          break;
        case L_DPAD_DOWN:
          left_buttons = left_buttons | XUSB_GAMEPAD_DPAD_DOWN;
          break;
        case L_DPAD_LEFT:
          left_buttons = left_buttons | XUSB_GAMEPAD_DPAD_LEFT;
          break;
        case L_DPAD_RIGHT:
          left_buttons = left_buttons | XUSB_GAMEPAD_DPAD_RIGHT;
          break;
        case L_DPAD_SL:
          left_buttons = left_buttons | XUSB_GAMEPAD_X;
          break;
        case L_DPAD_SR:
          left_buttons = left_buttons | XUSB_GAMEPAD_A;
          break;
      }
      break;
    case LEFT_ANALOG:
      switch(button) {
        case L_ANALOG_DOWN:
          report.sThumbLX = 0;
          report.sThumbLY = XBOX_ANALOG_MIN;
          break;
        case L_ANALOG_UP:
          report.sThumbLX = 0;
          report.sThumbLY = XBOX_ANALOG_MAX;
          break;
        case L_ANALOG_LEFT:
          report.sThumbLX = XBOX_ANALOG_MIN;
          report.sThumbLY = 0;
          break;
        case L_ANALOG_RIGHT:
          report.sThumbLX = XBOX_ANALOG_MAX;
          report.sThumbLY = 0;
          break;
        case L_ANALOG_DOWN_LEFT:
          report.sThumbLX = XBOX_ANALOG_DIAG_MIN;
          report.sThumbLY = XBOX_ANALOG_DIAG_MIN;
          break;
        case L_ANALOG_DOWN_RIGHT:
          report.sThumbLX = XBOX_ANALOG_DIAG_MAX;
          report.sThumbLY = XBOX_ANALOG_DIAG_MIN;
          break;
        case L_ANALOG_UP_LEFT:
          report.sThumbLX = XBOX_ANALOG_DIAG_MIN;
          report.sThumbLY = XBOX_ANALOG_DIAG_MAX;
          break;
        case L_ANALOG_UP_RIGHT:
          report.sThumbLX = XBOX_ANALOG_DIAG_MAX;
          report.sThumbLY = XBOX_ANALOG_DIAG_MAX;
          break;
        case L_ANALOG_NONE:
          report.sThumbLX = 0;
          report.sThumbLY = 0;
          break;
      }
      break;
    case LEFT_AUX:
      switch(button) {
        case L_SHOULDER:
          left_buttons = left_buttons | XUSB_GAMEPAD_LEFT_SHOULDER;
          break;
        case L_TRIGGER:
          report.bLeftTrigger = 255;
          break;
        case L_CAPTURE:
          left_buttons = left_buttons | XUSB_GAMEPAD_BACK;
          break;
        case L_MINUS:
          left_buttons = left_buttons | XUSB_GAMEPAD_BACK;
          break;
        case L_STICK:
          left_buttons = left_buttons | XUSB_GAMEPAD_LEFT_THUMB;
          break;
      }
      break;
    case RIGHT_ANALOG:
      switch(button) {
        case R_ANALOG_DOWN:
          report.sThumbRX = 0;
          report.sThumbRY = XBOX_ANALOG_MIN;
          break;
        case R_ANALOG_UP:
          report.sThumbRX = 0;
          report.sThumbRY = XBOX_ANALOG_MAX;
          break;
        case R_ANALOG_LEFT:
          report.sThumbRX = XBOX_ANALOG_MIN;
          report.sThumbRY = 0;
          break;
        case R_ANALOG_RIGHT:
          report.sThumbRX = XBOX_ANALOG_MAX;
          report.sThumbRY = 0;
          break;
        case R_ANALOG_DOWN_LEFT:
          report.sThumbRX = XBOX_ANALOG_DIAG_MIN;
          report.sThumbRY = XBOX_ANALOG_DIAG_MIN;
          break;
        case R_ANALOG_DOWN_RIGHT:
          report.sThumbRX = XBOX_ANALOG_DIAG_MAX;
          report.sThumbRY = XBOX_ANALOG_DIAG_MIN;
          break;
        case R_ANALOG_UP_LEFT:
          report.sThumbRX = XBOX_ANALOG_DIAG_MIN;
          report.sThumbRY = XBOX_ANALOG_DIAG_MAX;
          break;
        case R_ANALOG_UP_RIGHT:
          report.sThumbRX = XBOX_ANALOG_DIAG_MAX;
          report.sThumbRY = XBOX_ANALOG_DIAG_MAX;
          break;
        case R_ANALOG_NONE:
          report.sThumbRX = 0;
          report.sThumbRY = 0;
          break;
      }
      break;
    case RIGHT_AUX:
      switch(button) {
      case R_SHOULDER:
        right_buttons = right_buttons | XUSB_GAMEPAD_RIGHT_SHOULDER;
        break;
      case R_TRIGGER:
        report.bRightTrigger = 255;
        break;
      case R_HOME:
        right_buttons = right_buttons | XUSB_GAMEPAD_START;
        break;
      case R_PLUS:
        right_buttons = right_buttons | XUSB_GAMEPAD_START;
        break;
      case R_STICK:
        right_buttons = right_buttons | XUSB_GAMEPAD_RIGHT_THUMB;
        break;
      }
      break;
    case RIGHT_BUTTONS:
      switch(button) {
        case R_BUT_A:
          right_buttons = right_buttons | XUSB_GAMEPAD_B;
          break;
        case R_BUT_B:
          right_buttons = right_buttons | XUSB_GAMEPAD_A;
          break;
        case R_BUT_X:
          right_buttons = right_buttons | XUSB_GAMEPAD_Y;
          break;
        case R_BUT_Y:
          right_buttons = right_buttons | XUSB_GAMEPAD_X;
          break;
        case R_BUT_SL:
          right_buttons = right_buttons | XUSB_GAMEPAD_B;
          break;
        case R_BUT_SR:
          right_buttons = right_buttons | XUSB_GAMEPAD_Y;
          break;
      }
      break;
  }
}

void process_buttons(JOYCON_REGION region, JOYCON_BUTTON a) {
  process_button(region, a);
}

void process_buttons(JOYCON_REGION region, JOYCON_BUTTON a, JOYCON_BUTTON b) {
  process_button(region, a);
  process_button(region, b);
}

void process_buttons(JOYCON_REGION region, JOYCON_BUTTON a, JOYCON_BUTTON b, JOYCON_BUTTON c) {
  process_button(region, a);
  process_button(region, b);
  process_button(region, c);
}

void process_buttons(JOYCON_REGION region, JOYCON_BUTTON a, JOYCON_BUTTON b, JOYCON_BUTTON c, JOYCON_BUTTON d) {
  process_button(region, a);
  process_button(region, b);
  process_button(region, c);
  process_button(region, d);
}

inline bool has_button(unsigned char data, JOYCON_BUTTON button) {
  return !!(data & button);
}

inline void region_part(unsigned char data, JOYCON_REGION region, JOYCON_BUTTON button) {
  if(has_button(data, button)) process_buttons(region, button);
}

void process_left_joycon() {
  report.bLeftTrigger = 0;
  left_buttons = 0;
  u8 offset = 0;
  u8 shift = 0;
  u8 offset2 = 0;
  if(data[0] == 0x30 || data[0] == 0x21) {
    // 0x30 input reports order the button status data differently
    // this approach is ugly, but doesn't require changing the enum
    offset = 2;
    offset2 = 3;
    shift = 1;
    process_stick(true, data[6], data[7], data[8]);
  }
  else {
    process_buttons(LEFT_ANALOG, (JOYCON_BUTTON)data[3]);
  }
  region_part(data[1 + offset*2]<<shift, LEFT_DPAD, L_DPAD_UP);
  region_part(data[1 + offset*2]<<shift, LEFT_DPAD, L_DPAD_DOWN);
  region_part(data[1 + offset*2]>>(shift*3), LEFT_DPAD, L_DPAD_LEFT);
  region_part(data[1 + offset*2]<<shift, LEFT_DPAD, L_DPAD_RIGHT);
  region_part(data[1 + offset*2]>>shift, LEFT_DPAD, L_DPAD_SL);
  region_part(data[1 + offset*2]<<shift, LEFT_DPAD, L_DPAD_SR);
  region_part(data[2 + offset2], LEFT_AUX, L_TRIGGER);
  region_part(data[2 + offset2], LEFT_AUX, L_SHOULDER);
  region_part(data[2 + offset], LEFT_AUX, L_CAPTURE);
  region_part(data[2 + offset], LEFT_AUX, L_MINUS);
  region_part(data[2 + offset]>>shift, LEFT_AUX, L_STICK);
  report.wButtons = right_buttons | left_buttons;
}

void process_right_joycon() {
  report.bRightTrigger = 0;
  right_buttons = 0;
  u8 offset = 0;
  u8 shift = 0;
  u8 offset2 = 0;
  if(data[0] == 0x30 || data[0] == 0x21) {
    // 0x30 input reports order the button status data differently
    // this approach is ugly, but doesn't require changing the enum
    offset = 2;
    offset2 = 1;
    shift = 1;
    process_stick(false, data[9], data[10], data[11]);
  } else process_buttons(RIGHT_ANALOG, (JOYCON_BUTTON)data[3]);
  region_part(data[1 + offset]>>(shift*3), RIGHT_BUTTONS, R_BUT_A);
  region_part(data[1 + offset], RIGHT_BUTTONS, R_BUT_B);
  region_part(data[1 + offset], RIGHT_BUTTONS, R_BUT_X);
  region_part(data[1 + offset]<<(shift*3), RIGHT_BUTTONS, R_BUT_Y);
  region_part(data[1 + offset]>>shift, RIGHT_BUTTONS, R_BUT_SL);
  region_part(data[1 + offset]<<shift, RIGHT_BUTTONS, R_BUT_SR);
  region_part(data[2 + offset2], RIGHT_AUX, R_TRIGGER);
  region_part(data[2 + offset2], RIGHT_AUX, R_SHOULDER);
  region_part(data[2 + offset], RIGHT_AUX, R_HOME);
  region_part(data[2 + offset], RIGHT_AUX, R_PLUS);
  region_part(data[2 + offset]<<shift, RIGHT_AUX, R_STICK);
  report.wButtons = left_buttons | right_buttons;
}

void joycon_cleanup(hid_device *jc, u8 is_left) {
  u8 send_buf = 0x3f;
  subcomm(jc, &send_buf, 1, 0x3, 1, is_left);
}

DWORD WINAPI left_joycon_thread(__in LPVOID lpParameter) {
  WaitForSingleObject(report_mutex, INFINITE);
  std::cout << " => left Joy-Con thread started" << std::endl;
  initialize_left_joycon();
  ReleaseMutex(report_mutex);
  for(;;) {
    if(kill_threads) break;
    hid_read(left_joycon, data, DATA_BUFFER_SIZE);
    WaitForSingleObject(report_mutex, INFINITE);
    process_left_joycon();
    vigem_target_x360_update(client, target, report);
    std::cout << std::endl;
    ReleaseMutex(report_mutex);
  }
  joycon_cleanup(left_joycon, 1);
  return 0;
}

DWORD WINAPI right_joycon_thread(__in LPVOID lpParameter) {
  WaitForSingleObject(report_mutex, INFINITE);
  std::cout << " => right Joy-Con thread started" << std::endl;
  initialize_right_joycon();
  ReleaseMutex(report_mutex);
  for(;;) {
    if(kill_threads) break;
    hid_read(right_joycon, data, DATA_BUFFER_SIZE);
    WaitForSingleObject(report_mutex, INFINITE);
    process_right_joycon();
    vigem_target_x360_update(client, target, report);
    std::cout << std::endl;
    ReleaseMutex(report_mutex);
  }
  joycon_cleanup(right_joycon, 0);
  return 0;
}

void terminate() {
  kill_threads = true;
  Sleep(10);
  TerminateThread(left_thread, 0);
  TerminateThread(right_thread, 0);
  std::cout << "disconnecting and exiting..." << std::endl;
  disconnect_exit();
}

void exit_handler(int signum) {
  terminate();
  exit(signum);
}

int main() {
  signal(SIGINT, exit_handler);
  std::cout << "XJoy v0.1.9" << std::endl << std::endl;

  initialize_xbox();
  hid_init();

  std::cout << std::endl;
  std::cout << "initializing threads..." << std::endl;
  report_mutex = CreateMutex(NULL, FALSE, NULL);
  if(report_mutex == NULL) {
    printf("CreateMutex error: %d\n", GetLastError());
    return 1;
  }
  std::cout << " => created report mutex" << std::endl;
  left_thread = CreateThread(0, 0, left_joycon_thread, 0, 0, &left_thread_id);
  right_thread = CreateThread(0, 0, right_joycon_thread, 0, 0, &right_thread_id);
  Sleep(500);
  std::cout << std::endl;
  getchar();
  terminate();
}

