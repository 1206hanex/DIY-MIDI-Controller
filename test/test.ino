// GP-5 USB MIDI host test for Adafruit Feather RP2040 with USB Type A Host
//
// Proves the Feather can send MIDI to a Valeton GP-5 over the USB-A host port.
// Every 2 seconds it sends CC#0 = 1, 2, 3 (preset recall) on MIDI channel 1.
//
// Arduino IDE settings (Tools menu):
//   Board:     Adafruit Feather RP2040 USB Host
//   USB Stack: Adafruit TinyUSB
//   CPU Speed: 120 MHz
//
// Libraries needed: Adafruit TinyUSB Library, Pico PIO USB.

#include "pio_usb.h"
#include "Adafruit_TinyUSB.h"
#include "hardware/clocks.h"

// Pin defaults for the Feather RP2040 USB Host (same fallbacks as Adafruit's usbh_helper.h)
#ifndef PIN_USB_HOST_DP
#define PIN_USB_HOST_DP 16   // D+ on GPIO 16, D- on GPIO 17
#endif
#ifndef PIN_5V_EN
#define PIN_5V_EN 18         // enables 5V on the USB-A port
#endif
#ifndef PIN_5V_EN_STATE
#define PIN_5V_EN_STATE 1
#endif

Adafruit_USBH_Host USBHost;

// TinyUSB MIDI host index of the GP-5; -1 means nothing is connected
volatile int8_t midiIdx = -1;

const uint8_t MIDI_CHANNEL = 0;          // 0 = MIDI channel 1 (GP-5 only listens on ch 1)
const uint8_t TEST_PRESETS[] = {1, 2, 3};
const uint32_t SEND_INTERVAL_MS = 2000;

//--------------------------------------------------------------------+
// Core 0: USB serial for debug output only
//--------------------------------------------------------------------+
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) delay(10);  // wait up to 3 s for Serial Monitor
  Serial.println("GP-5 MIDI host test starting");
}

void loop() {
  // nothing - all USB host work runs on core 1
}

//--------------------------------------------------------------------+
// Core 1: USB host (PIO USB on the Type A port)
//--------------------------------------------------------------------+
void setup1() {
  // PIO USB needs a CPU clock that is a multiple of 12 MHz
  uint32_t cpu_hz = clock_get_hz(clk_sys);
  if (cpu_hz != 120000000UL && cpu_hz != 240000000UL) {
    while (!Serial) delay(10);
    Serial.printf("CPU is %lu Hz. Set Tools > CPU Speed to 120 MHz and re-upload.\r\n", cpu_hz);
    while (1) delay(1);
  }

  // Turn on 5V power to the USB-A host port
  pinMode(PIN_5V_EN, OUTPUT);
  digitalWrite(PIN_5V_EN, PIN_5V_EN_STATE);

  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = PIN_USB_HOST_DP;  // GPIO 16 on this board
  USBHost.configure_pio_usb(1, &pio_cfg);
  USBHost.begin(1);
}

void sendControlChange(uint8_t cc, uint8_t value) {
  uint8_t msg[3] = { (uint8_t)(0xB0 | MIDI_CHANNEL), cc, value };
  tuh_midi_stream_write((uint8_t)midiIdx, 0, msg, sizeof(msg));
  tuh_midi_write_flush((uint8_t)midiIdx);
}

void loop1() {
  USBHost.task();

  static uint32_t lastSend = 0;
  static uint8_t step = 0;

  if (midiIdx >= 0 && millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();
    uint8_t preset = TEST_PRESETS[step];
    sendControlChange(0, preset);
    Serial.printf("Sent CC#0 = %u on channel %u\r\n", preset, MIDI_CHANNEL + 1);
    step = (step + 1) % sizeof(TEST_PRESETS);
  }
}

//--------------------------------------------------------------------+
// TinyUSB host callbacks
//--------------------------------------------------------------------+
extern "C" {

void tuh_mount_cb(uint8_t daddr) {
  uint16_t vid = 0, pid = 0;
  tuh_vid_pid_get(daddr, &vid, &pid);
  Serial.printf("USB device attached: address %u, VID %04x, PID %04x\r\n", daddr, vid, pid);
}

void tuh_umount_cb(uint8_t daddr) {
  Serial.printf("USB device removed: address %u\r\n", daddr);
}

void tuh_midi_mount_cb(uint8_t idx, const tuh_midi_mount_cb_t* mount_cb_data) {
  (void)mount_cb_data;
  Serial.printf("MIDI interface ready (index %u) - starting test messages\r\n", idx);
  midiIdx = (int8_t)idx;
}

void tuh_midi_umount_cb(uint8_t idx) {
  Serial.printf("MIDI interface %u disconnected\r\n", idx);
  if (midiIdx == (int8_t)idx) midiIdx = -1;
}

}  // extern "C"
