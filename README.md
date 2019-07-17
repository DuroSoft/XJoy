# XJoy

XJoy allows you to use a pair of Nintendo Joy-Cons as a virtual Xbox 360 controller
on Windows. XJoy is made possible by [ViGEm](https://vigem.org/) and
[hidapi](https://github.com/signal11/hidapi).

## Support this project
XJoy is a free product that I work on in my free time, so any contributions are greatly appreciated.

[![](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=8DTF7NWTZX7ZJ)

## Installation

1. [Install the ViGEm Bus Driver](https://github.com/ViGEm/ViGEmBus/releases/tag/v1.16.112) (install all requirements as well)
2. Install the [Visual C++ Redistributable for Visual Studio 2017](https://go.microsoft.com/fwlink/?LinkId=746572)
2. Download the latest zip from the [releases page](https://github.com/sam0x17/XJoy/releases) and extract it somewhere permanent like your
Documents folder
3. That's it!

## Usage

1. Pair each of your Joy-Cons with Windows (hold down the button on the side to put into
   pairing mode, then go to add bluetooth device in Windows)
2. Ensure that both Joy-Cons show as "Connected" in your bluetooth devices page
3. Run XJoy.exe
4. Start playing games with your Joy-Cons. A virtual xbox controller should
   show up as soon as XJoy.exe starts running (you will hear the USB device inserted sound).
5. To confirm that it is working, try pressing some buttons on your Joy-Cons. You should
   see the names of the buttons currently being pressed printed in the terminal.
6. To exit, press [ENTER] in the terminal window. You can also simply close the window
   however this may not disconnect from the Joy-Cons and the virtual controller properly.

When you launch XJoy.exe, you should get output similar to this:

```
XJoy v0.1.0

initializing emulated Xbox 360 controller...
 => connected successfully
 => added target Xbox 360 Controller

initializing threads...
 => created report mutex
 => left Joy-Con thread started
 => found left Joy-Con
 => successfully connected to left Joy-Con
 => right Joy-Con thread started
 => found right Joy-Con
 => successfully connected to right Joy-Con
```

## Customization

Right now all buttons are hard-coded to their "default" xbox equivalents. If you wish to
customize these mappings, feel free to modify the `process_button` method in XJoy.cpp. I
plan to add support for a configuration file and maybe a GUI in later versions. The default
mappings are shown below:


| Joy-Con Button     | Xbox Button    |
|--------------------|----------------|
| A                  | B              |
| B                  | A              |
| X                  | Y              |
| Y                  | X              |
| Left Trigger       | Left Trigger   |
| Right Trigger      | Right Trigger  |
| Left Shoulder      | Left Shoulder  |
| Right Shoulder     | Right Shoulder |
| D-PAD              | D-PAD          |
| Left Analog        | Left Analog    |
| Right Analog       | Right Analog   |
| Left Stick         | Left Thumb     |
| Right Stick        | Right Thumb    |
| Home               | Start          |
| Capture            | Back           |
| Plus               | Start          |
| Minus              | Back           |

## Building

If you wish to build XJoy yourself, simply install the ViGEm Bus Driver as outlined in the
installation steps, open the XJoy.sln file in Visual Studio 2017, and build. Everything
should work out of the box but if it does not feel free to submit an issue. Note that at
least on my end it _seems_ to be working in Visual Studio 2019 as well, which is good.

## Coding Style Guidelines
* unix-style line breaks, UTF-8 file encoding
* indent with two spaces (no tabs)
* `{` should be at the end of the relevant line, not on a new line
* opening parenthesis after a control statement should not be preceded by a space -
  `if(` (good), `for(` (good), `while(` (good), `if (` (bad), etc.
* always indent the contents of a `switch` statement, and likewise always indent the contents of
  each `case` unless it is a one-liner (which is totally fine);
* prefer breaking/returning early over complicated if/else chains
* prefer `for(;;) {` for infinite loops
* indent one space after `//` when commenting (`// example comment`)
* prefer `//` over multi-line comment syntax (looks better)
* prefer no blank lines within functions, but acceptable if things get crazy
* prefer one-line if/while/etc statements when possible
* prefer two-line if/else when possible
* separate functions and classes with a single blank line

### Example:

```C++
if(boolean_expression) i++;
else cool_thing();
if(!another_expression) {
  crazy_variable--;
  awesome_stuff();
} else if(other_thing) {
  wicked();
} else {
  thats_cool();
}
switch(state) {
  case 0:
    std::cout << "it was zero" << std::endl;
    break;
  case 1:
    std::cout << "it was one" << std::endl;
    break;
}
while(something) something_else();
for(;;) {
  // infinite loop
  if(condition) return;
}
```

## Contributing
1. create a fork for your feature/bugfix/whatever
2. make your additions/modifications
3. test your changes, doing your best to ensure you aren't introducing any environment-specific
   behavior that may cause issues for users without your exact setup. XJoy's target audience is
   freshly installed Windows 10 users who have followed the ViGEm installation instructions.
4. follow the above code style guidelines
5. submit a pull request
