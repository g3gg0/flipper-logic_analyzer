# flipper-logic-analyzer
Source: https://github.com/g3gg0/flipper-logic_analyzer
I'm in the process of bringing this in line with the latest flipper firmwares. Right now it loads on my flipper, next I need to test its functionality for bugs. Here are the steps I used to upload:
1. Install [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) (`pip install --upgrade ufbt`)
2. Install [PulseView](https://www.sigrok.org/wiki/Downloads)
3. Clone this repo: `git clone https://github.com/ecopsychologer/flipper-logic-analyzer`
4. Change directories into the repo: `cd flipper-logic-analyzer`
5. Run the app `ufbt launch APPID=fz_logic_analyzer`
6. Launch PulseView and connect to channels C0, C1, C3, B2, B3, A4, A6, A7
#### from the original developer:
 - all 8 channels supported Channel 0 is C0, Channel 1 is C1, ... Channel 7 is A7
 - fixed sampling rate not supported (yet?)
 - if a trigger level is defined, no matter which one, the signals are captured as soon this signal changes
 - maximum capture rate unclear. didn't make any tests. guess in the 100kHz range
 - sample count capped to 16384 for now. didn't check what is possible using malloc()
 - only ONE SHOT currently supported. unknown reason. you have to close and reopen the capture window in PulseView (probably bug in PulseView?)

Discussion thread: https://discord.com/channels/740930220399525928/1074401633615749230
 
