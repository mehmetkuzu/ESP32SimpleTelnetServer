# ESP32SimpleTelnetServer
A simple telnet server running on Nodemcu. 

Responds standart commands. 

Proxies to USB. 

Extendible.

## Major points

- secrets.h is for wifi configuration. More than one SSID and PSA Key may be stored and used/tried. The file is ignored in the repository, but a template/sample file is given.
- telnet clients are used to connect. But options should be studied so that the client does not send handshake or negotiation signals.
- Code includes the ArduinoOTA handle. Which makes possible to upload sketches on-the-air. (Via network)
