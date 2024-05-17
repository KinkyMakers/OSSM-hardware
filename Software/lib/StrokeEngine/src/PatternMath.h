#pragma once

#include <math.h>
#include <Arduino.h>


/**************************************************************************/
/*!
  @brief This function will scale one set of floating point numbers (range) 
  to another set of floating point numbers (range). It has a "curve" parameter 
  so that it can be made to favor either the end of the output. 
  (Logarithmic mapping) Source: https://playground.arduino.cc/Main/Fscale/
  @param originalMin the minimum value of the original range 
                     this MUST be less than originalMax
  @param originalMax the maximum value of the original range 
                     this MUST be greater than originalMin
  @param newBegin    the end of the new range which maps to originalMin 
                     it can be smaller, or larger, than newEnd, to 
                     facilitate inverting the ranges
  @param newEnd      the end of the new range which maps to originalMax
                     it can be larger, or smaller, than newBegin, to 
                     facilitate inverting the ranges
  @param inputValue  the variable for input that will mapped to the given ranges, 
                     this variable is constrained to 
                     originaMin <= inputValue <= originalMax
  @param curve       curve is the curve which can be made to favor either 
                     end of the output scale in the mapping. 
                     Parameters are from -10 to 10 with 0 being a linear mapping 
                     (which basically takes curve out of the equation)
  @returns the scaled value
*/
/**************************************************************************/
inline float fscale( float originalMin, float originalMax, float newBegin, float
newEnd, float inputValue, float curve){

  float OriginalRange = 0;
  float NewRange = 0;
  float zeroRefCurVal = 0;
  float normalizedCurVal = 0;
  float rangedValue = 0;
  bool invFlag = 0;

  // condition curve parameter
  // limit range

  if (curve > 10) curve = 10;
  if (curve < -10) curve = -10;

  curve = (curve * -.1) ; // - invert and scale - this seems more intuitive - positive numbers give more weight to high end on output
  curve = pow(10, curve); // convert linear scale into logarithmic exponent for other pow function

  // Check for out of range inputValues
  if (inputValue < originalMin) {
    inputValue = originalMin;
  }
  if (inputValue > originalMax) {
    inputValue = originalMax;
  }

  // Zero Reference the values
  OriginalRange = originalMax - originalMin;

  if (newEnd > newBegin){
    NewRange = newEnd - newBegin;
  }
  else
  {
    NewRange = newBegin - newEnd;
    invFlag = 1;
  }

  zeroRefCurVal = inputValue - originalMin;
  normalizedCurVal  =  zeroRefCurVal / OriginalRange;   // normalize to 0 - 1 float

  // Check for originalMin > originalMax  - the math for all other cases i.e. negative numbers seems to work out fine
  if (originalMin > originalMax ) {
    return 0;
  }

  if (invFlag == 0){
    rangedValue =  (pow(normalizedCurVal, curve) * NewRange) + newBegin;

  }
  else     // invert the ranges
  {  
    rangedValue =  newBegin - (pow(normalizedCurVal, curve) * NewRange);
  }

  return rangedValue;
}

/**************************************************************************/
/*!
  @brief  Float version of Arduino's map() function. 
  @param x          The value to be mapped
  @param in_min     in_min gets mapped to out_min
  @param in_max     in_max gets mapped to out_max
  @param out_min    in_min gets mapped to out_min
  @param out_max    in_max gets mapped to out_max
  @returns mapped value
*/
/**************************************************************************/
inline float fmap(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**************************************************************************/
/*!
  @brief  Maps a Sensation value from -100 to +100 to an arbitrary factor. 
  Positive values become a factor > 1. 0 maps to 1.0 and negative values are 
  mapped to the invers between 0 and 1.0. A curve argument may be given if the
  mapping should be curved (log). It uses fscale() under the hood.
  @param maximumFactor   the factor +100 gets mapped to. Should be > 1.0
  @param inputValue     Input parameter to be mapped
  @param curve          curve is the curve which can be made to favor either 
                        end of the output scale in the mapping. 
                        Parameters are from -10 to 10 with 0 being a linear mapping 
                        (which basically takes curve out of the equation)
  @returns the scaled factor
*/
/**************************************************************************/
inline float mapSensationToFactor(float maximumFactor, float inputValue, float curve = 0.0) {
    inputValue = constrain(inputValue, -100.0, 100.0);
    float fscaledValue = 0.0;

    if (inputValue == 0.0) {
        return 1.0;
    } 

    fscaledValue = fscale(0.0, 100.0, 1.0, maximumFactor, abs(inputValue), curve);

    if (inputValue >= 0) {
        return fscaledValue;
    } else {
        return 1.0/fscaledValue;
    }
    
}


