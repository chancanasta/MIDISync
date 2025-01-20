# MIDISync
Arduino project to sync old pre-MIDI drum machines (Linn, Oberheim) to MIDI


##MIDI to Clock Pulse
This is an Arduino sketch that reads in MIDI clock signals and converts them to
a square wave pulse that will drive the clock sync inputs of pre-MIDI drum machines.

It can output 24/48/96 pulses per quarter note (PPQ) - which covers old Roland, Linn and Oberheim drum machines

To meet MIDI standards, an opto isolator should be used when connecting the MIDI input
