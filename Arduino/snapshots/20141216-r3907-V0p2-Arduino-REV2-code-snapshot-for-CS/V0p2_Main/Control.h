/*
The OpenTRV project licenses this file to you
under the Apache Licence, Version 2.0 (the "Licence");
you may not use this file except in compliance
with the Licence. You may obtain a copy of the Licence at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the Licence is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied. See the Licence for the
specific language governing permissions and limitations
under the Licence.

Author(s) / Copyright (s): Damon Hart-Davis 2013--2014
*/

/*
 Control/model for TRV and boiler.
 */

#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>

#include "V0p2_Main.h"
#include "Messaging.h"

// Special setup for OpenTRV beyond generic hardware setup.
void setupOpenTRV();

// Main loop for OpenTRV radiator control.
void loopOpenTRV();


// Minimum and maximum bounds target temperatures; degrees C/Celsius/centigrade, strictly positive.
// Minimum is some way above 0C to avoid freezing pipework even with small measurement errors and non-uniform temperatures.
// Maximum is set a little below boiling/100C for DHW applications for safety.
// Setbacks and uplifts cannot move temperature targets outside this range for safety.
#define MIN_TARGET_C 5 // Minimum temperature setting allowed (to avoid freezing, allowing for offsets at temperature sensor, etc). 
#define MAX_TARGET_C 95 // Maximum temperature setting allowed (eg for DHW).


// Default frost (minimum) temperature in degrees C, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
// Setting frost temperatures at a level likely to protect (eg) fridge/freezers as well as water pipes.
#define BIASECO_FROST (max(7,MIN_TARGET_C)) // Target FROST temperature for ECO bias; must be in range [MIN_TARGET_C,BIASCOM_FROST[.
#define BIASCOM_FROST (max(12,MIN_TARGET_C)) // Target FROST temperature for Comfort bias; must be in range ]BIASECO_FROST,MAX_TARGET_C].
#define FROST BIASECO_FROST
// 18C is a safe temperature even for the slightly infirm according to NHS England 2014:
//     http://www.nhs.uk/Livewell/winterhealth/Pages/KeepWarmKeepWell.aspx
// so should probably clearly marked on the control.
#define SAFE_ROOM_TEMPERATURE 18 // Safe for most purposes.
// Default warm/comfort room (air) temperature in degrees C; strictly greater than FROST, in range [MIN_TARGET_C,MAX_TARGET_C].
// Control loop effectively targets upper end of this 1C window as of 20130518, middle as of 20141209.
#ifndef DHW_TEMPERATURES
// 17/18 good for energy saving at ~1C below typical UK room temperatures (~19C in 2012).
#define BIASECO_WARM 17 // Target WARM temperature for ECO bias; must be in range ]BIASCOM_FROST+1,BIASCOM_WARM[.
#define BIASCOM_WARM 21 // Target WARM temperature for Comfort bias; must be in range ]BIASECO_WARM,MAX_TARGET_C-BAKE_UPLIFT-1].
#else // Default settings for DHW control.
// 55C+ with boost to 60C+ for DHW Legionella control.
#define BIASECO_WARM 55 // Target WARM temperature for ECO bias; must be in range [MODECOM_WARM,MAX_TARGET_C].
#define BIASCOM_WARM 65 // Target WARM temperature for Comfort bias; must be in range ]MIN_TARGET_C,MODEECO_WARM].
#endif
// Default to a 'safe' temperature.
#define WARM max(BIASECO_WARM,SAFE_ROOM_TEMPERATURE) 

// Raise target by this many degrees in 'BAKE' mode (strictly positive).
#define BAKE_UPLIFT 5
// Maximum 'BAKE' minutes, ie time to crank heating up to BAKE setting (minutes, strictly positive, <255).
#define BAKE_MAX_M 30

