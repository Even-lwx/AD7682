# PCF8563 RTC

U9 is NXP PCF8563TS/5,118 on the shared I2C1 bus: PB11=SCL and PB12=SDA.
The external 32.768 kHz crystal is connected to OSCI/OSCO. The fixed 7-bit
address is 0x51. The driver provides BCD date/time read/write, clock start or
stop, and the voltage-low flag.
