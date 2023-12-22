# Proofuino

Proofuino is a project that combines hardware and software to create a Dough Proofing Box controlled by an ESP8266 using the Arduino platform. This setup utilizes a styrofoam box with internal components including a heating mat, relay, and temperature sensors to regulate and maintain the temperature for proofing dough.

## Features

- **Hardware Components:**
  - Styrofoam box housing the proofing environment
  - Heating mat controlled by a relay
  - ESP8266 microcontroller for temperature regulation and control
  - Two DS18b20 temperature sensors:
    - One to measure dough temperature
    - Another to measure ambient temperature inside the box

- **Operation Modes:**
  - *Boost Mode:* Initiates to raise the dough temperature to the desired level.
  - *Hold Mode:* Maintains the desired temperature for proofing.

- **Monitoring and Visualization:**
  - Raspberry Pi running a Grafana and InfluxDB Docker container for monitoring purposes.
  - The ESP8266 logs temperature readings, relay state, and the current mode to InfluxDB.
  - Grafana dashboard visualizes the proofing process with graphical representations.

## How It Works

The Proofuino system employs an ESP8266 microcontroller to manage the temperature inside the proofing box. The heating mat is controlled by a relay, allowing the ESP8266 to regulate the temperature by turning the heating mat on or off. Two DS18b20 temperature sensors aid in maintaining the desired dough temperature and preventing the box from overheating. Boost Mode is used initially to bring the dough to the desired temperature, while Hold Mode maintains the set temperature for proper proofing.

A Raspberry Pi is employed to oversee the proofing process through a Grafana dashboard, leveraging InfluxDB to store data logged by the ESP8266. This dashboard provides a visual representation of the temperature variations, relay states, and the current operational mode, offering insights into the proofing progress.

## Usage

To utilize Proofuino effectively, ensure the hardware components are properly assembled within the styrofoam box. Upload the Arduino code to the ESP8266 microcontroller to enable control and monitoring functionalities. Set up the Raspberry Pi with Grafana and InfluxDB Docker containers to visualize and monitor the proofing process.

Refer to the documentation and code within the respective folders for detailed instructions on setup and configuration.

---

*Note: This README provides an overview of the Proofuino project. For detailed setup instructions, code explanations, and additional information, refer to the project documentation and accompanying files.*