// Initial minor setback degrees C (strictly positive).  Note that 1C heating setback may result in ~8% saving in UK.
// This may be the maximum setback applied with a comfort bias for example.
#define SETBACK_DEFAULT 1
// Enhanced setback in full-on eco mode for extra energy savings.  Not more than FULL_SETBACK.
#define SETBACK_ECO (1+SETBACK_DEFAULT)
// Full setback degrees C (strictly positive and significantly, ie several degrees, greater than SETBACK, less than MIN_TARGET_C).
// This must be less than MIN_TARGET_C to avoid problems with unsigned arithmetic.
#define SETBACK_FULL 3
// Prolonged inactivity time deemed to indicate room(s) really unoccupied to trigger full setback (minutes, strictly positive).
#define SETBACK_FULL_M 50

#ifdef LEARN_BUTTON_AVAILABLE
// Period in minutes for simple learned on-time; strictly positive (and less than 256).
#define LEARNED_ON_PERIOD_M 60
// Period in minutes for simple learned on-time with comfort bias; strictly positive (and less than 256).
#define LEARNED_ON_PERIOD_COMFORT_M 120
#endif



// Forcing the warm mode to the specified state immediately.
// Iff forcing to FROST mode then any pending BAKE time is cancelled,
// else BAKE status is unchanged.
// Should be only be called once 'debounced' if coming from a button press for example.
void setWarmModeDebounced(const bool warm);

// If true then the unit is in 'warm' (heating) mode, else 'frost' protection mode.
// This is a 'debounced' value to reduce accidental triggering.
bool inWarmMode();

#ifdef SUPPORT_BAKE // IF DEFINED: this unit supports BAKE mode.
// Force to BAKE mode;
// Should be only be called once 'debounced' if coming from a button press for example.
void startBakeDebounced();
//// If true then the unit is in 'bake' mode, a subset of 'warm' mode which boosts the temperature target temporarily.
//bool inBakeMode();
// If true then the unit is in 'bake' mode, a subset of 'warm' mode which boosts the temperature target temporarily.
// This is a 'debounced' value to reduce accidental triggering.
bool inBakeMode();
// Cancel 'bake' mode if active.
// Should be only be called once 'debounced' if coming from a button press for example.
void cancelBakeDebounced();
#else
#define startBakeDebounced() {}
// NO-OP versions if BAKE mode not supported.
//#define inBakeMode() (false)
#define inBakeModeDebounced() (false)
#define cancelBakeDebounced() {}
#endif




// If true (the default) then the system has an 'Eco' energy-saving bias, else it has a 'comfort' bias.
// Several system parameters are adjusted depending on the bias,
// with 'eco' slanted toward saving energy, eg with lower target temperatures and shorter on-times.
// This is determined from user-settable temperature values.
bool hasEcoBias();


// Get dynamically-set thresholds/parameters.
#if defined(SETTABLE_TARGET_TEMPERATURES) || defined(TEMP_POT_AVAILABLE)
// Get 'FROST' protection target in C; no higher than getWARMTargetC() returns, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
// Depends dynamically on current (last-read) temp-pot setting.
uint8_t getFROSTTargetC();
// Get 'WARM' target in C; no lower than getFROSTTargetC() returns, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
// Depends dynamically on current (last-read) temp-pot setting.
uint8_t getWARMTargetC();
#endif

#if defined(TEMP_POT_AVAILABLE)
// Expose internal calculation of WARM target based on user physical control for unit testing.
// Derived from temperature pot position, 0 for coldest (most eco), 255 for hotest (comfort).
// Temp ranges from eco-1C to comfort+1C levels across full (reduced jitter) [0,255] pot range.
uint8_t computeWARMTargetC(const uint8_t pot);
#endif


#if defined(SETTABLE_TARGET_TEMPERATURES)
// Set (non-volatile) 'FROST' protection target in C; no higher than getWARMTargetC() returns, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
// Can also be used, even when a temperature pot is present, to set a floor setback temperature.
// Returns false if not set, eg because outside range [MIN_TARGET_C,MAX_TARGET_C], else returns true.
bool setFROSTTargetC(uint8_t tempC);
#endif
#if defined(SETTABLE_TARGET_TEMPERATURES) && !defined(TEMP_POT_AVAILABLE)
// Set 'WARM' target in C; no lower than getFROSTTargetC() returns, strictly positive, in range [MIN_TARGET_C,MAX_TARGET_C].
// Returns false if not set, eg because below FROST setting or outside range [MIN_TARGET_C,MAX_TARGET_C], else returns true.
bool setWARMTargetC(uint8_t tempC);
#endif

