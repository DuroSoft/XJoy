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

int main() {
  std::cout << "XJoy v0.1.0" << std::endl << std::endl;

  initialize_joycons();
  initialize_xbox();

  for(;;) {
    hid_read(left_joycon, data, DATA_BUFFER_SIZE);
    for (int i = 0; i < DATA_BUFFER_SIZE; i++) {
      std::cout << (unsigned int)data[i];
    }
    std::cout << std::endl;
  }

  XUSB_REPORT_INIT(&report);
  report.wButtons = _XUSB_BUTTON::XUSB_GAMEPAD_A | _XUSB_BUTTON::XUSB_GAMEPAD_B;

  vigem_target_x360_update(client, target, report);

  Sleep(10000);
  std::cout << "disconnecting and exiting..." << std::endl;
  disconnect_exit();
}
