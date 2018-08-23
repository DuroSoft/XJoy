#include "stdafx.h"
#include "Windows.h"
#include "ViGEmClient.h"
#include "hidapi.h"
#include <iostream>
#include <string>

const unsigned short NINTENDO = 1406; // 0x057e
const unsigned short JOYCON_L = 8198; // 0x2006
const unsigned short JOYCON_R = 8199; // 0x2007
const int XBOX_ANALOG_MIN = -32768;
const int XBOX_ANALOG_MAX = 32767;
const int XBOX_ANALOG_DIAG_MAX = round(XBOX_ANALOG_MAX * 0.5 * sqrt(2.0));
const int XBOX_ANALOG_DIAG_MIN = round(XBOX_ANALOG_MIN * 0.5 * sqrt(2.0));
const XUSB_REPORT blank_report;
#define DATA_BUFFER_SIZE 6

PVIGEM_CLIENT client = vigem_alloc();
hid_device *left_joycon = NULL;
hid_device *right_joycon = NULL;
PVIGEM_TARGET target;
XUSB_REPORT report;
unsigned char data_left[DATA_BUFFER_SIZE];
unsigned char data_right[DATA_BUFFER_SIZE];
int res;
HANDLE left_thread;
DWORD left_thread_id;
HANDLE right_thread;
DWORD right_thread_id;
HANDLE report_mutex;
bool kill_threads = false;

enum JOYCON_REGION {
  LEFT_DPAD,
  LEFT_ANALOG,
  LEFT_AUX,
  RIGHT_BUTTONS,
  RIGHT_AUX,
  RIGHT_ANALOG
};

