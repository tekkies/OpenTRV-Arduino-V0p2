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

Author(s) / Copyright (s): Deniz Erbilgin 2017
*/
/**
 * Minimal REV10 config for testing power consumption.
 * Aim is to:
 *     - init GPIO pins to safe mode.
 *     - init peripherals to safe low power mode.
 *     - toggle LED
 *     - loop endlessly, toggling LED, reading sensors and listening on radio.
 */

// INCLUDES & DEFINES
// Debug output flag
#define DEBUG
// REV11 as Sensor with secure TX.
#define CONFIG_REV10_SECURE_BHR
// Get defaults for valve applications.
#include <OTV0p2_valve_ENABLE_defaults.h>
// All-in-one valve unit (DORM1).
#include <OTV0p2_CONFIG_REV11.h>
// I/O pin allocation and setup: include ahead of I/O module headers.
#include <OTV0p2_Board_IO_Config.h>
// OTV0p2Base Libraries
#include <OTV0p2Base.h>
// Radio Libraries
//#include <OTRadioLink.h>
#include <OTRFM23BLink.h>
// RadValve libraries
#include <OTRadValve.h>


// Debugging output
#ifndef DEBUG
#define DEBUG_SERIAL_PRINT(s) // Do nothing.
#define DEBUG_SERIAL_PRINTFMT(s, format) // Do nothing.
#define DEBUG_SERIAL_PRINT_FLASHSTRING(fs) // Do nothing.
#define DEBUG_SERIAL_PRINTLN_FLASHSTRING(fs) // Do nothing.
#define DEBUG_SERIAL_PRINTLN() // Do nothing.
#define DEBUG_SERIAL_TIMESTAMP() // Do nothing.
#else
// Send simple string or numeric to serial port and wait for it to have been sent.
// Make sure that Serial.begin() has been invoked, etc.
#define DEBUG_SERIAL_PRINT(s) { OTV0P2BASE::serialPrintAndFlush(s); }
#define DEBUG_SERIAL_PRINTFMT(s, fmt) { OTV0P2BASE::serialPrintAndFlush((s), (fmt)); }
#define DEBUG_SERIAL_PRINT_FLASHSTRING(fs) { OTV0P2BASE::serialPrintAndFlush(F(fs)); }
#define DEBUG_SERIAL_PRINTLN_FLASHSTRING(fs) { OTV0P2BASE::serialPrintlnAndFlush(F(fs)); }
#define DEBUG_SERIAL_PRINTLN() { OTV0P2BASE::serialPrintlnAndFlush(); }
// Print timestamp with no newline in format: MinutesSinceMidnight:Seconds:SubCycleTime
extern void _debug_serial_timestamp();
#define DEBUG_SERIAL_TIMESTAMP() _debug_serial_timestamp()
#endif // DEBUG

// OBJECTS & VARIABLES
/**
 * Peripherals present on REV7
 * - PHT        read from in loop
 * - LED        Toggle
 * - TMP112     read from in loop.
 * - RFM23B     read from in loop
 * - SIM900     TODO
 * - Boiler     TODO
 * - XTAL       Setup and leave running?
 * - UART       print sensor values
 */

/**
 * Dummy Types
 */
// Placeholder class with dummy static status methods to reduce code complexity.
typedef OTV0P2BASE::DummySensorOccupancyTracker OccupancyTracker;

/**
 * Supply Voltage instance
 */
// Sensor for supply (eg battery) voltage in millivolts.
// Singleton implementation/instance.
OTV0P2BASE::SupplyVoltageCentiVolts Supply_cV;

/*
 * RFM23B instance
 */
static constexpr bool RFM23B_allowRX = true;
 
static constexpr uint8_t RFM23B_RX_QUEUE_SIZE = OTRFM23BLink::DEFAULT_RFM23B_RX_QUEUE_CAPACITY;
static constexpr int8_t RFM23B_IRQ_PIN = PIN_RFM_NIRQ;
OTRFM23BLink::OTRFM23BLink<OTV0P2BASE::V0p2_PIN_SPI_nSS, RFM23B_IRQ_PIN, RFM23B_RX_QUEUE_SIZE, RFM23B_allowRX> PrimaryRadio;
// Pick an appropriate radio config for RFM23 (if it is the primary radio).
// Nodes talking on fast GFSK channel 0.
static const uint8_t nPrimaryRadioChannels = 1;
static const OTRadioLink::OTRadioChannelConfig RFM23BConfigs[nPrimaryRadioChannels] = {
    // GFSK channel 0 full config, RX/TX, not in itself secure.
    OTRadioLink::OTRadioChannelConfig(OTRFM23BLink::StandardRegSettingsGFSK57600, true), };


