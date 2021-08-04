import serial
import time

ser = serial.Serial ("/dev/ttyS0", 9600)    #Open port with default GPS Serial baud rate

current_seconds = int(time.time())

while True:
    if current_seconds != int(time.time()):
        gprmc_msg = time.strftime("$GPRMC,%H%M%S,A,4722.6790,N,0832.7730,E,00.0,00.0,%d%m%y,0.0,E,A*", time.localtime()) # create the SPRMC string
        ser.write(gprmc_msg.encode())                # transmit data serially
        current_seconds = int(time.time())      # update current time
