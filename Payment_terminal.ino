/**************************************************************************/
/*! 
    @file     readMifareClassicIrq.pde
    @author   Adafruit Industries
	@license  BSD (see license.txt)

    This example will wait for any ISO14443A card or tag, and
    depending on the size of the UID will attempt to read from it.
   
    If the card has a 4-byte UID it is probably a Mifare
    Classic card, and the following steps are taken:
   
    Reads the 4 byte (32 bit) ID of a MiFare Classic card.
    Since the classic cards have only 32 bit identifiers you can stick
	them in a single variable and use that to compare card ID's as a
	number. This doesn't work for ultralight cards that have longer 7
	byte IDs!
    
    Note that you need the baud rate to be 115200 because we need to
	print out the data and read from the card at the same time!

This is an example sketch for the Adafruit PN532 NFC/RFID breakout boards
This library works with the Adafruit NFC breakout 
  ----> https://www.adafruit.com/products/364
 
Check out the links above for our tutorials and wiring diagrams 

This example is for communicating with the PN532 chip using I2C. Wiring 
should be as follows:
  PN532 SDA -> SDA pin
  PN532 SCL -> SCL pin
  PN532 IRQ -> D2
  PN532 SDA -> 3.3v (with 2k resistor)
  PN532 SCL -> 3.3v (with 2k resistor)
  PN532 3.3v -> 3.3v
  PN532 GND -> GND

Adafruit invests time and resources providing this open source code, 
please support Adafruit and open-source hardware by purchasing 
products from Adafruit!
*/
/**************************************************************************/
#include <stdio.h> 
#include <stdlib.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <TroykaTextLCD.h>
#include <TroykaLight.h>

// If using the breakout with SPI, define the pins for SPI communication.
#define PN532_SCK  (2)
#define PN532_MOSI (3)
#define PN532_SS   (4)
#define PN532_MISO (5)

// If using the breakout or shield with I2C, define just the pins connected
// to the IRQ and reset lines.  Use the values below (2, 3) for the shield!
#define PN532_IRQ   (9)
#define PN532_RESET (100)  // Not connected by default on the NFC Shield

const int DELAY_BETWEEN_CARDS = 5000;
long timeLastCardRead = 0;
boolean readerDisabled = false;
int irqCurr;
int irqPrev;

// This example uses the IRQ line, which is available when in I2C mode.
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

// создаём объект для работы с датчиком освещённости
// и передаём ему номер пина выходного сигнала
TroykaLight sensorLight(A0);

// создаем объект для работы с дисплеем
TroykaTextLCD lcd;

void setup(void) {
  Serial.begin(115200);
  // while (!Serial) delay(10); // for Leonardo/Micro/Zero

  Serial.println("Hello!");

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  
  // configure board to read RFID tags
  nfc.SAMConfig();
  // устанавливаем количество столбцов и строк экрана
  // устанавливаем контрастность в диапазоне от 0 до 63
  lcd.setContrast(27);
  startListeningToNFC();
}

void loop(void) {
  // считывание данных с датчика освещённости
  sensorLight.read();
  // вывод показателей сенсора освещённости в люксахи
  // Serial.print("Light is ");
  // Serial.print(sensorLight.getLightLux());
  // Serial.print(" Lx\t");
  // вывод показателей сенсора освещённости в фут-свечах
  //Serial.print(sensorLight.getLightFootCandles());
  //Serial.println(" Foot Candles");
  if (sensorLight.getLightLux() > 100) {
    lcd.setBrightness(255);
  }
  if (sensorLight.getLightLux() <= 100) {
    lcd.setBrightness(sensorLight.getLightLux() * 2.55);
  }
  if (readerDisabled) {
    if (millis() - timeLastCardRead > DELAY_BETWEEN_CARDS) {
      readerDisabled = false;
      startListeningToNFC();
    }
  } else {
    irqCurr = digitalRead(PN532_IRQ);

    // When the IRQ is pulled low - the reader has got something for us.
    if (irqCurr == LOW && irqPrev == HIGH) {
       Serial.println("Got NFC IRQ");  
       handleCardDetected(); 
    }
  
    irqPrev = irqCurr;
  }
}