// True if specified temperature is at or below 'eco' WARM target temperature, ie is eco-friendly.
#define isEcoTemperature(tempC) ((tempC) <= BIASECO_WARM)
// True if specified temperature is at or above 'comfort' WARM target temperature.
#define isComfortTemperature(tempC) ((tempC) >= BIASCOM_WARM)


#ifdef ENABLE_BOILER_HUB
// Get minimum on (and off) time for pointer (minutes); zero if not in hub mode.
uint8_t getMinBoilerOnMinutes();
// Set minimum on (and off) time for pointer (minutes); zero to disable hub mode.
// Suggested minimum of 4 minutes for gas combi; much longer for heat pumps for example.
void setMinBoilerOnMinutes(uint8_t mins);
#else
#define getMinBoilerOnMinutes() (0) // Always disabled.
#define setMinBoilerOnMinutes(mins) {} // Do nothing.
#endif
// True if in central hub/listen mode (possibly with local radiator also).
#define inHubMode() (0 != getMinBoilerOnMinutes())


// Typical minimum valve percentage open to be considered actually/significantly open; [1,100].
// Setting this above 0 delays calling for heat from a central boiler until water is likely able to flow.
// (It may however be possible to scavenge some heat if a particular valve opens below this and the circulation pump is already running, for example.)
// DHD20130522: FHT8V + valve heads I have been using are not typically upen until around 6%.
// Allowing valve to linger at just below this level without calling for heat when shutting
// may allow comfortable bolier pump overrun in older systems with no/poor bypass to avoid overheating.
#define DEFAULT_MIN_VALVE_PC_REALLY_OPEN 10



// Delay in minutes after increasing flow before re-closing is allowed.
// This is to avoid excessive seeking in th presence of strong draughts for example.
// Too large a value will cause significant temperature overshoots.
#define ANTISEEK_VALVE_RECLOSE_DELAY_M 4
// Delay in minutes after restricting flow before re-opening is allowed.
// This is to avoid excessive seeking in th presence of strong draughts for example.
// Too large a value will cause significant temperature undershoots.
#define ANTISEEK_VALVE_REOPEN_DELAY_M (ANTISEEK_VALVE_RECLOSE_DELAY_M*2)

