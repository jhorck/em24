Works with Victron Energy

Thanks to 
https://github.com/stephane/libmodbus


apt install libmodbus5 libmodbus-dev


cp em24.service /etc/systemd/system/
systemctl enable em24.service
systemctl start em24.service

