# BLExUart
BLE <--> Uart converter for ESP32-S3 or ESP32-C3 Super mini

SmartPhone <--BLE--> ESP32-S3  <--Uart1(U1TxD,U1RxD)--> 
                               <--COM(USB)-->

SmartPhone <--BLE--> ESP32-C3  <--Uart1(U0TxD,U0RxD)--> 
                               <--COM(USB H/W CDC)-->

[UART Baudrate]
Default is 9600bps.

only for S3
You can change baudrate by pressing the boot button.
The color of the LED indicates baudrate.

Baudrate   LED
9600bps    RED
19200      GREEN
38400      YELLOW
57600      BLUE
115200     PURPLE
230400     LIGHT_BLUE
9600       WHITE

[tool]
Arduino IDE 2.3.6