/**
 * SIM900 instance
 */
// SIM card configs
static const char SIM900_PIN[5] PROGMEM       = "1111";
static const char SIM900_APN[] PROGMEM      = "\"mobiledata\""; // GeoSIM
// UDP Configs - Edit SIM900_UDP_ADDR for relevant server. NOTE: The server IP address should never be committed to GitHub.
static const char SIM900_UDP_ADDR[16] PROGMEM = ""; // Of form "1.2.3.4".
static const char SIM900_UDP_PORT[5] PROGMEM = "9999";             // Standard port for OpenTRV servers
const OTSIM900Link::OTSIM900LinkConfig_t SIM900Config(
                                                false,
                                                SIM900_PIN,
                                                SIM900_APN,
                                                SIM900_UDP_ADDR,
                                                SIM900_UDP_PORT);
OTSIM900Link::OTSIM900Link<8, 5, RADIO_POWER_PIN, OTV0P2BASE::getSecondsLT> SecondaryRadio; // (REGULATOR_POWERUP, RADIO_POWER_PIN);



/*
 * TMP112 instance
 */
OTV0P2BASE::RoomTemperatureC16_TMP112 TemperatureC16;

/**
 * Ambient Light Sensor
 */
typedef OTV0P2BASE::SensorAmbientLight AmbientLight;
// Singleton implementation/instance.
AmbientLight AmbLight;

// FUNCTIONS

/**
 * Panic Functions
 */
// Indicate that the system is broken in an obvious way (distress flashing the main LED).
// DOES NOT RETURN.
// Tries to turn off most stuff safely that will benefit from doing so, but nothing too complex.
// Tries not to use lots of energy so as to keep distress beacon running for a while.
void panic()
{
    // Reset radio and go into low-power mode.
    PrimaryRadio.panicShutdown();
    // Power down almost everything else...
    OTV0P2BASE::minimisePowerWithoutSleep();
    pinMode(OTV0P2BASE::LED_HEATCALL_L, OUTPUT);
    for( ; ; )
    {
        OTV0P2BASE::LED_HEATCALL_ON();
        OTV0P2BASE::nap(WDTO_15MS);
        OTV0P2BASE::LED_HEATCALL_OFF();
        OTV0P2BASE::nap(WDTO_120MS);
    }
}
// Panic with fixed message.
void panic(const __FlashStringHelper *s)
{
    OTV0P2BASE::serialPrintlnAndFlush(); // Start new line to highlight error.  // May fail.
    OTV0P2BASE::serialPrintAndFlush('!'); // Indicate error with leading '!' // May fail.
    OTV0P2BASE::serialPrintlnAndFlush(s); // Print supplied detail text. // May fail.
    panic();
}


/**
 * @brief   Set pins and on-board peripherals to safe low power state.
 */


//========================================
// SETUP
//========================================

