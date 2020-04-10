/**
 * ,---------,       ____  _ __
 * |  ,-^-,  |      / __ )(_) /_______________ _____  ___
 * | (  O  ) |     / __  / / __/ ___/ ___/ __ `/_  / / _ \
 * | / ,--´  |    / /_/ / / /_/ /__/ /  / /_/ / / /_/  __/
 *    +------`   /_____/_/\__/\___/_/   \__,_/ /___/\___/
 *
 * Crazyflie control firmware
 *
 * Copyright (C) 2019 - 2020 Bitcraze AB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, in version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * pulse_processor.h - pulse decoding for lighthouse V1 base stations
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "ootx_decoder.h"
#include "lighthouse_calibration.h"

#define PULSE_PROCESSOR_N_SWEEPS 2
#define PULSE_PROCESSOR_N_BASE_STATIONS 2
#define PULSE_PROCESSOR_N_SENSORS 4
#define PULSE_PROCESSOR_HISTORY_LENGTH 8
#define PULSE_PROCESSOR_TIMESTAMP_BITWIDTH 24
#define PULSE_PROCESSOR_TIMESTAMP_MAX ((1<<PULSE_PROCESSOR_TIMESTAMP_BITWIDTH)-1)

// Utility functions and macros
// #define TS_DIFF(X, Y) ((X-Y)&((1<<TIMESTAMP_BITWIDTH)-1))
inline static uint32_t TS_DIFF(uint32_t x, uint32_t y) {
  const uint32_t bitmask = (1 << PULSE_PROCESSOR_TIMESTAMP_BITWIDTH) - 1;
  return (x - y) & bitmask;
}


enum pulseClass_e {unknown, sync0, sync1, sweep};

typedef struct {
  uint32_t timestamp;
  int width;
} pulseProcessorPulse_t;

typedef enum {
  sweepDirection_x = 0,
  sweepDirection_y = 1
} SweepDirection;

typedef enum {
  sweepStorageStateWaiting = 0,
  sweepStorageStateValid,
  sweepStorageStateError,
} SweepStorageState_t;

/**
 * @brief Holds data for one sweep and one sensor.
 *
 */
typedef struct {
    uint32_t timestamp;
    uint32_t offset;
    uint8_t channel;
    uint8_t slowbit;
    bool channelFound; // Indicates if channel and slowbit are valid
    bool isSet; // Indicates that the data in this struct has been set
} pulseProcessorV2Pulse_t;

typedef struct {
    pulseProcessorV2Pulse_t sensors[PULSE_PROCESSOR_N_SENSORS];
    uint32_t latestTimestamp;
} pulseProcessorV2PulseWorkspace_t;

/**
 * @brief Holds derived data for one sweep through all sensors, derived from the pulses
 *
 */
typedef struct {
    uint32_t offset[PULSE_PROCESSOR_N_SENSORS];
    uint32_t timestamp; // Timestamp of sensor 0
    uint8_t channel;
    uint8_t slowbit;
} pulseProcessorV2SweepBlock_t;

/**
 * @brief Holds data for the sweeps of one base station
 *
 */
typedef struct {
    pulseProcessorV2SweepBlock_t blocks[PULSE_PROCESSOR_N_SWEEPS];
} pulseProcessorV2BaseStation_t;

typedef struct pulseProcessor_s {
  union {
    // V1 base stations
    struct {
      bool synchronized;    // At true if we are currently syncthonized
      int basestationsSynchronizedCount;

      // Synchronization state
      pulseProcessorPulse_t pulseHistory[PULSE_PROCESSOR_N_SENSORS][PULSE_PROCESSOR_HISTORY_LENGTH];
      int pulseHistoryIdx[PULSE_PROCESSOR_N_SENSORS];


      // Sync pulse timestamp estimation
      uint32_t lastSync;        // Last sync seen
      uint64_t currentSyncSum;  // Sum of the timestamps of all the close-together sync
      int nSyncPulses;          // Number of sync pulses accumulated

      // Sync pulse timestamps
      uint32_t currentSync;   // Sync currently used for sweep phase measurement
      uint32_t currentSync0;  // Sync0 of the current frame
      uint32_t currentSync0Width;  // Width of sync0 in the current frame
      uint32_t currentSync1Width;  // Width of sync1 in the current frame

      uint32_t currentSync0X;
      uint32_t currentSync0Y;
      uint32_t currentSync1X;
      uint32_t currentSync1Y;

      float frameWidth[2][2];
    };

    // V2 base stations
    struct {
      // Raw data for the sensors
      pulseProcessorV2PulseWorkspace_t pulseWorkspace;

      // Refined data for multiple base stations
      pulseProcessorV2SweepBlock_t blocksV2[PULSE_PROCESSOR_N_BASE_STATIONS];
    };
  };

  // Base station and axis of the current frame
  int currentBaseStation;
  SweepDirection currentAxis;

  // Sweep timestamps
  struct {
    uint32_t timestamp;
    SweepStorageState_t state;
  } sweeps[PULSE_PROCESSOR_N_SENSORS];
  bool sweepDataStored;

  ootxDecoderState_t ootxDecoder0;
  ootxDecoderState_t ootxDecoder1;

  lighthouseCalibration_t bsCalibration[PULSE_PROCESSOR_N_BASE_STATIONS];
} pulseProcessor_t;

typedef struct {
  float angles[PULSE_PROCESSOR_N_SWEEPS];
  float correctedAngles[PULSE_PROCESSOR_N_SWEEPS];
  int validCount;
} pulseProcessorBaseStationMeasuremnt_t;

typedef struct {
  pulseProcessorBaseStationMeasuremnt_t baseStatonMeasurements[PULSE_PROCESSOR_N_BASE_STATIONS];
} pulseProcessorSensorMeasurement_t;

typedef struct {
  pulseProcessorSensorMeasurement_t sensorMeasurements[PULSE_PROCESSOR_N_SENSORS];
} pulseProcessorResult_t;

typedef struct {
  uint8_t sensor;
  uint32_t timestamp;

  // V1 base station data --------
  uint16_t width;

  // V2 base station data --------
  uint32_t beamData;
  uint32_t offset;
  // Channel is zero indexed (0-15) here, while it is one indexed in the base station config (1 - 16)
  uint8_t channel; // Valid if channelFound is true
  uint8_t slowbit; // Valid if channelFound is true
  bool channelFound;
} pulseProcessorFrame_t;

/**
 * @brief Interface for processing of pulse data from the lighthouse
 *
 * @param state
 * @param frameData
 * @param baseStation
 * @param axis
 * @return true, angle, base station and axis are written
 * @return false, no valid result
 */
typedef bool (*pulseProcessorProcessPulse_t)(pulseProcessor_t *state, const pulseProcessorFrame_t* frameData, pulseProcessorResult_t* angles, int *baseStation, int *axis);

/**
 * @brief Apply calibration correction to all angles of all sensors for a particular baseStation
 *
 * @param state
 * @param angles
 * @param baseStation
 */
void pulseProcessorApplyCalibration(pulseProcessor_t *state, pulseProcessorResult_t* angles, int baseStation);

/**
 * @brief Clear result struct
 *
 * @param angles
 * @param baseStation
 */
void pulseProcessorClear(pulseProcessorResult_t* angles, int baseStation);
