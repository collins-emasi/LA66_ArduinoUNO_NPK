#include <SoftwareSerial.h>

#define RE 4
#define DE 5

#define BUFF_SIZE 128

String inputString = "";      // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete

long old_time = millis();
long new_time;

long uplink_interval = 30000;
long new_interval = uplink_interval;

bool time_to_at_recvb = false;
bool get_LA66_data_status = false;

bool network_joined_status = false;
 
const byte nitro[] = { 0x01, 0x03, 0x00, 0x1e, 0x00, 0x01, 0xe4, 0x0c };
const byte phos[] = { 0x01, 0x03, 0x00, 0x1f, 0x00, 0x01, 0xb5, 0xcc };
const byte pota[] = { 0x01, 0x03, 0x00, 0x20, 0x00, 0x01, 0x85, 0xc0 };

SoftwareSerial dragino_shield(10, 11);  // Arduino RX, TX ,
SoftwareSerial npk(8, 9);               // Arduino RX, TX
const int bat_pin = A0;                 // Analog Battery Pin

char rxbuff[BUFF_SIZE];
uint8_t rxbuff_index = 0;
byte values[11]; // Holds the output of the sensor (index 4 has sensor reading)

byte val1, val2, val3, bat_level;

byte read_nutrient(byte * nutrient, size_t nutrient_size);
byte read_battery_level();
void read_npk_values();

void setup() {
    Serial.begin(9600);
    npk.begin(9600);
    dragino_shield.begin(9600);
    
    pinMode(RE, OUTPUT);
    pinMode(DE, OUTPUT);

    digitalWrite(RE, HIGH);
    digitalWrite(DE, HIGH);

    // reserve 200 bytes for the inputString:
    inputString.reserve(200);

    dragino_shield.println("ATZ");  //reset LA66
    delay(1000);
}
 
void loop() {  
    // read_npk_values();
    new_time = millis();
    
    if ((new_time - old_time >= uplink_interval) && (network_joined_status == 1)) {
        old_time = new_time;
        get_LA66_data_status = false;

        read_npk_values();

        Serial.println("---------- Transmitting --------------");
        Serial.print("\tNitrogen: "); Serial.print(val1); Serial.println(" mg/kg");
        Serial.print("\tPhosphorous: "); Serial.print(val2); Serial.println(" mg/kg");
        Serial.print("\tPotassium: "); Serial.print(val3); Serial.println(" mg/kg");
        Serial.print("\tBattery Level: "); Serial.print(bat_level); Serial.println("%");

        dragino_shield.listen();
        delay(650);     // Minimum time to start listening

        char sensor_data_buff[BUFF_SIZE] = "\0";
        snprintf(sensor_data_buff, BUFF_SIZE, "AT+SENDB=%d,%d,%d,%02X%02X%02X%02X", 0, 2, 4, val1, val2, val3, bat_level);
        dragino_shield.println(sensor_data_buff);

        uplink_interval = new_interval;
    }

    if (time_to_at_recvb == true) {
        time_to_at_recvb = false;
        get_LA66_data_status = true;
        delay(1000);

        dragino_shield.println("AT+CFG");
    }

    while ( Serial.available()) {
        // get the new byte:
        char inChar = (char) Serial.read();
        // add it to the inputString:
        inputString += inChar;
        // if the incoming character is a newline, set a flag so the main loop can
        // do something about it:
        if (inChar == '\n' || inChar == '\r') {
            dragino_shield.print(inputString);
            inputString = "\0";
        }
    }

    while (dragino_shield.available()) {
        // get the new byte:
        char inChar = (char)dragino_shield.read();
        // add it to the inputString:
        inputString += inChar;

        rxbuff[rxbuff_index++] = inChar;

        if (rxbuff_index > BUFF_SIZE)
        break;

        // if the incoming character is a newline, set a flag so the main loop can
        // do something about it:
        if (inChar == '\n' || inChar == '\r') {
            stringComplete = true;
            rxbuff[rxbuff_index] = '\0';

            if (strncmp(rxbuff, "JOINED", 6) == 0) {
                network_joined_status = 1;
            }

            if (strncmp(rxbuff, "Dragino LA66 Device", 19) == 0) {
                network_joined_status = 0;
            }

            if (strncmp(rxbuff, "Run AT+RECVB=? to see detail", 28) == 0) {
                time_to_at_recvb = true;
                stringComplete = false;
                inputString = "\0";
            }

            if (strncmp(rxbuff, "AT+RECVB=", 9) == 0) {
                stringComplete = false;
                inputString = "\0";
                String downlink = &rxbuff[9];
                Serial.print("\r\nGet downlink data(FPort & Payload) "); Serial.println(&rxbuff[9]);
                int conf_msg = downlink_action(downlink);
                if (conf_msg == 0) { Serial.print("Changed 'uplink_interval' to: "); Serial.println(new_interval); }
                if (conf_msg == 1) { Serial.println("Device has been reset"); }
            }

            rxbuff_index = 0;

            if (get_LA66_data_status == true) {
                stringComplete = false;
                inputString = "\0";
            }
        }
    }

    // print the string when a newline arrives:
    if (stringComplete) {
        Serial.print(inputString);
        
        // clear the string:
        inputString = "\0";
        stringComplete = false;
    }
}

int downlink_action(String port_payload) {
    // Example:     2:0100012c
    // Example:  port:01234567
    // String port = port_payload.substring(0,1);
    String payload = port_payload.substring(2);
    String type = payload.substring(0, 2);

    if (type == "00") {             // 00 means you change the uplink interval with the last 2 characters
        // Change uplink interval
        long factor = strtol(payload.substring(6).c_str(), nullptr, 16);
        new_interval = factor * 60000;
        return 0;
    }
    if (type == "01") {      // 01 means you Reset the device
        // Reset the device
        dragino_shield.println("ATZ");  //reset LA66
        delay(1000);
        return 1;
    }
}

void read_npk_values() {
    npk.listen();
    
    delay(250);
    val1 = read_nutrient(pota, sizeof(nitro)); // Error
    delay(250);
    val1 = read_nutrient(nitro, sizeof(nitro));
    delay(250);
    val2 = read_nutrient(phos, sizeof(phos));
    delay(250);
    val3 = read_nutrient(pota, sizeof(pota));
    delay(250);
    bat_level = read_battery_level();
}

byte read_nutrient(byte * nutrient, size_t nutrient_size) {
    digitalWrite(DE, HIGH);
    digitalWrite(RE, HIGH);
    delay(10);

    if (npk.write(nutrient, nutrient_size) == 8) {
        digitalWrite(DE, LOW);
        digitalWrite(RE, LOW);
        for (byte i = 0; i < 7; i++) {
            values[i] = npk.read();
        }
    }
    return values[4];
}


byte read_battery_level() {
    int raw_value = analogRead(bat_pin);                        // Read raw analog input
    float voltage = raw_value * (5.0/1023.0);                   // Convert to actual voltage
    float bat_level = (voltage - 3.3) * 100.0 / (4.2 - 3.3);    // Convert to percentage
    if (voltage <= 3.3) { bat_level = 0.0; }                    // If the voltage level drops below 3.3 make it to zero
    return static_cast<byte>(bat_level);
}