void startListeningToNFC() {
  // Reset our IRQ indicators
  irqPrev = irqCurr = HIGH;
  
  Serial.println("Waiting for an ISO14443A Card ...");
  lcd.clear();
  lcd.begin(16, 2, 2);
  lcd.setCursor(1, 0);
  lcd.print("Scan your card");
  // lcd.setCursor(0, 1);
  // lcd.print("to the scanner");
  nfc.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A);
}

void handleCardDetected() {
    uint8_t success = false;
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
    uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    uint8_t currentblock; 

    // read the NFC tag's info
    success = nfc.readDetectedPassiveTargetID(uid, &uidLength);
    Serial.println(success ? "Read successful" : "Read failed (not a card?)");

    if (success) {
      // Display some basic information about the card
      Serial.println("Found an ISO14443A card");
      Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
      Serial.print("  UID Value: ");
      nfc.PrintHex(uid, uidLength);
      
      if (uidLength == 4)
      {
        Serial.print("Seems to be a Mifare Classic card #");
        uint32_t cardid = uid[0];
        cardid <<= 8;
        cardid |= uid[1];
        cardid <<= 8;
        cardid |= uid[2];  
        cardid <<= 8;
        cardid |= uid[3]; 
        Serial.println(cardid);

        Serial.println("Trying to authenticate block 4 with default KEYA value");
        uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

        // Start with block 4 (the first block of sector 1) since sector 0
	      // contains the manufacturer data and it's probably better just
	      // to leave it alone unless you know what you're doing
        success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);
        if (success)
        {
          Serial.println("Sector 1 (Blocks 4..7) has been authenticated");
          uint8_t data[16];
          lcd.clear();
          lcd.begin(16, 2);
          // If you want to write something to block 4 to test with, uncomment
          // the following line and this text should be read back in a minute
          //memcpy(data, (const uint8_t[]){ 'a', 'd', 'a', 'f', 'r', 'u', 'i', 't', '.', 'c', 'o', 'm', 0, 0, 0, 0 }, sizeof data);
          // success = nfc.mifareclassic_WriteDataBlock (4, data);
          for (currentblock = 4; currentblock < 8; currentblock++)
          {
            if (currentblock == 5)
            {
              // int x = 9999;
              // int i;
              // int n = log10(x) + 1;
              // char *numberArray = calloc(n, sizeof data);
              // for (i = n-1; i >= 0; --i, x /= 10)
              // {
              //     numberArray[i] = (x % 10) + '0';
              // }
              // // memcpy(data, (const uint8_t[]){ '1', '2', '5', '3', '5', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, sizeof data);
              // memcpy(data, numberArray, sizeof data);
              // nfc.mifareclassic_WriteDataBlock (currentblock, data);  
                          
            }
            // Try to read the contents of block 4
            success = nfc.mifareclassic_ReadDataBlock(currentblock, data);
        
            if (success)
            {
              // Data seems to have been read ... spit it out
              Serial.print("Reading Block ");Serial.print(currentblock);Serial.println(":");
              nfc.PrintHexChar(data, 16);
              if (currentblock == 4)
              {
                uint32_t szPos;
                char str[16];
                for (szPos = 0; szPos < 16; szPos++) {
                  if (data[szPos] <= 0x1F)
                    str[szPos] = 0;
                  else
                    // PN532DEBUGPRINT.print((char)data[szPos]);
                    str[szPos] = (char)data[szPos];
                }
                // Serial.println("");
                // Serial.println(str);
                lcd.setCursor(0, 0);
                lcd.print(str);
              }
              if (currentblock == 5)
              {
                uint32_t szPos;
                char str[16];
                for (szPos = 0; szPos < 16; szPos++) {
                  if (data[szPos] <= 0x1F)
                    str[szPos] = 0;
                  else
                    // PN532DEBUGPRINT.print((char)data[szPos]);
                    str[szPos] = (char)data[szPos];
                }
                // Serial.println("");
                // Serial.println(str);
                lcd.setCursor(0, 1);
                lcd.print(str);
              }
              Serial.println("");
              // Wait a bit before reading the card again
              // delay(1000);
            }
            else
            {
              Serial.print("Ooops ... unable to read block ");Serial.print(currentblock);Serial.println(".  Try another key?");
            }
          }
        }
        else
        {
          Serial.println("Ooops ... authentication failed: Try another key?");
        }
      }
      Serial.println("");

      timeLastCardRead = millis();
    }

    // The reader will be enabled again after DELAY_BETWEEN_CARDS ms will pass.
    readerDisabled = true;
}