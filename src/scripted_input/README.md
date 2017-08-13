# ScriptedInput

This is a system for supplying button/pad/touch input to the emulator from a file, rather than user input.

It also supports saving screenshots of the game, so they can be compared with past behaviour.

This allows us to create reproducable test cases that utilize real games, so we can test the behaviour citra.

## Script File Format

Example
```bash
30 # first line isn't allowed buttons
10 a # get through the first menu
10
10 start # get through the second menu
10
10 [200,150] # Touch the button
10
screenshot # screenshot of second menu fading out
```

Each line starts with a number, this is how many frames to run. It is then followed by a space separated list of buttons.

Optionally a line maybe a have a comment, started with a `#` character, everything after that is ignored.

If a line starts with `screenshot` then a screenshot will be saved at that time, the file is named `{framenumber}.bmp` and saved in the current working directory.

**THE FOLLOWING ARE NOT YET IMPLEMENTED**

Touch screen input can be denoted with `[100,100]`, where the numbers are the pixel location of the touch (0-320, 0-240).

Circle pad input can be denoted with `(-1,1)`, where the numbers are the joystick location (from -1 to 1). If not specified, the circle pad will be at (0,0).

C-stick input can be denoted with `{1,-1}`, where the numbers are the joystick location (from -1 to 1). If not specified, the C-stick will be at (0,0).