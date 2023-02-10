# smart-thermostat

The idea of this project is to create a truly smart thermostat.
Main features are:

* Can be integrated into a home automation system (such as Home Assistant)
* Works independently of being cloud connected
* Wifi connected
* Bluetooth LE connected (required for initial setup)
* Will act similarly to a person's preferences when it comes to HVAC. For example:
    * the temperature set should be dependent upon local forecast
    * If the outside temp is close to the set thermostat temp, suggest opening a window
    * Detect if a window or door is open and disable the heat/AC
* Use Matter protocol
* Provide local web site to control thermostat
* Allow ssh login
* Detect degradation of temp rise/fall to identify clogged filter or service required
* Employ remote temp sensors to balance temp within home
* Detect lower air quality to suggest opening a window (and disable HVAC)

Basic layout of thermostat:
<p align="center">
  <img src="./Block%20Diagram.drawio.png">
</p>