//  specific Front Panel hardware file

#include "..\SDT.h"

#include "..\Encoders.h"
#include "FrontPanel.h"

#if !defined(PROJECTSYSTEM_ENCODER_MCP)

// v11 type encoders and switches
#include <Bounce.h>

#include "..\CWProcessing.h"
#include "..\MenuProc.h"
#include "..\Tune.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#ifdef PROJECTSYSTEM_VOLUME_ENCODER
Rotary volumeEncoder = Rotary(VOLUME_ENCODER_A, VOLUME_ENCODER_B);        // ( 2,  3)
Bounce encoderSwitch = Bounce(VOLUME_SWITCH, 10);  // 10 ms debounce
#endif
#ifdef PROJECTSYSTEM_FILTER_ENCODER
Rotary menuChangeEncoder = Rotary(FILTER_ENCODER_A, FILTER_ENCODER_B);        // ( 2,  3)
Bounce encoder2Switch = Bounce(FILTER_SWITCH, 10);  // 10 ms debounce
#endif
#ifdef PROJECTSYSTEM_FINETUNE_ENCODER
Rotary fineTuneEncoder = Rotary(FINETUNE_ENCODER_A, FINETUNE_ENCODER_B);  // ( 4,  5)
Bounce encoder3Switch = Bounce(FINETUNE_SWITCH, 10);  // 10 ms debounce
#endif
#ifdef PROJECTSYSTEM_TUNE_ENCODER
Rotary tuneEncoder = Rotary(TUNE_ENCODER_A, TUNE_ENCODER_B);              // (16, 17)
Bounce encoder4Switch = Bounce(TUNE_SWITCH, 10);  // 10 ms debounce
#endif

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void ProcessMenuEncoder();

void EncoderVolumeISR();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// set up encoders
//#ifdef PROJECTSYSTEM_TUNE_ENCODER
#if defined(PROJECTSYSTEM_VOLUME_ENCODER) || defined(PROJECTSYSTEM_FILTER_ENCODER) || defined(PROJECTSYSTEM_FINETUNE_ENCODER) || defined(PROJECTSYSTEM_FILTER_ENCODER)
void EncodersInit() {
#ifdef PROJECTSYSTEM_VOLUME_ENCODER
  volumeEncoder.begin(true);
  attachInterrupt(digitalPinToInterrupt(VOLUME_ENCODER_A), EncoderVolumeISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(VOLUME_ENCODER_B), EncoderVolumeISR, CHANGE);

  // set up encoder switch debounce
  pinMode(VOLUME_SWITCH, INPUT_PULLUP);
#endif
#ifdef PROJECTSYSTEM_FILTER_ENCODER
  pinMode(FILTER_ENCODER_A, INPUT);
  pinMode(FILTER_ENCODER_B, INPUT);

  menuChangeEncoder.begin(true);
  attachInterrupt(digitalPinToInterrupt(FILTER_ENCODER_A), EncoderMenuChangeFilterISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(FILTER_ENCODER_B), EncoderMenuChangeFilterISR, CHANGE);

  // set up encoder switch debounce
  pinMode(FILTER_SWITCH, INPUT_PULLUP);
#endif
#ifdef PROJECTSYSTEM_FINETUNE_ENCODER
  fineTuneEncoder.begin(true);
  attachInterrupt(digitalPinToInterrupt(FINETUNE_ENCODER_A), EncoderFineTuneISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(FINETUNE_ENCODER_B), EncoderFineTuneISR, CHANGE);
#endif
#ifdef PROJECTSYSTEM_TUNE_ENCODER
  tuneEncoder.begin(true);
#endif
}
#endif

#ifdef PROJECTSYSTEM_VOLUME_ENCODER
/*****
  Purpose: Encoder volume control ISR
*****/
// why not FASTRUN
void EncoderVolumeISR() {
  char result = 0;

  result = volumeEncoder.process();  // Read the encoder

  if(result == 0) {  // Nothing read
    return;
  }

  // TODO: check encoder setup as this is opposite T41
  switch(result) {
    case DIR_CW:  // Turned it clockwise, 16
      adjustVolEncoder = -1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      adjustVolEncoder = 1;
      break;
  }

  if((calibrateItem >= 1) && (calibrateItem <= 3)) return;

  audioVolume += adjustVolEncoder;
  adjustVolEncoder = 0;

  if(audioVolume > MAX_AUDIO_VOLUME) {
    audioVolume = MAX_AUDIO_VOLUME;
  } else if(audioVolume < MIN_AUDIO_VOLUME) {
    audioVolume = MIN_AUDIO_VOLUME;
  }

  volumeChangeFlag = true; // flag needed for display update
}
#endif

#ifdef PROJECTSYSTEM_FILTER_ENCODER
/*****
  Purpose: Menu/Change/Filter encoder movement ISR
*****/
FASTRUN void EncoderMenuChangeFilterISR() {
  char result;

  result = menuChangeEncoder.process();  // Read the encoder

  if(result == 0) {
    return;
  }

  switch(result) {
    case DIR_CW:  // Turned it clockwise, 16
      menuEncoderMove = 1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      menuEncoderMove = -1;
      break;
  }

  ProcessMenuEncoder();
}
#endif

#ifdef PROJECTSYSTEM_FINETUNE_ENCODER
/*****
  Purpose: Fine tune control ISR
*****/
FASTRUN void EncoderFineTuneISR() {
  char result;

  result = fineTuneEncoder.process();  // Read the encoder

// *** TODO: we'll go through here many times if fine tune encoder bounces ***
  // *** If fineTuneEncoderMove isn't processed in the meantime,
  //    and result == 0, then fineTuneEncoderMove will be reset to zero ***

  if(result == 0) {                   // Nothing read
    fineTuneEncoderMove = 0L;
    return;
  }

  switch(result) {
    case DIR_CW:  // Turned it clockwise, 16
      fineTuneEncoderMove = 1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      fineTuneEncoderMove = -1;
      break;
  }

  // *** TODO: from v12, validate v11 calibration routines
  // fine tune used in calibration routines, return to process
  //   - receive calibrate adjusts In/Out attenuation
  //   - transmit calibrate adjusts In/Out attenuation
  //   - two tone adjusts tone 2
  if((calibrateItem >= 1) && (calibrateItem <= 3)) {
    // -= fineTuneEncoderMove;
    fineTuneEncoderMove = 0;
    return;
  }

  SetFineTune(ftIncrement * fineTuneEncoderMove);

  fineTuneEncoderMove = 0L;
}
#endif

#ifdef PROJECTSYSTEM_TUNE_ENCODER
/*****
  Purpose: Set center tune frequency based on
*****/
bool EncoderCenterTune() {
  unsigned char result;

  result = tuneEncoder.process();  // Read the encoder

  if(result == 0)  // Nothing read
    return false;

  if(radioMode == CW_MODE && decoderFlag == ON) {  // No reason to reset if we're not doing decoded CW
    ResetHistograms();
  }

  switch(result) {
    case DIR_CW:  // Turned it clockwise, 16
      tuneChange = 1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      tuneChange = -1;
      break;
  }

  // *** TODO: from v12, validate v11 calibration routines
  // center tune used in calibration routines, return to process
  //   - receive calibrate adjusts noise floor
  //   - transmit calibrate adjusts image value
  //   - two tone adjusts tone 1
  if((calibrateItem >= 1) && (calibrateItem <= 3)) return;

  SetCenterTune((long)freqIncrement * tuneChange);

  return true;
}
#endif

#endif