// Internal model of radidator valve position, embodying control logic.
class ModelledRadValve : public AbstractRadValve
  {
  private:
    // True if this node is calling for heat.
    uint8_t targetTempC;

    // True if this node is calling for heat.
    // Marked volatile for thread-safe lock-free access.
    volatile bool callingForHeat;

    // True if the computed valve position was changed by read().
    bool valveMoved;

    // True if in glacial mode.
    // TODO: not fully implemented.
    bool glacial;

    // Set non-zero when valve flow is constricted, and then counts down to zero.
    // Some or all attempts to open the valve are deferred while this is non-zero
    // to reduce valve hunting if there is string turbulence from the radiator
    // or maybe draughts from open windows/doors
    // causing measured temperatures to veer up and down.
    // This attempts to reduce excessive valve noise and energy use
    // and help to avoid boiler short-cycling.
    uint8_t valveTurndownCountdownM;
    // Mark flow as having been reduced.
    void valveTurndown() { valveTurndownCountdownM = ANTISEEK_VALVE_REOPEN_DELAY_M; }

    // Set non-zero when valve flow is increased, and then counts down to zero.
    // Some or all attempts to close the valve are deferred while this is non-zero
    // to reduce valve hunting if there is string turbulence from the radiator
    // or maybe draughts from open windows/doors
    // causing measured temperatures to veer up and down.
    // This attempts to reduce excessive valve noise and energy use
    // and help to avoid boiler short-cycling.
    uint8_t valveTurnupCountdownM;
    // Mark flow as having been increased.
    void valveTurnup() { valveTurnupCountdownM = ANTISEEK_VALVE_RECLOSE_DELAY_M; }

    // Cache of minValvePcReallyOpen value [0,99] to save some EEPROM access.
    // A value of 0 means not yet loaded from EEPROM.
    static uint8_t mVPRO_cache;

    // Set heat demand with some hysteresis and a hint of proportional control.
    // Always be willing to turn off quickly, but on slowly (AKA "slow start" algorithm),
    // and try to eliminate unnecessary 'hunting' which makes noise and uses actuator energy.
    bool computeRequiredTRVPercentOpen();

    // Compute target temperature and set heat demand for TRV and boiler.
    // CALL APPROXIMATELY ONCE PER MINUTE TO ALLOW SIMPLE TIME-BASED CONTROLS.
    // Inputs are inWarmMode(), isRoomLit().
    // The inputs must be valid (and recent).
    // Values set are targetTempC, value (TRVPercentOpen).
    // This may also prepare data such as TX command sequences for the TRV, boiler, etc.
    // This routine may take significant CPU time; no I/O is done, only internal state is updated.
    // Returns true if valve target changed and thus messages may need to be recomputed/sent/etc.
    bool computeCallForHeat();

  public:
    ModelledRadValve()
      : targetTempC(FROST),
        callingForHeat(false),
        valveMoved(false),
#if defined(TRV_SLEW_GLACIAL)
        glacial(true),
#else
        glacial(false),
#endif
        valveTurndownCountdownM(0), valveTurnupCountdownM(0)
      { }

    // Force a read/poll/recomputation of the target position and call for heat.
    // Sets/clears changed flag if computed valve position changed.
    // Call at a fixed rate (1/60s).
    // Potentially expensive/slow.
    virtual uint8_t read();

    // Returns preferred poll interval (in seconds); non-zero.
    // Must be polled at near constant rate, about once per minute.
    virtual uint8_t preferredPollInterval_s() const { return(60); }

    // Returns a suggested (JSON) tag/field/key name including units of get(); NULL means no recommended tag.
    // The lifetime of the pointed-to text must be at least that of the Sensor instance.
    virtual const char *tag() const { return("v|%"); }


    // Returns true if (re)calibrating/(re)initialising/(re)syncing.
    // The target valve position is not lost while this is true.
    // By default there is no recalibration step.
    virtual bool isRecalibrating() const;

    // If possible exercise the valve to avoid pin sticking and recalibrate valve travel.
    // Default does nothing.
    virtual void recalibrate();

    // True if the controlled physical valve is thought to be at least partially open right now.
    // If multiple valves are controlled then is this true only if all are at least partially open.
    // Used to help avoid running boiler pump against closed valves.
    // The default is to use the check the current computed position
    // against the minimum open percentage.
    // True iff the valve(s) (if any) controlled by this unit are really open.
    //
    // When driving a remote wireless valve such as the FHT8V,
    // this waits until at least the command has been sent.
    // This also implies open to DEFAULT_MIN_VALVE_PC_REALLY_OPEN or equivalent.
    // Must be exactly one definition/implementation supplied at link time.
    // If more than one valve is being controlled by this unit,
    // then this should return true if any of the valves are (significantly) open.
    virtual bool isControlledValveReallyOpen() const;

    // Get estimated minimum percentage open for significant flow [1,99] for this device.
    // Return global node value.
    virtual uint8_t getMinPercentOpen() const { return(getMinValvePcReallyOpen()); }

    // Get maximum allowed percent open [1,100] to limit maximim flow rate.
    // This may be important for systems such as district heat systems that charge by flow,
    // and other systems that prefer return temperatures to be as low as possible,
    // such as condensing boilers.
    uint8_t getMaxPercentageOpenAllowed() const { return(100); } // TODO: make settable/persistent.

    // Enable/disable 'glacial' mode (default false/off).
    // For heat-pump, district-heating and similar slow-reponse and pay-by-volume environments.
    // Also may help with over-powerful or unbalanced radiators
    // with a significant risk of overshoot.
    void setGlacialMode(bool glacialOn) { glacial = glacialOn; }

    // Returns true if this valve control is in glacial mode.
    bool inGlacialMode() const { return(glacial); }

    // True if the computed valve position was changed by read().
    // Can be used to trigger rebuild of messages, force updates to actuators, etc.
    bool isValveMoved() const { return(valveMoved); }

    // True if this unit is nominally calling for heat (temperature is under target).
    // Thread-safe and ISR safe.
    bool isCallingForHeat() const { return(callingForHeat); }

    // Get target temperature in C as computed by computeTargetTemperature().
    uint8_t getTargetTempC() const { return(targetTempC); }

    // Compute/update target temperature.
    // Can be called as often as required though may be slowish/expensive.
    // Can be called after any UI/CLI/etc operation
    // that may cause the target temperature to change.
    // (Will also be called by computeCallForHeat().)
    // One aim is to allow reasonable energy savings (10--30%)
    // even if the device is left in WARM mode all the time,
    // using occupancy/light/etc to determine when temperature can be set back
    // without annoying users.
    void computeTargetTemperature();

    // Return minimum valve percentage open to be considered actually/significantly open; [1,100].
    // This is a value that has to mean all controlled valves are at least partially open if more than one valve.
    // At the boiler hub this is also the threshold percentage-open on eavesdropped requests that will call for heat.
    // If no override is set then DEFAULT_MIN_VALVE_PC_REALLY_OPEN is used.
    static uint8_t getMinValvePcReallyOpen();

    // Set and cache minimum valve percentage open to be considered really open.
    // Applies to local valve and, at hub, to calls for remote calls for heat.
    // Any out-of-range value (eg >100) clears the override and DEFAULT_MIN_VALVE_PC_REALLY_OPEN will be used.
    static void setMinValvePcReallyOpen(uint8_t percent);
  };