// Setup routine: runs once after reset.
// Does some limited board self-test and will panic() if anything is obviously broken.
void setup()
{
    // Set appropriate low-power states, interrupts, etc, ASAP.
    OTV0P2BASE::powerSetup();
    // IO setup for safety, and to avoid pins floating.
    OTV0P2BASE::IOSetup();

    OTV0P2BASE::serialPrintAndFlush(F("\r\nOpenTRV: ")); // Leading CRLF to clear leading junk, eg from bootloader.
    V0p2Base_serialPrintlnBuildVersion();

    OTV0P2BASE::LED_HEATCALL_ON();

    // Give plenty of time for the XTal to settle.
    delay(1000);

    // Have 32678Hz clock at least running before going any further.
    // Check that the slow clock is running reasonably OK, and tune the fast one to it.
    if(!::OTV0P2BASE::HWTEST::calibrateInternalOscWithExtOsc()) { panic(F("Xtal")); } // Async clock not running or can't tune.
//    if(!::OTV0P2BASE::HWTEST::check32768HzOsc()) { panic(F("xtal")); } // Async clock not running correctly.

    // Initialise the radio, if configured, ASAP because it can suck a lot of power until properly initialised.
    PrimaryRadio.preinit(NULL);
    // Check that the radio is correctly connected; panic if not...
    if(!PrimaryRadio.configure(nPrimaryRadioChannels, RFM23BConfigs) || !PrimaryRadio.begin()) { panic(F("r1")); }

  // Turn power on for SIM900 with PFET for secondary power control.
    fastDigitalWrite(A3, 0);
    pinMode(A3, OUTPUT);
  // Initialise the radio, if configured, ASAP because it can suck a lot of power until properly initialised.
    SecondaryRadio.preinit(NULL);
//  DEBUG_SERIAL_PRINTLN_FLASHSTRING("R2");
  // Check that the radio is correctly connected; panic if not...
    if(!SecondaryRadio.configure(1, &SecondaryRadioConfig) || !SecondaryRadio.begin()) { panic(F("r2")); }
  // Assume no RX nor filtering on secondary radio.

    // Collect full set of environmental values before entering loop() in normal mode.
    // This should also help ensure that sensors are properly initialised.

    // No external sensors are *assumed* present if running alt main loop
    // This may mean that the alt loop/POST will have to initialise them explicitly,
    // and the initial seed entropy may be marginally reduced also.
    const int cV = Supply_cV.read();
    DEBUG_SERIAL_PRINT_FLASHSTRING("V: ");
    DEBUG_SERIAL_PRINT(cV);
    DEBUG_SERIAL_PRINTLN();
    const int heat = TemperatureC16.read();
    DEBUG_SERIAL_PRINT_FLASHSTRING("T: ");
    DEBUG_SERIAL_PRINT(heat);
    DEBUG_SERIAL_PRINTLN();
    const int light = AmbLight.read();
    DEBUG_SERIAL_PRINT_FLASHSTRING("L: ");
    DEBUG_SERIAL_PRINT(light);
    DEBUG_SERIAL_PRINTLN();

    // Initialised: turn main/heatcall UI LED off.
    OTV0P2BASE::LED_HEATCALL_OFF();

    // Do OpenTRV-specific (late) setup.
    PrimaryRadio.listen(true);
    SecondaryRadio.listen(false);
}


//========================================
// MAIN LOOP
//========================================
/**
 * @brief   Sleep in low power mode if not sleeping.
 */
void loop()
{
    static bool LED_on;

    // Toggle LED.
    if(LED_on) { OTV0P2BASE::LED_HEATCALL_ON(); }
    else { OTV0P2BASE::LED_HEATCALL_OFF(); }

    // Repeatedly read and print sensor values for fun.
    const int heat = TemperatureC16.read();
    DEBUG_SERIAL_PRINT_FLASHSTRING("T: ");
    DEBUG_SERIAL_PRINT(heat);
    DEBUG_SERIAL_PRINTLN();
    const uint8_t rh = RelHumidity.read();
    DEBUG_SERIAL_PRINT_FLASHSTRING("RH%: ");
    DEBUG_SERIAL_PRINT(rh);
    DEBUG_SERIAL_PRINTLN();
    const int light = AmbLight.read();
    DEBUG_SERIAL_PRINT_FLASHSTRING("L: ");
    DEBUG_SERIAL_PRINT(light);
    DEBUG_SERIAL_PRINTLN();

    // Very crude poll/RX; likely to miss plenty.
    PrimaryRadio.poll();
    const uint8_t rxed = PrimaryRadio.getRXMsgsQueued();
    DEBUG_SERIAL_PRINT_FLASHSTRING("RXED: ");
    DEBUG_SERIAL_PRINT(rxed);
    DEBUG_SERIAL_PRINTLN();
    if(0 != rxed)
      {
      DEBUG_SERIAL_PRINT_FLASHSTRING("RX 1st byte: ");
      DEBUG_SERIAL_PRINTFMT(*PrimaryRadio.peekRXMsg(), HEX);
      DEBUG_SERIAL_PRINTLN();
      PrimaryRadio.removeRXMsg();
      }
    SecondaryRadio.poll();

    // =====
    // To sleep, perchance to dream...

    // Ensure that serial I/O is off while sleeping.
    OTV0P2BASE::powerDownSerial();
    // Power down most stuff (except radio for hub RX).
    OTV0P2BASE::minimisePowerWithoutSleep();
    // Normal long minimal-power sleep until wake-up interrupt.
    // Rely on interrupt to force quick loop round to I/O poll.
    OTV0P2BASE::sleepUntilInt();

    LED_on = !LED_on;
}

