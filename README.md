Works with Victron Energy ESS<br>

Subscribe to a local MQTT broker and make energy data available on a Modbus server

sml_smartmeter  -->  MQTT broker  -->  em24  <--  Victron


Thanks to 
https://github.com/stephane/libmodbus


apt install libmodbus5 libmodbus-dev


cp em24.service /etc/systemd/system/<br>
systemctl enable em24.service<br>
systemctl start em24.service

