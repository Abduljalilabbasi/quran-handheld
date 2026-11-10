// Arduino IDE entry point. Keep Reader.cpp, Config.h, WaveState.h and font beside this file.
void readerSetup();
void readerLoop();
void setup() { readerSetup(); }
void loop() { readerLoop(); }
