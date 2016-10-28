# paulFanSpeed

1.) History
Release		Date		Changes
v0.1		2016-10-08	initial release
v0.3 		2016-10-24	configuration for LED panel/TFT touchscreen

2.) Description
Arduino fan speed control sketch for Paul ventilation unit Focus 200/Novus 300. These units uses RS485 serial communication in master-slave arrangement.

3.) Requirements
This sketch works with ethernet shield and custom made "paul relay shield". I have tested it with arduino mega board (because of hardware serial port), but it should also work with uno and software serial

4.) Setup
- download the zip file
- copy HardwareSerial into the libraries directory of your Arduino folder

5.) Usage
- build (and solder) your "paul relay shield"
- mount it together with arduino board, network shield
- connect it between control unit and adapter board
- configure your local network IP address for network shield in main file
- upload the sketch

6.) Paul relay shield

7.) Download

8.) Contribute