// Singleton implementation for entire node.
extern ModelledRadValve NominalRadValve;




// Default maximum time to allow the boiler to run on to allow for lost call-for-heat transmissions etc.
// Should be (much) greater than the gap between transmissions (eg ~2m for FHT8V/FS20).
// Should be greater than the run-on time at the OpenTRV boiler unit and any further pump run-on time.
// Valves may have to linger open at minimum of this plus maybe an extra minute or so for timing skew
// for systems with poor/absent bypass to avoid overheating.
// Having too high a linger time value may cause excessive temperature overshoot.
#define DEFAULT_MAX_RUN_ON_TIME_M 5

// If defined then turn off valve very slowly after stopping call for heat (ie when shutting) which
// may allow comfortable bolier pump overrun in older systems with no/poor bypass to avoid overheating.
// In any case this should help reduce strain on circulation pumps, etc.
#define VALVE_TURN_OFF_LINGER

//// True if this unit is nominally calling for heat (temperature is under target).
//// Thread-safe.
//bool isCallingForHeat();

//// Compute target temperature.
//// Can be called as often as require though may be slow/expensive.
//// Will be called by computeCallForHeat().
//// One aim is to allow reasonable energy savings (10--30%)
//// even if the device is left in WARM mode all the time,
//// using occupancy/light/etc to determine when temperature can be set back
//// without annoying users.
//void computeTargetTemperature();

//// Compute target temperature and set heat demand for TRV and boiler.
//// CALL APPROXIMATELY ONCE PER MINUTE TO ALLOW SIMPLE TIME-BASED CONTROLS.
//// Inputs are inWarmMode(), isRoomLit().
//// The inputs must be valid (and recent).
//// Values set are targetTempC, TRVPercentOpen.
//// This may also prepare data such as TX command sequences for the TRV, boiler, etc.
//// This routine may take significant CPU time; no I/O is done, only internal state is updated.
//// Returns true if valve target changed and thus messages may need to be recomputed/sent/etc.
//bool computeCallForHeat();


#ifdef ENABLE_ANTICIPATION
// Returns true if system is in 'learn'/smart mode.
// If in 'smart' mode then the unit can anticipate user demand
// to pre-warm rooms, maintain customary temperatures, etc.
// Currently true if any simple schedule is set.
bool inSmartMode();
#endif



