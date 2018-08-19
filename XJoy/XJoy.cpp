#include "stdafx.h"
#include "Windows.h"
#include "ViGEmClient.h"
#include "hidapi.h"
#include <assert.h>
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

std::string vigem_error_to_string(VIGEM_ERROR err) {
  switch (err) {
    case VIGEM_ERROR_NONE:
      return "none";
    case VIGEM_ERROR_ALREADY_CONNECTED:
      return "already connected";
    case VIGEM_ERROR_BUS_ACCESS_FAILED:
      return "bus access failed";
    case VIGEM_ERROR_BUS_ALREADY_CONNECTED:
      return "bus already connected";
    case VIGEM_ERROR_BUS_NOT_FOUND:
      return "bus not found";
    case VIGEM_ERROR_BUS_VERSION_MISMATCH:
      return "bus version mismatch";
    case VIGEM_ERROR_CALLBACK_ALREADY_REGISTERED:
      return "callback already registered";
    case VIGEM_ERROR_CALLBACK_NOT_FOUND:
      return "callback not found";
    case VIGEM_ERROR_INVALID_TARGET:
      return "invalid target";
    case VIGEM_ERROR_NO_FREE_SLOT:
      return "no free slot";
    case VIGEM_ERROR_REMOVAL_FAILED:
      return "removal failed";
    case VIGEM_ERROR_TARGET_NOT_PLUGGED_IN:
      return "target not plugged in";
    case VIGEM_ERROR_TARGET_UNINITIALIZED:
      return "target uninitialized";
  }
  return "none";
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
  if (err == VIGEM_ERROR_NONE) {
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

void process_left_joycon() {
  // TODO
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
