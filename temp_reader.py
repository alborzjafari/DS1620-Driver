#!/usr/bin/env python3

import time                                                                     

PATH = "/sys/sensors/ds1620/temperature"
delay = 3       # Second(s)

temperature = 0.0

try:
    while True:                                                              
        with open(PATH, 'r') as temperature_file:
            temperature = float(temperature_file.read())
            print("Temperature: {}".format(temperature), flush=True, end='\r')
        time.sleep(delay)
except KeyboardInterrupt:
    print("Temperature: {}".format(temperature), flush=True)
    pass