// IF DEFINED: support for general timed and multi-input occupancy detection / use.
#ifdef OCCUPANCY_SUPPORT

// Number of minutes that room is regarded as occupied after markAsOccupied(); strictly positive.
// DHD20130528: no activity for ~30 minutes usually enough to declare room empty in my experience.
// Should probably be at least as long as, or a little longer than, the BAKE timeout.
// Should probably be significantly shorter than normal 'learn' on time to allow savings from that in empty rooms.
#define OCCUPATION_TIMEOUT_M (min(max(SETBACK_FULL_M, 30), 255))
// Threshold from 'likely' to 'probably'.
#define OCCUPATION_TIMEOUT_1_M ((OCCUPATION_TIMEOUT_M*2)/3)

// Occupancy measure as a % confidence that the room/area controlled by this unit has active human occupants.
class OccupancyTracker : public SimpleTSUint8Sensor
  {
  private:
    // Time until room regarded as unoccupied, in minutes; initially zero (ie treated as unoccupied at power-up).
    // Marked voilatile for thread-safe lock-free non-read-modify-write access to byte-wide value.
    // Compound operations must block interrupts.
    volatile uint8_t occupationCountdownM;

    // Non-zero if occpuancy system recently notified of activity.
    // Marked voilatile for thread-safe lock-free non-read-modify-write access to byte-wide value.
    // Compound operations must block interrupts.
    volatile uint8_t activityCountdownM;

    // Hours and minutes since room became vacant (doesn't roll back to zero from max hours); zero when room occupied.
    uint8_t vacancyH;
    uint8_t vacancyM;

  public:
    OccupancyTracker() : occupationCountdownM(0), activityCountdownM(0), vacancyH(0), vacancyM(0) { }

    // Force a read/poll of the occupancy and return the % likely occupied [0,100].
    // Potentially expensive/slow.
    // Not thread-safe nor usable within ISRs (Interrupt Service Routines).
    // Poll at a fixed rate.
    virtual uint8_t read();

    // Returns true if this sensor reading value passed is potentially valid, eg in-range.
    // True if in range [0,100].
    virtual bool isValid(uint8_t value) const { return(value <= 100); }

    // This routine should be called once per minute.
    virtual uint8_t preferredPollInterval_s() const { return(60); }

    // Recommended JSON tag for full value; not NULL.
    virtual const char *tag() const { return("occ|%"); }

    // True if activity/occupancy recently reported (within last couple of minutes).
    // Includes weak and strong reports.
    // Thread-safe.
    bool reportedRecently() { return(0 != activityCountdownM); }

    // Returns true if the room appears to be likely occupied (with active users) now.
    // Operates on a timeout; calling markAsOccupied() restarts the timer.
    // Defaults to false (and API still exists) when OCCUPANCY_SUPPORT not defined.
    // Thread-safe.
    bool isLikelyOccupied() { return(0 != occupationCountdownM); }

    // Returns true if the room appears to be likely occupied (with active users) recently.
    // This uses the same timer as isOccupied() (restarted by markAsOccupied())
    // but returns to false somewhat sooner for example to allow ramping up more costly occupancy detection methods
    // and to allow some simple graduated occupancy responses.
    // Thread-safe.
    bool isLikelyRecentlyOccupied() { return(occupationCountdownM > OCCUPATION_TIMEOUT_1_M); }

    // False if room likely currently unoccupied (no active occupants).
    // Defaults to false (and API still exists) when OCCUPANCY_SUPPORT not defined.
    // This may require a substantial time after activity stops to become true.
    // This and isLikelyOccupied() cannot be true together; it is possible for neither to be true.
    // Thread-safe.
    bool isLikelyUnoccupied() { return(!isLikelyOccupied()); }

    // Call when some evidence of room occupation has occurred.
    // Do not call based on internal/synthetic events.
    // Such evidence may include operation of buttons (etc) on the unit or PIR.
    // Do not call from (for example) 'on' schedule change.
    // Makes occupation immediately visible.
    // Thread-safe and ISR-safe.
    void markAsOccupied() { value = 100; occupationCountdownM = OCCUPATION_TIMEOUT_M; activityCountdownM = 2; }

    // Call when some/weak evidence of room occupation, such as a light being turned on.
    // Do not call based on internal/synthetic events.
    // Doesn't force the room to appear recently occupied.
    // Thread-safe.
    void markAsPossiblyOccupied();

    // Two-bit occupancy: (00 not disclosed,) 1 not occupied, 2 possibly occupied, 3 probably occupied.
    // 0 is not returned by this implementation.
    // Thread-safe.
    uint8_t twoBitOccupancyValue() { return(isLikelyRecentlyOccupied() ? 3 : (isLikelyOccupied() ? 2 : 1)); }

    // Recommended JSON tag for two-bit occupancy value; not NULL.
    const char *twoBitTag() { return("O"); }

    // Returns true if it is worth expending extra effort to check for occupancy.
    // This will happen when confidence in occupancy is not yet zero but is approaching,
    // so checking more thoroughly now can help maintain non-zero value if someone is present and active.
    // At other times more relaxed checking (eg lower power) can be used.
    bool increaseCheckForOccupancy() { return(!isLikelyRecentlyOccupied() && isLikelyOccupied() && !reportedRecently()); }

    // Get number of hours room vacant, zero when room occupied; does not wrap.
    // If forced to zero as soon as occupancy is detected.
    uint16_t getVacancyH() { return((value != 0) ? 0 : vacancyH); }

    // Recommended JSON tag for vacancy hours; not NULL.
    const char *vacHTag() { return("vac|h"); }

    // Returns true if room appears to have been vacant for over a day.
    // For a home or an office no sign of acticity for this long suggests a weekend or a holiday for example.
    bool longVacant() { return(getVacancyH() > 24); }

    // Returns true if room appears to have been vacant for over a two days.
    // For a home or an office no sign of acticity for this long suggests a long weekend or a holiday for example.
    bool longLongVacant() { return(getVacancyH() > 48); }
  };
