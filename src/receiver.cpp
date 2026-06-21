#include "receiver.h"
#include "config.h"
#include "sensor.h"
#include "bark.h"
#include "eventlog.h"

// ============================================================================
// Laser-beam receiver — see include/receiver.h. Pure configuration; the
// polling / debounce / logging / notification logic lives in sensor.cpp.
// ============================================================================
//
// The beam is RECEIVED at level RECEIVER_BEAM_HIGH, so the "alert" (beam broken)
// state is the opposite level. The pull resistor is chosen so a DISCONNECTED
// receiver reads as "beam broken" (fail-safe for a tripwire):
//   beam HIGH  -> alert LOW  -> INPUT_PULLDOWN (idle reads LOW  = broken)
//   beam LOW   -> alert HIGH -> INPUT_PULLUP   (idle reads HIGH = broken)

// Runtime beam-present level (defaults to the config.h value; overridden by the
// persisted setting at boot and by /api/receiver/config live). beamHigh decides
// both the alert polarity and the input pull — see applyBeamPolarity().
static bool beamHigh = RECEIVER_BEAM_HIGH;

static Sensor beam = {
  /* enabled       */ RECEIVER_ENABLED,
  /* pin           */ RECEIVER_PIN,
  /* inputMode     */ RECEIVER_BEAM_HIGH ? INPUT_PULLDOWN : INPUT_PULLUP,
  /* alertHigh     */ RECEIVER_BEAM_HIGH ? false : true,
  /* pollMs        */ RECEIVER_POLL_MS,
  /* debounceMs    */ 30,
  /* logSource     */ LOG_BEAM,
  /* barkSource    */ BARK_SRC_LASER,
  /* tag           */ "beam",
  /* alertMsg      */ "Beam broken",
  /* clearMsg      */ "Beam restored",
  /* barkTitle     */ BARK_BEAM_TITLE,
  /* barkBody      */ BARK_BEAM_BODY,
  /* detectDelayMs */ RECEIVER_DEFAULT_DELAY_MS,
  /* runtime ...   */ false, false, 0, 0, 0, 0, 0,
};

// Map the beam-present level onto the generic Sensor's alert polarity + pull:
//   beam HIGH -> alert LOW  -> INPUT_PULLDOWN (idle/disconnected reads LOW  = broken)
//   beam LOW  -> alert HIGH -> INPUT_PULLUP   (idle/disconnected reads HIGH = broken)
static void applyBeamPolarity() {
  sensorSetInput(beam, beamHigh ? INPUT_PULLDOWN : INPUT_PULLUP, beamHigh ? false : true);
}

void          receiverBegin()                 { sensorBegin(beam); }
void          receiverUpdate()                { sensorUpdate(beam); }
bool          receiverEnabled()               { return beam.enabled; }
bool          receiverActive()                { return sensorActive(beam); }
void          receiverSetDelay(unsigned long ms) { sensorSetDelay(beam, ms); }
unsigned long receiverDelay()                 { return sensorDelay(beam); }
void          receiverSetBeamHigh(bool high)  { beamHigh = high; applyBeamPolarity(); }
bool          receiverBeamHigh()              { return beamHigh; }

// sensorStatusJson + the runtime beam-present level, so the WebUI can show and
// flip the polarity. Pass through verbatim when the receiver is compiled out.
String receiverStatusJson() {
  String j = sensorStatusJson(beam);
  if (!beam.enabled) return j;          // "{\"enabled\":false}"
  j.remove(j.length() - 1);             // drop the trailing '}'
  j += ",\"beamHigh\":" + String(beamHigh ? 1 : 0) + "}";
  return j;
}
