#include "stdafx.h"
#include "Windows.h"
#include "ViGEmClient.h"
#include "hidapi.h"
#include <iostream>
#include <string>

const unsigned short NINTENDO = 1406; // 0x057e
const unsigned short JOYCON_L = 8198; // 0x2006
const unsigned short JOYCON_R = 8199; // 0x2007
#define DATA_BUFFER_SIZE 6

PVIGEM_CLIENT client = vigem_alloc();
hid_device *left_joycon = NULL;
hid_device *right_joycon = NULL;
PVIGEM_TARGET target;
XUSB_REPORT report;
unsigned char data[DATA_BUFFER_SIZE];
int res;

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
  std::cout << "joycon: " << joycon_button_to_string(region, button) << std::endl;
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

void process_left_joycon() {
  switch(data[1]) { // DPAD
    case L_DPAD_DOWN:
      process_buttons(LEFT_DPAD, L_DPAD_DOWN);
      break;
    case L_DPAD_UP:
      process_buttons(LEFT_DPAD, L_DPAD_UP);
      break;
    case L_DPAD_RIGHT:
      process_buttons(LEFT_DPAD, L_DPAD_RIGHT);
      break;
    case L_DPAD_LEFT:
      process_buttons(LEFT_DPAD, L_DPAD_LEFT);
      break;
    case L_DPAD_DOWN + L_DPAD_LEFT:
      process_buttons(LEFT_DPAD, L_DPAD_DOWN, L_DPAD_LEFT);
      break;
    case L_DPAD_DOWN + L_DPAD_RIGHT:
      process_buttons(LEFT_DPAD, L_DPAD_DOWN, L_DPAD_RIGHT);
      break;
    case L_DPAD_DOWN + L_DPAD_UP:
      process_buttons(LEFT_DPAD, L_DPAD_DOWN, L_DPAD_UP);
      break;
    case L_DPAD_LEFT + L_DPAD_RIGHT:
      process_buttons(LEFT_DPAD, L_DPAD_LEFT, L_DPAD_RIGHT);
      break;
    case L_DPAD_LEFT + L_DPAD_UP:
      process_buttons(LEFT_DPAD, L_DPAD_LEFT, L_DPAD_UP);
      break;
    case L_DPAD_UP + L_DPAD_RIGHT:
      process_buttons(LEFT_DPAD, L_DPAD_UP, L_DPAD_RIGHT);
      break;
    case L_DPAD_DOWN + L_DPAD_LEFT + L_DPAD_RIGHT:
      process_buttons(LEFT_DPAD, L_DPAD_DOWN, L_DPAD_LEFT, L_DPAD_RIGHT);
      break;
    case L_DPAD_DOWN + L_DPAD_UP + L_DPAD_RIGHT:
      process_buttons(LEFT_DPAD, L_DPAD_DOWN, L_DPAD_UP, L_DPAD_RIGHT);
      break;
    case L_DPAD_DOWN + L_DPAD_LEFT + L_DPAD_UP:
      process_buttons(LEFT_DPAD, L_DPAD_DOWN, L_DPAD_LEFT, L_DPAD_UP);
      break;
    case L_DPAD_UP + L_DPAD_LEFT + L_DPAD_RIGHT:
      process_buttons(LEFT_DPAD, L_DPAD_UP, L_DPAD_LEFT, L_DPAD_RIGHT);
      break;
    case L_DPAD_UP + L_DPAD_DOWN + L_DPAD_LEFT + L_DPAD_RIGHT:
      process_buttons(LEFT_DPAD, L_DPAD_UP, L_DPAD_DOWN, L_DPAD_LEFT, L_DPAD_RIGHT);
      break;
  }
}

void process_right_joycon() {
  // TODO
}

int main() {
  std::cout << "XJoy v0.1.0" << std::endl << std::endl;

  initialize_joycons();
  initialize_xbox();

  for(;;) {
    hid_read(left_joycon, data, DATA_BUFFER_SIZE);
    process_left_joycon();
    process_right_joycon();
  }

  XUSB_REPORT_INIT(&report);
  report.wButtons = _XUSB_BUTTON::XUSB_GAMEPAD_A | _XUSB_BUTTON::XUSB_GAMEPAD_B;

  vigem_target_x360_update(client, target, report);

  Sleep(10000);
  std::cout << "disconnecting and exiting..." << std::endl;
  disconnect_exit();
}