#else
// Placeholder namespace with dummy static status methods to reduce code complexity.
class OccupancyTracker
  {
  public:
    static void markAsOccupied() {} // Defined as NO-OP for convenience when no general occupancy support.
    static void markAsPossiblyOccupied() {} // Defined as NO-OP for convenience when no general occupancy support.
    static bool isLikelyRecentlyOccupied() { return(false); } // Always false without OCCUPANCY_SUPPORT
    static bool isLikelyOccupied() { return(false); } // Always false without OCCUPANCY_SUPPORT
    static bool isLikelyUnoccupied() { return(false); } // Always false without OCCUPANCY_SUPPORT
    static uint8_t twoBitOccValue() { return(0); } // Always 0 without OCCUPANCY_SUPPORT.
    static uint16_t getVacancyH() { return(0); } // Always 0 without OCCUPANCY_SUPPORT.
    static bool longVacant() { return(false); } // Always false without OCCUPANCY_SUPPORT.
    static bool longLongVacant() { return(false); } // Always false without OCCUPANCY_SUPPORT.
  };
#endif
// Singleton implementation for entire node.
extern OccupancyTracker Occupancy;



// Sample statistics once per hour as background to simple monitoring and adaptive behaviour.
// Call this once per hour with fullSample==true, as near the end of the hour as possible;
// this will update the non-volatile stats record for the current hour.
// Optionally call this at a small (2--10) even number of evenly-spaced number of other times thoughout the hour
// with fullSample=false to sub-sample (and these may receive lower weighting or be ignored).
// (EEPROM wear should not be an issue at this update rate in normal use.)
void sampleStats(bool fullSample);

// Clear all collected statistics, eg when moving device to a new room or at a major time change.
// Requires 1.8ms per byte for each byte that actually needs erasing.
//   * maxBytesToErase limit the number of bytes erased to this; strictly positive, else 0 to allow 65536
// Returns true if finished with all bytes erased.
bool zapStats(uint16_t maxBytesToErase = 0);

// Get raw stats value for hour HH [0,23] from stats set N from non-volatile (EEPROM) store.
// A value of 0xff (255) means unset (or out of range); other values depend on which stats set is being used.
uint8_t getByHourStat(uint8_t hh, uint8_t statsSet);

