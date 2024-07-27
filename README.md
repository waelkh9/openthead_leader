This is the leader thread device that will initiate the network. 
Hardware Requirements ESP32C6: The main microcontroller for interfacing with sensors and handling Zigbee communication. 
BME680 Sensor: Used to measure temperature, humidity, pressure, and gas resistance. 
BH1750 Sensor: Used to measure light intensity (lux). 
I2C Connection: Ensure proper wiring between the ESP32 and the sensors using I2C protocol. 
Software Requirements ESP-IDF: The official development framework for the ESP32.
Available through ESP-IDF components. 



The OpenThread CLI (command line interface) provides access to configuration and management APIs through a command-line interface. its possible to use the OT CLI to set up
an OpenThread development environment. In the case of this project, the command line
interface will be used to initialize and setup the thread network. After flashing the code on
the leader microcontroller, the first step is to initialize the dataset by running the following
command:

– dataset init new
Then this changes must be applied by running the following command, to commit the
active dataset that was initialized :
– ”dataset commit active”
Next, the thread network can be started by running these commands:
– ”ifconfig up” followed by thread start
Sources: 
https://openthread.io/reference/cli/concepts/dataset
https://devzone.nordicsemi.com/f/nordic-q-a/42117/cli-example-openthread-2-0-0---udp-data-transfer-from-main-c

https://stackoverflow.com/questions/54118223/openthread-cli-udp-communication-from-main-c-nrf52840
https://devzone.nordicsemi.com/f/nordic-q-a/88037/udp-on-openthread-is-not-received

https://www.youtube.com/watch?v=A9c9moP_WNw


