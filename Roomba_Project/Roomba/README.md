# roomba_mqtt

Raspberry Pi <-> iRobot Roomba 600 / Create 2 (Open Interface) <-> MQTT.

## Bouwen
    sudo apt install build-essential cmake pkg-config libmosquitto-dev mosquitto mosquitto-clients
    cmake -B build && cmake --build build -j
    sudo usermod -aG dialout $USER   # opnieuw inloggen daarna

## Draaien
    ./build/roomba_mqtt --device /dev/ttyUSB0 --host localhost --mode safe

## Meeluisteren
    mosquitto_sub -t 'roomba/#' -v

## Besturen
    mosquitto_pub -t roomba/cmd/mode -m safe
    mosquitto_pub -t roomba/cmd/drive -m "150 32767"      # rechtdoor, 150 mm/s (herhaal < 1 s!)
    mosquitto_pub -t roomba/cmd/drive_direct -m "100 -100"
    mosquitto_pub -t roomba/cmd/stop -m ""
    mosquitto_pub -t roomba/cmd/action -m dock

## Aansluiting
- USB-kabel (Create 2) -> /dev/ttyUSB0
- Of Mini-DIN direct op de Pi UART (/dev/serial0): Roomba TX is 5 V, de Pi is 3,3 V -> levelshifter / spanningsdeler!