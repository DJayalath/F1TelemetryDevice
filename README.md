# F1TelemetryDevice
An IoT device based on ESP8266 for displaying telemetry from Codemasters' F1 games. This program uses UDP telemetry packets broadcast from Codemasters' F1 games to calculate and display useful statistics on any panel using an SPI interface. The code provided has been tested on a Wemos D1 Mini and NodeMCU, it follows that it should work correctly on any derivative boards.

<img src="https://thumbs.gfycat.com/PlainPaleBullmastiff-small.gif" width='640'>

## Planning

Inspired by steering wheels in Formula 1, I wanted to create a mod for my Logitech G29 gaming wheel which added an onboard display. This display should show useful data such as tyre wear, tyre temperature, and estimated fuel usage during a race in the game F1 2018 on my PS4.

### Gathering Telemetry Data

Codemasters' F1 series of games, as well as many other racing simulators and arcades, have an option to broadcast telemetry. This sends UDP data packets containing information about the session, lap, car and other drivers to any local IP address of one's choosing. Alternatively, it can broadcast this data to all devices through the subnet mask. The data is sent in the form of a header which specifies the type of packet alongside the rest of the packet's data. I could receive this data by using a packed struct in C++ and reading the exact number of bytes in the packet into the associated struct I had created.

### Receiving Telemetry Data

Since the device needs to have a relatively small form factor in order to fit on the steering wheel, the usual options such as a Rasberry Pi, or even a Pi Zero were unrealistic. Instead, I chose to use an ESP8266 microcontroller on a Wemos D1 Mini board. This is both more powerful than a typical ATMega328P (so can drive a display at an acceptable refresh rate) and has onboard WiFi in order to receive the data packets. This is far more convenient and space-saving than using an addon for WiFI with an alternative board.

### Displaying Telemetry Data

The G29 does not have large wheel and only has a small area of open space that could fit a display. For this reason, I opted for a 3.2 inch LCD panel which uses an SPI interface. The SPI interface ensures it is easy to connect and use with the ESP8266. The panel interfaces with the ILI9341 driver which has a well-maintained library for my device.

## Designing

My initial idea for the logic of the system was simple - a microcontroller connected to a display. However, on further contemplation, it became obvious that this was an oversimplification. Powering the device was a serious consideration since when the wheel was turned, any connected wire would tangle (especially since the G29 has a maximum of 900 degrees of rotation). The simplest solution to this was to make the device battery powered. Lithium Ion was, in my opinion, the best option here because of the low discharge and degredation alongside a high energy density. Further to this, Lithium Polymer was a much more expensive option considering the cost per unit capacity. The batteries can simply be charged while the wheel is not in use with a cable.

With the feasibility considerations made, I knew exactly what parts I'd need for the project:

-    Wemos D1 Mini
-    3.2" SPI TFT Display
-    2x 18650 9600mAh Batteries
-    18650 Charging Shield
-    Voltage Regulator (for 5v supply to D1 Mini)
-    SPST Toggle Switch

Additionally, the next challenge was connecting all of this together in a permanent way. The best option was a custom printed circuit board. So, I set about designing a compact PCB that could fit everything but the battery shield under the space encompassed by the display.
