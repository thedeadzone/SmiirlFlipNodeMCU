**How-to**
1. Once the NodeMCU has started, you can connect to the AP: `Smiirl Connect`.
2. Navigate to `192.168.4.1` to fill in your network and api url details.
3. Once saved the ESP will restart and connect to the network and start retrieving the information.

**Notes**
- For debugging, it just generates a random number currently, you can comment this code and enable to the GET for the `apiUrl`.
- Make sure to set the `deviceAddresses` using a I2C scanner (see `scanner.ino`. 

**Wiring**
There is two types of Smiirl i've run into so far. The old version can be recognised by the huge chip on the main board. Wiring is different for both as on the new version the wiring coloring is wrong. Please do check if this is also the case for you. I've had some issues with 3v vs 5v based upon the new/old board, results may vary.

New
- Blue = 3V
- Brown = GND
- Black = D1 (SCL)
- Red = D2 (SDA)

Old
- Red = 5V
- Black = GND
- White = D1
- Blue = D2