// Returns true if specified hour is (conservatively) in the specifed outlier quartile for specified stats set.
// Returns false if a full set of stats not available, eg including the specified hour.
// Always returns false if all samples are the same.
//   * inTop  test for membership of the top quartile if true, bottom quartile if false
//   * statsSet  stats set number to use.
//   * hour  hour of day to use or ~0 for current hour.
bool inOutlierQuartile(uint8_t inTop, uint8_t statsSet, uint8_t hour = ~0);

// 'Unset'/invalid values for byte (eg raw EEPROM byte) and int (eg after decompression).
#define STATS_UNSET_BYTE 0xff
#define STATS_UNSET_INT 0x7fff

#ifdef ENABLE_ANTICIPATION
// Returns true iff room likely to be occupied and need warming at the specified hour's sample point based on collected stats.
// Used for predictively warming a room in smart mode and for choosing setback depths.
// Returns false if no good evidence to warm the room at the given time based on past history over about one week.
//   * hh hour to check for predictive warming [0,23]
bool shouldBeWarmedAtHour(const uint_least8_t hh);
#endif



#ifdef UNIT_TESTS
// Compute new linearly-smoothed value given old smoothed value and new value.
// Guaranteed not to produce a value higher than the max of the old smoothed value and the new value.
// Uses stochastic rounding to nearest to allow nominally sub-lsb values to have an effect over time.
// Usually only made public for unit testing.
uint8_t smoothStatsValue(uint8_t oldSmoothed, uint8_t newValue);
#endif

// Range-compress an signed int 16ths-Celsius temperature to a unsigned single-byte value < 0xff.
// This preserves at least the first bit after the binary point for all values,
// and three bits after binary point for values in the most interesting mid range around normal room temperatures,
// with transitions at whole degrees Celsius.
// Input values below 0C are treated as 0C, and above 100C as 100C, thus allowing air and DHW temperature values.
#define COMPRESSION_C16_FLOOR_VAL 0 // Floor input value to compression.
#define COMPRESSION_C16_LOW_THRESHOLD (16<<4) // Values in range [COMPRESSION_LOW_THRESHOLD_C16,COMPRESSION_HIGH_THRESHOLD_C16[ have maximum precision.
#define COMPRESSION_C16_LOW_THR_AFTER (COMPRESSION_C16_LOW_THRESHOLD>>3) // Low threshold after compression.
#define COMPRESSION_C16_HIGH_THRESHOLD (24<<4)
#define COMPRESSION_C16_HIGH_THR_AFTER (COMPRESSION_C16_LOW_THR_AFTER + ((COMPRESSION_C16_HIGH_THRESHOLD-COMPRESSION_C16_LOW_THRESHOLD)>>1)) // High threshold after compression.
#define COMPRESSION_C16_CEIL_VAL (100<<4) // Ceiling input value to compression.
#define COMPRESSION_C16_CEIL_VAL_AFTER (COMPRESSION_C16_HIGH_THR_AFTER + ((COMPRESSION_C16_CEIL_VAL-COMPRESSION_C16_HIGH_THRESHOLD) >> 3)) // Ceiling input value after compression.
uint8_t compressTempC16(int tempC16);
// Reverses range compression done by compressTempC16(); results in range [0,100], with varying precision based on original value.
// 0xff (or other invalid) input results in STATS_UNSET_INT. 
int expandTempC16(uint8_t cTemp);

// Maximum valid encoded/compressed stats values.
#define MAX_STATS_TEMP COMPRESSION_C16_CEIL_VAL_AFTER // Maximum valid compressed temperature value in stats.
#define MAX_STATS_AMBLIGHT 254 // Maximum valid ambient light value in stats (very top of range is compressed).



// Clear and populate core stats structure with information from this node.
// Exactly what gets filled in will depend on sensors on the node,
// and may depend on stats TX security level (if collecting some sensitive items is also expensive).
void populateCoreStats(FullStatsMessageCore_t *content);


#endif