enum JOYCON_BUTTON {
  L_DPAD_LEFT = 1,            // left dpad
  L_DPAD_DOWN = 2,
  L_DPAD_UP = 4,
  L_DPAD_RIGHT = 8,
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

std::string joycon_button_to_string(JOYCON_REGION region, JOYCON_BUTTON button) {
  switch(region) {
    case LEFT_DPAD:
      switch(button) {
        case L_DPAD_LEFT: return "L_DPAD_LEFT";
        case L_DPAD_DOWN: return "L_DPAD_DOWN";
        case L_DPAD_UP: return "L_DPAD_UP";
        case L_DPAD_RIGHT: return "L_DPAD_RIGHT";
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

void initialize_joycons() {
  std::cout << "initializing Joy-Cons..." << std::endl;
  hid_init();
  std::cout << " => initialized hidapi library" << std::endl;
  struct hid_device_info *left_joycon_info = hid_enumerate(NINTENDO, JOYCON_L);
  if(left_joycon_info != NULL) std::cout << " => found left Joy-Con" << std::endl;
  else {
    std::cout << " => could not find left Joy-Con" << std::endl;
    hid_exit();
    vigem_free(client);
    exit(1);
  }
  struct hid_device_info *right_joycon_info = hid_enumerate(NINTENDO, JOYCON_R);
  if(right_joycon_info != NULL) std::cout << " => found right Joy-Con" << std::endl;
  else {
    std::cout << " => could not find right Joy-Con" << std::endl;
    hid_exit();
    vigem_free(client);
    exit(1);
  }
  left_joycon = hid_open(NINTENDO, JOYCON_L, left_joycon_info->serial_number);
  if(left_joycon != NULL) std::cout << " => successfully connected to left Joy-Con" << std::endl;
  else {
    std::cout << " => could not connect to left Joy-Con" << std::endl;
    hid_exit();
    vigem_free(client);
    exit(1);
  }
  right_joycon = hid_open(NINTENDO, JOYCON_R, right_joycon_info->serial_number);
  if(left_joycon != NULL) std::cout << " => successfully connected to right Joy-Con" << std::endl;
  else {
    std::cout << " => could not connect to right Joy-Con" << std::endl;
    hid_exit();
    vigem_free(client);
    exit(1);
  }
  std::cout << std::endl;
}

void initialize_xbox() {
  std::cout << "initializing emulated Xbox 360 controller..." << std::endl;
  VIGEM_ERROR err = vigem_connect(client);
  if(err == VIGEM_ERROR_NONE) {
    std::cout << " => connected successfully" << std::endl;
  } else {
    std::cout << "connection error: " << vigem_error_to_string(err) << std::endl;
    std::cout << "exiting..." << std::endl;
    vigem_free(client);
    exit(1);
  }
  target = vigem_target_x360_alloc();
  vigem_target_add(client, target);
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

void process_button(JOYCON_REGION region, JOYCON_BUTTON button) {
  if(!(region == LEFT_ANALOG && button == L_ANALOG_NONE) && !(region == RIGHT_ANALOG && button == R_ANALOG_NONE))
  std::cout << joycon_button_to_string(region, button) << " ";
  USHORT xbox_button = 0;
  switch(region) {
    case LEFT_DPAD:
      switch(button) {
        case L_DPAD_UP: xbox_button = XUSB_GAMEPAD_DPAD_UP; break;
        case L_DPAD_DOWN: xbox_button = XUSB_GAMEPAD_DPAD_DOWN; break;
        case L_DPAD_LEFT: xbox_button = XUSB_GAMEPAD_DPAD_LEFT; break;
        case L_DPAD_RIGHT: xbox_button = XUSB_GAMEPAD_DPAD_RIGHT; break;
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
      }
      break;
    case LEFT_AUX:
      switch(button) {
        case L_SHOULDER:
          report.wButtons = report.wButtons | XUSB_GAMEPAD_LEFT_SHOULDER;
          break;
        case L_TRIGGER:
          report.bLeftTrigger = 255;
          break;
        case L_CAPTURE:
          report.wButtons = report.wButtons | XUSB_GAMEPAD_GUIDE;
          break;
        case L_MINUS:
          // not implemented
          break;
        case L_STICK:
          report.wButtons = report.wButtons | XUSB_GAMEPAD_LEFT_THUMB;
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
      }
    case RIGHT_AUX:
      switch(button) {
      case R_SHOULDER:
        report.wButtons = report.wButtons | XUSB_GAMEPAD_RIGHT_SHOULDER;
        break;
      case R_TRIGGER:
        report.bRightTrigger = 255;
        break;
      case R_HOME:
        report.wButtons = report.wButtons | XUSB_GAMEPAD_START;
        break;
      case R_PLUS:
        // not implemented
        break;
      case R_STICK:
        report.wButtons = report.wButtons | XUSB_GAMEPAD_RIGHT_THUMB;
        break;
      }
      break;
    case RIGHT_BUTTONS:
      switch(button) {
        case R_BUT_A:
          report.wButtons = report.wButtons | XUSB_GAMEPAD_B;
          break;
        case R_BUT_B:
          report.wButtons = report.wButtons | XUSB_GAMEPAD_A;
          break;
        case R_BUT_X:
          report.wButtons = report.wButtons | XUSB_GAMEPAD_Y;
          break;
        case R_BUT_Y:
          report.wButtons = report.wButtons | XUSB_GAMEPAD_X;
          break;
      }
      break;
  }
  report.wButtons = report.wButtons | xbox_button;
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
  region_part(data_left[1], LEFT_DPAD, L_DPAD_UP);
  region_part(data_left[1], LEFT_DPAD, L_DPAD_DOWN);
  region_part(data_left[1], LEFT_DPAD, L_DPAD_LEFT);
  region_part(data_left[1], LEFT_DPAD, L_DPAD_RIGHT);
  process_buttons(LEFT_ANALOG, (JOYCON_BUTTON)data_left[3]);
  region_part(data_left[2], LEFT_AUX, L_TRIGGER);
  region_part(data_left[2], LEFT_AUX, L_SHOULDER);
  region_part(data_left[2], LEFT_AUX, L_CAPTURE);
  region_part(data_left[2], LEFT_AUX, L_MINUS);
  region_part(data_left[2], LEFT_AUX, L_STICK);
}

void process_right_joycon() {
  region_part(data_right[1], RIGHT_BUTTONS, R_BUT_A);
  region_part(data_right[1], RIGHT_BUTTONS, R_BUT_B);
  region_part(data_right[1], RIGHT_BUTTONS, R_BUT_X);
  region_part(data_right[1], RIGHT_BUTTONS, R_BUT_Y);
  process_buttons(RIGHT_ANALOG, (JOYCON_BUTTON)data_right[3]);
  region_part(data_right[2], RIGHT_AUX, R_TRIGGER);
  region_part(data_right[2], RIGHT_AUX, R_SHOULDER);
  region_part(data_right[2], RIGHT_AUX, R_HOME);
  region_part(data_right[2], RIGHT_AUX, R_PLUS);
  region_part(data_right[2], RIGHT_AUX, R_STICK);
}

DWORD WINAPI left_joycon_thread(__in LPVOID lpParameter) {
  WaitForSingleObject(report_mutex, INFINITE);
  std::cout << " => left joycon thread started" << std::endl;
  ReleaseMutex(report_mutex);
  for(;;) {
    if(kill_threads) return 0;
    hid_read(left_joycon, data_left, DATA_BUFFER_SIZE);
    WaitForSingleObject(report_mutex, INFINITE);
    process_left_joycon();
    report = blank_report;
    XUSB_REPORT_INIT(&report);
    vigem_target_x360_update(client, target, report);
    std::cout << std::endl;
    ReleaseMutex(report_mutex);
  }
  return 0;
}

DWORD WINAPI right_joycon_thread(__in LPVOID lpParameter) {
  WaitForSingleObject(report_mutex, INFINITE);
  std::cout << " => right joycon thread started" << std::endl;
  ReleaseMutex(report_mutex);
  for(;;) {
    if(kill_threads) return 0;
    hid_read(right_joycon, data_right, DATA_BUFFER_SIZE);
    WaitForSingleObject(report_mutex, INFINITE);
    process_right_joycon();
    report = blank_report;
    XUSB_REPORT_INIT(&report);
    vigem_target_x360_update(client, target, report);
    std::cout << std::endl;
    ReleaseMutex(report_mutex);
  }
  return 0;
}

int main() {
  std::cout << "XJoy v0.1.0" << std::endl << std::endl;

  initialize_joycons();
  initialize_xbox();

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
  kill_threads = true;
  Sleep(10);
  TerminateThread(left_thread, 0);
  TerminateThread(right_thread, 0);
  std::cout << "disconnecting and exiting..." << std::endl;
  disconnect_exit();
}
