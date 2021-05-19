#include <math.h>
#include <Servo.h>
#include <GyverEncoder.h>
#include <ShiftRegister74HC595.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <OneWire.h>
#include <Time.h>
#include <avr/eeprom.h>

// LCD Display
LiquidCrystal_I2C display(0x27, 16, 2);

// ShiftRegister Pins
#define LATCH_PIN 5
#define CLOCK_PIN 4
#define DATA_PIN 3
ShiftRegister74HC595<1> sr(DATA_PIN, CLOCK_PIN, LATCH_PIN);

// Clock
RTC_DS3231 rtc;
long tempClockUpdateTime = 0;
long dataSavedTimeoutTemp = 0;
const int DATA_SAVED_TIMEOUT = 1000;
const int CLOCK_UPDATE_TIME = 100;
int hours, minutes, seconds, day, month, year, prevDay, prevYear, prevMonth;
long timeOnSeconds, timeOffSeconds;

// Relay Pins
// #define LIGHTS_RELAY 15

// Encoder Pins
#define CLK 6
#define DT 7
#define SW 8
Encoder enc(CLK, DT, SW);

int menuList = 0;
int editMode = 0;

// Servos
Servo cage1;
long servoLastUpdateTime;

// Temperature Sensor
#define TEMP_SENSOR 9
double temperature = 0;
OneWire tempSensor(TEMP_SENSOR);

long tempLastUpdateTime = 0;
const int TEMP_UPDATE_TIME = 500;

int temperatureTreshold = 20;

long dangerStatusLastUpdateTime = 0;
const int DANGER_STATUS_INTERVAL = 200;
int dangerLEDState = LOW;

byte celsius[8] = {
    0b01100,
    0b10010,
    0b10010,
    0b01100,
    0b00000,
    0b00000,
    0b00000,
    0b00000};
byte p20[8] = {
    B10000,
    B10000,
    B10000,
    B10000,
    B10000,
    B10000,
    B10000,
    B10000,
};

byte p40[8] = {
    B11000,
    B11000,
    B11000,
    B11000,
    B11000,
    B11000,
    B11000,
    B11000,
};

byte p60[8] = {
    B11100,
    B11100,
    B11100,
    B11100,
    B11100,
    B11100,
    B11100,
    B11100,
};

byte p80[8] = {
    B11110,
    B11110,
    B11110,
    B11110,
    B11110,
    B11110,
    B11110,
    B11110,
};

byte p100[8] = {
    B11111,
    B11111,
    B11111,
    B11111,
    B11111,
    B11111,
    B11111,
    B11111,
};

int editPage2List = 0;
int editCurrentHour = 0;
int editCurrentMinute = 0;
int editCurrentYear = 2021;
int editCurrentMonth = 1;
int editCurrentDay = 1;

int feedingTime = 1;
int editPage4List = 0;
int editPageFlag = 0;

int feedingHour1 = 0;
int feedingMinute1 = 0;
int feedingHour2 = 0;
int feedingMinute2 = 0;
int feedingHour3 = 0;
int feedingMinute3 = 0;

int editPage5List = 0;
int lightsOnHour = 0;
int lightsOnMinutes = 0;
int lightsOffHour = 0;
int lightsOffMinutes = 0;
int lightsState = LOW;

int feedingStatus = 0;

long convertTimeToSeconds(int hours, int minutes, int seconds = 0)
{
    long timeToSeconds = (long)hours * 3600 + minutes * 60 + seconds;
    return timeToSeconds;
}

void setup()
{
    Serial.begin(9600);

    float TRESHOLD_EEPROM;
    TRESHOLD_EEPROM = eeprom_read_float(0);

    int FEEDING_TIME, FEEDING_HOUR_1, FEEDING_HOUR_2, FEEDING_HOUR_3, FEEDING_MINUTE_1, FEEDING_MINUTE_2, FEEDING_MINUTE_3;

    FEEDING_TIME = eeprom_read_byte(4);
    FEEDING_HOUR_1 = eeprom_read_byte(5);
    FEEDING_MINUTE_1 = eeprom_read_byte(6);
    FEEDING_HOUR_2 = eeprom_read_byte(7);
    FEEDING_MINUTE_2 = eeprom_read_byte(8);
    FEEDING_HOUR_3 = eeprom_read_byte(9);
    FEEDING_MINUTE_3 = eeprom_read_byte(10);

    int LIGHTS_ON_HOUR, LIGHTS_ON_MINUTES, LIGHTS_OFF_HOUR, LIGHTS_OFF_MINUTES;

    LIGHTS_ON_HOUR = eeprom_read_byte(11);
    LIGHTS_ON_MINUTES = eeprom_read_byte(12);
    LIGHTS_OFF_HOUR = eeprom_read_byte(13);
    LIGHTS_OFF_MINUTES = eeprom_read_byte(14);

    // ShiftRegister Initialization
    sr.setAllHigh();

    // Display Initialization
    display.init();
    display.backlight();

    display.createChar(2, p20);
    display.createChar(3, p40);
    display.createChar(4, p60);
    display.createChar(5, p80);
    display.createChar(6, p100);
    display.createChar(1, celsius);

    display.setCursor(0, 0);
    display.print("Loading...  v0.7");

    display.setCursor(0, 1);
    display.print("                ");
    for (int i = 0; i < 16; i++)
    {
        for (int j = 2; j <= 6; j++)
        {
            display.setCursor(i, 1);
            display.write(j);
            delay(20);
        };
    };
    display.setCursor(0, 0);
    display.print("Ready to go...  ");

    temperatureTreshold = TRESHOLD_EEPROM;

    feedingTime = FEEDING_TIME;
    feedingHour1 = FEEDING_HOUR_1;
    feedingMinute1 = FEEDING_MINUTE_1;
    feedingHour2 = FEEDING_HOUR_2;
    feedingMinute2 = FEEDING_MINUTE_2;
    feedingHour3 = FEEDING_HOUR_3;
    feedingMinute3 = FEEDING_MINUTE_3;

    lightsOnHour = LIGHTS_ON_HOUR;
    lightsOnMinutes = LIGHTS_ON_MINUTES;
    lightsOffHour = LIGHTS_OFF_HOUR;
    lightsOffMinutes = LIGHTS_OFF_MINUTES;

    timeOnSeconds = convertTimeToSeconds(lightsOnHour, lightsOnMinutes);
    timeOffSeconds = convertTimeToSeconds(lightsOffHour, lightsOffMinutes);

    // Relay Initialization
    // pinMode(LIGHTS_RELAY, OUTPUT);

    // delay(2000);

    // Servo Initialization

    // Encoder Setup
    enc.setType(TYPE1);
    enc.setTickMode(AUTO);

    // RTC Initialization
    rtc.begin();
    if (!rtc.begin())
    {
        Serial.println("Couldn't find RTC!!!");
        while (1)
            ;
    }
    else
    {
        sr.setAllLow();
    };
    if (rtc.lostPower())
    {
        Serial.println("RTC lost power, lets set the time!");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    };

    sr.setAllLow();
};

void loop()
{
    encoderAction();
    getTime();
    detectTemperature();
    dateUpdate();
    lightsManager();

    switch (menuList)
    {
    case 0: // MAIN DISPLAY
        renderFrame0();
        break;
    case 1: // TEMPERATURE CONTROL
        renderFrame2();
        break;
    case 2: // FEEDING CONTROL
        renderFrame3();
        break;
    case 3: // CLOCK SETTINGS
        renderFrame4();
        break;
    case 4: // LIGHT SETTINGS
        renderFrame5();
        break;
    case 5: // MANUAL FEEDING
        renderFrame6();
        break;
    case 6:
        renderSavedFrame();
        break;
    default:
        renderFrame0();
        break;
    };

    // EDIT MODE INDICATOR
    if (editMode == 1)
    {
        sr.set(0, 1);
    }
    else
    {
        sr.set(0, 0);
    };

    // TEMPERATURE INDICATOR
    if (temperature > temperatureTreshold)
    {
        sr.set(2, 0);
        dangerStatus(1);
    }
    else
    {
        sr.set(1, 0);
        sr.set(2, 1);
    }

    // FEEDING INDICATOR
    sr.set(3, feedingStatus);
};

void servoInitPosition()
{
    cage1.write(0);
    delay(500);
    cage1.write(270);
    delay(500);
};

void servoOpen()
{
    cage1.write(180);
};

void servoClose()
{
    cage1.write(0);
};

void dateUpdate()
{
    if (prevDay != day)
    {
        prevDay = day;
        editCurrentDay = day;
    };
    if (prevMonth != month)
    {
        prevMonth = month;
        editCurrentMonth = month;
    };
    if (prevYear != year)
    {
        prevYear = year;
        editCurrentYear = year;
    };
}

void encoderAction()
{
    enc.tick();
    if (editMode == 1)
    {
        // CLOCK SETTINGS
        if (menuList == 1)
        {
            if (editPage2List == 0)
            {
                if (editPageFlag == 0)
                {
                    if (enc.isRight())
                    {
                        if (editCurrentHour >= 23)
                        {
                            editCurrentHour = 0;
                        }
                        else
                        {
                            editCurrentHour++;
                        };
                    };
                    if (enc.isLeft())
                    {
                        if (editCurrentHour <= 0)
                        {
                            editCurrentHour = 23;
                        }
                        else
                        {
                            editCurrentHour--;
                        };
                    };
                }
                if (editPageFlag == 1)
                {
                    if (enc.isRight())
                    {
                        if (editCurrentMinute >= 59)
                        {
                            editCurrentMinute = 0;
                        }
                        else
                        {
                            editCurrentMinute++;
                        };
                    };
                    if (enc.isLeft())
                    {
                        if (editCurrentMinute <= 0)
                        {
                            editCurrentMinute = 59;
                        }
                        else
                        {
                            editCurrentMinute--;
                        };
                    };
                };
                if (editPageFlag == 2)
                {
                    if (enc.isHolded())
                    {
                        editMode = 0;
                        editPageFlag = 0;
                        menuList = 1;
                    };
                };
                if (editPageFlag == 3)
                {
                    if (enc.isHolded())
                    {
                        editPageFlag = 0;
                        editPage2List = 1;
                    };
                };

                if (enc.isClick())
                {
                    if (editPageFlag < 3)
                    {
                        editPageFlag++;
                    }
                    else
                    {
                        editPageFlag = 0;
                    };
                };
            };
            if (editPage2List == 1)
            {
                if (editPageFlag == 0)
                {
                    if (enc.isRight())
                    {
                        if (editCurrentDay >= 31)
                        {
                            editCurrentDay = 1;
                        }
                        else
                        {
                            editCurrentDay++;
                        };
                    }
                    if (enc.isLeft())
                    {
                        if (editCurrentDay <= 1)
                        {
                            editCurrentDay = 31;
                        }
                        else
                        {
                            editCurrentDay--;
                        };
                    };
                };
                if (editPageFlag == 1)
                {
                    if (enc.isRight())
                    {
                        if (editCurrentMonth >= 12)
                        {
                            editCurrentMonth = 1;
                        }
                        else
                        {
                            editCurrentMonth++;
                        };
                    };
                    if (enc.isLeft())
                    {
                        if (editCurrentMonth <= 1)
                        {
                            editCurrentMonth = 12;
                        }
                        else
                        {
                            editCurrentMonth--;
                        };
                    };
                };
                if (editPageFlag == 2)
                {
                    if (enc.isRight())
                    {
                        editCurrentYear++;
                    };
                    if (enc.isLeft())
                    {
                        editCurrentYear--;
                    }
                };
                if (editPageFlag == 3)
                {
                    if (enc.isHolded())
                    {
                        editPageFlag = 0;
                        editPage2List = 0;
                    };
                };
                if (editPageFlag == 4)
                {
                    if (enc.isHolded())
                    {

                        editPageFlag = 0;
                        editPage2List = 2;
                    };
                };

                if (enc.isClick())
                {
                    if (editPageFlag < 4)
                    {
                        editPageFlag++;
                    }
                    else
                    {
                        editPageFlag = 0;
                    };
                };
            };

            if (editPage2List == 2)
            {
                if (editPageFlag == 0)
                {
                    if (enc.isHolded())
                    {
                        editPageFlag = 0;
                        editMode = 0;
                        menuList = 0;
                    };
                };
                if (editPageFlag == 1)
                {
                    if (enc.isHolded())
                    {
                        rtc.adjust(DateTime(editCurrentYear, editCurrentMonth, editCurrentDay, editCurrentHour, editCurrentMinute, 0));
                        editPageFlag = 0;
                        menuList = 6;
                    }
                };
                if (enc.isClick())
                {
                    if (editPageFlag < 1)
                    {
                        editPageFlag++;
                    }
                    else
                    {
                        editPageFlag = 0;
                    };
                };
            };
        };

        // TEMPERATURE CONTROL
        if (menuList == 2)
        {
            if (enc.isHolded())
            {
                eeprom_write_float(0, temperatureTreshold);
                enc.resetStates();
                menuList = 6;
            };
            if (enc.isRight())
            {
                temperatureTreshold++;
            };
            if (enc.isLeft())
            {
                temperatureTreshold--;
            };
        };

        // FEEDING CONTROL
        if (menuList == 3)
        {
            if (editPage4List == 0)
            {
                if (editPageFlag == 0)
                {
                    if (enc.isRight())
                    {
                        if (feedingTime >= 9)
                        {
                            feedingTime = 9;
                        }
                        else
                        {
                            feedingTime++;
                        };
                    };
                    if (enc.isLeft())
                    {
                        if (feedingTime <= 1)
                        {
                            feedingTime = 1;
                        }
                        else
                        {
                            feedingTime--;
                        };
                    };
                }
                if (editPageFlag == 1)
                {
                    if (enc.isHolded())
                    {
                        editMode = 0;
                        editPageFlag = 0;
                        menuList = 3;
                        enc.resetStates();
                    };
                }
                if (editPageFlag == 2)
                {
                    if (enc.isHolded())
                    {
                        eeprom_write_byte(4, feedingTime);
                        editPage4List = 1;
                        editPageFlag = 0;
                        enc.resetStates();
                    };
                };

                if (enc.isClick())
                {
                    if (editPageFlag < 2)
                    {
                        editPageFlag++;
                    }
                    else
                    {
                        editPageFlag = 0;
                    };
                };
            };
            if (editPage4List == 1)
            {
                if (editPageFlag == 0)
                {
                    if (enc.isRight())
                    {
                        if (feedingHour1 >= 23)
                        {
                            feedingHour1 = 0;
                        }
                        else
                        {
                            feedingHour1++;
                        };
                    };
                    if (enc.isLeft())
                    {
                        if (feedingHour1 <= 0)
                        {
                            feedingHour1 = 23;
                        }
                        else
                        {
                            feedingHour1--;
                        };
                    };
                };

                if (editPageFlag == 1)
                {
                    if (enc.isRight())
                    {
                        if (feedingMinute1 >= 55)
                        {
                            feedingMinute1 = 0;
                        }
                        else
                        {
                            feedingMinute1 += 5;
                        };
                    };
                    if (enc.isLeft())
                    {
                        if (feedingMinute1 <= 0)
                        {
                            feedingMinute1 = 55;
                        }
                        else
                        {
                            feedingMinute1 -= 5;
                        };
                    };
                };

                if (editPageFlag == 2)
                {
                    if (enc.isHolded())
                    {
                        editPage4List = 0;
                        editPageFlag = 0;
                        enc.resetStates();
                    };
                }
                if (editPageFlag == 3)
                {
                    if (enc.isHolded())
                    {
                        eeprom_write_byte(5, feedingHour1);
                        eeprom_write_byte(6, feedingMinute1);
                        editPage4List = 2;
                        editPageFlag = 0;
                        enc.resetStates();
                    };
                };

                if (enc.isClick())
                {
                    if (editPageFlag < 3)
                    {
                        editPageFlag++;
                    }
                    else
                    {
                        editPageFlag = 0;
                    };
                };
            };

            if (editPage4List == 2)
            {
                if (editPageFlag == 0)
                {
                    if (enc.isRight())
                    {
                        if (feedingHour2 >= 23)
                        {
                            feedingHour2 = 0;
                        }
                        else
                        {
                            feedingHour2++;
                        };
                    };
                    if (enc.isLeft())
                    {
                        if (feedingHour2 <= 0)
                        {
                            feedingHour2 = 23;
                        }
                        else
                        {
                            feedingHour2--;
                        };
                    };
                };

                if (editPageFlag == 1)
                {
                    if (enc.isRight())
                    {
                        if (feedingMinute2 >= 55)
                        {
                            feedingMinute2 = 0;
                        }
                        else
                        {
                            feedingMinute2 += 5;
                        };
                    };
                    if (enc.isLeft())
                    {
                        if (feedingMinute2 <= 0)
                        {
                            feedingMinute2 = 55;
                        }
                        else
                        {
                            feedingMinute2 -= 5;
                        };
                    };
                };

                if (editPageFlag == 2)
                {
                    if (enc.isHolded())
                    {
                        editPage4List = 1;
                        editPageFlag = 0;
                        enc.resetStates();
                    };
                }
                if (editPageFlag == 3)
                {
                    if (enc.isHolded())
                    {
                        eeprom_write_byte(7, feedingHour2);
                        eeprom_write_byte(8, feedingMinute2);
                        editPage4List = 3;
                        editPageFlag = 0;
                        enc.resetStates();
                    };
                };

                if (enc.isClick())
                {
                    if (editPageFlag < 3)
                    {
                        editPageFlag++;
                    }
                    else
                    {
                        editPageFlag = 0;
                    };
                };
            };

            if (editPage4List == 3)
            {
                if (editPageFlag == 0)
                {
                    if (enc.isRight())
                    {
                        if (feedingHour3 >= 23)
                        {
                            feedingHour3 = 0;
                        }
                        else
                        {
                            feedingHour3++;
                        };
                    };
                    if (enc.isLeft())
                    {
                        if (feedingHour3 <= 0)
                        {
                            feedingHour3 = 23;
                        }
                        else
                        {
                            feedingHour3--;
                        };
                    };
                };

                if (editPageFlag == 1)
                {
                    if (enc.isRight())
                    {
                        if (feedingMinute3 >= 55)
                        {
                            feedingMinute3 = 0;
                        }
                        else
                        {
                            feedingMinute3 += 5;
                        };
                    };
                    if (enc.isLeft())
                    {
                        if (feedingMinute3 <= 0)
                        {
                            feedingMinute3 = 55;
                        }
                        else
                        {
                            feedingMinute3 -= 5;
                        };
                    };
                };

                if (editPageFlag == 2)
                {
                    if (enc.isHolded())
                    {
                        editPage4List = 2;
                        editPageFlag = 0;
                        enc.resetStates();
                    };
                }
                if (editPageFlag == 3)
                {
                    if (enc.isHolded())
                    {
                        eeprom_write_byte(9, feedingHour3);
                        eeprom_write_byte(10, feedingMinute3);
                        menuList = 6;
                        editPageFlag = 0;
                        enc.resetStates();
                    };
                };

                if (enc.isClick())
                {
                    if (editPageFlag < 3)
                    {
                        editPageFlag++;
                    }
                    else
                    {
                        editPageFlag = 0;
                    };
                };
            }
        };

        // LIGHTS CONTROL
        if (menuList == 4)
        {
            if (editPage5List == 0)
            {
                if (editPageFlag == 0)
                {
                    if (enc.isRight())
                    {
                        if (lightsOnHour >= 23)
                        {
                            lightsOnHour = 0;
                        }
                        else
                        {
                            lightsOnHour++;
                        };
                    };
                    if (enc.isLeft())
                    {
                        if (lightsOnHour <= 0)
                        {
                            lightsOnHour = 23;
                        }
                        else
                        {
                            lightsOnHour--;
                        };
                    };
                }
                if (editPageFlag == 1)
                {
                    if (enc.isRight())
                    {
                        if (lightsOnMinutes >= 55)
                        {
                            lightsOnMinutes = 0;
                        }
                        else
                        {
                            lightsOnMinutes += 5;
                        };
                    };
                    if (enc.isLeft())
                    {
                        if (lightsOnMinutes <= 0)
                        {
                            lightsOnMinutes = 55;
                        }
                        else
                        {
                            lightsOnMinutes -= 5;
                        };
                    };
                };
                if (editPageFlag == 2)
                {
                    if (enc.isHolded())
                    {
                        editMode = 0;
                        editPageFlag = 0;
                        menuList = 4;
                    };
                };
                if (editPageFlag == 3)
                {
                    if (enc.isHolded())
                    {
                        editPageFlag = 0;
                        editPage5List = 1;
                    };
                };

                if (enc.isClick())
                {
                    if (editPageFlag < 3)
                    {
                        editPageFlag++;
                    }
                    else
                    {
                        editPageFlag = 0;
                    };
                };
            };

            if (editPage5List == 1)
            {
                if (editPageFlag == 0)
                {
                    if (enc.isRight())
                    {
                        if (lightsOffHour >= 23)
                        {
                            lightsOffHour = 0;
                        }
                        else
                        {
                            lightsOffHour++;
                        };
                    };
                    if (enc.isLeft())
                    {
                        if (lightsOffHour <= 0)
                        {
                            lightsOffHour = 23;
                        }
                        else
                        {
                            lightsOffHour--;
                        };
                    };
                }
                if (editPageFlag == 1)
                {
                    if (enc.isRight())
                    {
                        if (lightsOffMinutes >= 55)
                        {
                            lightsOffMinutes = 0;
                        }
                        else
                        {
                            lightsOffMinutes += 5;
                        };
                    };
                    if (enc.isLeft())
                    {
                        if (lightsOffMinutes <= 0)
                        {
                            lightsOffMinutes = 55;
                        }
                        else
                        {
                            lightsOffMinutes -= 5;
                        };
                    };
                };
                if (editPageFlag == 2)
                {
                    if (enc.isHolded())
                    {
                        editPageFlag = 0;
                        editPage5List = 0;
                    };
                };
                if (editPageFlag == 3)
                {
                    if (enc.isHolded())
                    {
                        eeprom_write_byte(11, lightsOnHour);
                        eeprom_write_byte(12, lightsOnMinutes);
                        eeprom_write_byte(13, lightsOffHour);
                        eeprom_write_byte(14, lightsOffMinutes);
                        timeOnSeconds = convertTimeToSeconds(lightsOnHour, lightsOnMinutes);
                        timeOffSeconds = convertTimeToSeconds(lightsOffHour, lightsOffMinutes);
                        Serial.println(timeOnSeconds);
                        Serial.println(timeOffSeconds);
                        editPageFlag = 0;
                        editPage5List = 0;
                        menuList = 6;
                    };
                };

                if (enc.isClick())
                {
                    if (editPageFlag < 3)
                    {
                        editPageFlag++;
                    }
                    else
                    {
                        editPageFlag = 0;
                    };
                };
            };
        };

        // MANUAL FEEDING
        if (menuList == 5)
        {
            if (editPageFlag == 0)
            {
                if (enc.isPress())
                {
                    feedingStatus = 1;
                    servoOpen();
                };
                if (enc.isReleaseHold())
                {
                    feedingStatus = 0;
                    enc.resetStates();
                    servoClose();
                };
            }

            if (editPageFlag == 1)
            {
                if (enc.isHolded())
                {
                    menuList = 5;
                    editMode = 0;
                    editPageFlag = 0;
                };
            };
            if (enc.isRight())
            {
                if (editPageFlag >= 1)
                {
                    editPageFlag = 0;
                }
                else
                {
                    editPageFlag++;
                };
            };
            if (enc.isLeft())
            {
                if (editPageFlag <= 0)
                {
                    editPageFlag = 1;
                }
                else
                {
                    editPageFlag--;
                };
            };
        };
    }
    else
    {
        if (enc.isClick() == true)
        {
            if (menuList < 5)
            {
                menuList++;
            }
            else
            {
                menuList = 0;
            };
        };
        if (enc.isHolded() == true && menuList != 0)
        {
            editMode = 1;
            enc.resetStates();
        };
    };
};

void dangerStatus(int led)
{
    if ((millis() - dangerStatusLastUpdateTime) >= DANGER_STATUS_INTERVAL)
    {
        dangerStatusLastUpdateTime = millis();

        if (dangerLEDState == LOW)
        {
            dangerLEDState = HIGH;
        }
        else
        {
            dangerLEDState = LOW;
        };
        sr.set(led, dangerLEDState);
    };
}

void lightsManager()
{
    if (millis() - tempClockUpdateTime > CLOCK_UPDATE_TIME)
    {
        long currentTimeSeconds;
        currentTimeSeconds = convertTimeToSeconds(hours, minutes, seconds);

        long timeOnResult, timeOffResult;

        int zerroCrossing = 0;

        if (currentTimeSeconds == 0)
        {
            zerroCrossing = 1;
        };

        if (timeOnSeconds < timeOffSeconds)
        {
            timeOnResult = currentTimeSeconds - timeOnSeconds;
            timeOffResult = timeOffSeconds - currentTimeSeconds;

            if (timeOnResult >= 0 && timeOffResult >= 0)
            {
                lightsState = HIGH;
            }
            else
            {
                lightsState = LOW;
            };
        }
        else if (timeOnSeconds > timeOffSeconds)
        {
            int mayZeroCross = 1;

            if (zerroCrossing == 1)
            {
                timeOnResult = currentTimeSeconds - 0;
                timeOffResult = timeOffSeconds - currentTimeSeconds;
                if (timeOnResult >= 0 && timeOffResult >= 0)
                {
                    lightsState = HIGH;
                }
                else
                {
                    lightsState = 0;
                    zerroCrossing = 0;
                }
            }
            else
            {
                timeOnResult = currentTimeSeconds - timeOnSeconds;
                timeOffResult = timeOffSeconds - currentTimeSeconds;
                if (timeOnResult <= 0 && timeOffResult >= 0)
                {
                    lightsState = HIGH;
                }
                else
                {
                    lightsState = 0;
                    zerroCrossing = 0;
                }
            };
        }
    };

    sr.set(4, lightsState);
};

void renderFrame0()
{
    if (millis() - tempClockUpdateTime > CLOCK_UPDATE_TIME)
    {
        renderTime();
        display.setCursor(0, 1);
        renderTemperature(1);
    };
};

void renderFrame2()
{
    if (editMode == 0)
    {
        page2();
    }
    else
    {
        editPage2();
    };
};

void page2()
{
    display.setCursor(0, 0);
    display.print("CLOCK SETTINGS  ");
    display.setCursor(0, 1);
    // timeParser(day);
    // display.print("/");
    // timeParser(month);
    // display.print("/");
    // display.print(year);
    display.print("           >EDIT");
}

void editPage2()
{
    if (editPage2List == 0)
    {
        editPage2_1();
    };
    if (editPage2List == 1)
    {
        editPage2_2();
    };
    if (editPage2List == 2)
    {
        editPage2_3();
    };
}

void editPage2_1()
{
    display.setCursor(0, 0);
    if (editPageFlag == 0)
    {
        display.print("Time:");
        display.print("     ");
        display.print(">");
        timeParser(editCurrentHour);
        display.print(":");
        timeParser(editCurrentMinute);
    }
    else if (editPageFlag == 1)
    {
        display.print("Time:");
        display.print("     ");
        timeParser(editCurrentHour);
        display.print(":");
        display.print(">");
        timeParser(editCurrentMinute);
    }
    else
    {
        display.print("Time:");
        display.print("      ");
        timeParser(editCurrentHour);
        display.print(":");
        timeParser(editCurrentMinute);
    }

    display.setCursor(0, 1);
    if (editPageFlag == 2)
    {
        display.print(">Back       Next");
    }
    else if (editPageFlag == 3)
    {
        display.print("Back       >Next");
    }
    else
    {
        display.print("Back        Next");
    };
}

void editPage2_2()
{
    display.setCursor(0, 0);
    if (editPageFlag == 0)
    {
        display.print("Date:");
        display.print(">");
        timeParser(editCurrentDay);
        display.print("/");
        timeParser(editCurrentMonth);
        display.print("/");
        timeParser(editCurrentYear);
    }
    else if (editPageFlag == 1)
    {
        display.print("Date: ");
        timeParser(editCurrentDay);
        display.print(">");
        timeParser(editCurrentMonth);
        display.print("/");
        timeParser(editCurrentYear);
    }
    else if (editPageFlag == 2)
    {
        display.print("Date: ");
        timeParser(editCurrentDay);
        display.print("/");
        timeParser(editCurrentMonth);
        display.print(">");
        timeParser(editCurrentYear);
    }
    else
    {
        display.print("Date:");
        display.print(" ");
        timeParser(editCurrentDay);
        display.print("/");
        timeParser(editCurrentMonth);
        display.print("/");
        timeParser(editCurrentYear);
    }

    display.setCursor(0, 1);
    if (editPageFlag == 3)
    {
        display.print(">Back       SAVE");
    }
    else if (editPageFlag == 4)
    {
        display.print("Back       >SAVE");
    }
    else
    {
        display.print("Back        SAVE");
    };
}

void editPage2_3()
{
    display.setCursor(0, 0);
    display.print("OVERWRITE CLOCK?");
    display.setCursor(0, 1);
    if (editPageFlag == 0)
    {
        display.print(">No          Yes");
    }
    else if (editPageFlag == 1)
    {
        display.print("No          >Yes");
    };
}

void renderFrame3()
{
    if (editMode == 0)
    {
        page3();
    }
    else
    {
        editPage3();
    }
};

void renderFrame4()
{
    if (editMode == 0)
    {
        page4();
    }
    else
    {
        editPage4();
    }
}

void renderSavedFrame()
{
    display.setCursor(0, 0);
    display.print("   Data Saved   ");
    display.setCursor(0, 1);
    display.print("                ");
    if (millis() - dataSavedTimeoutTemp > DATA_SAVED_TIMEOUT)
    {
        dataSavedTimeoutTemp = millis();
        menuList = 0;
        editMode = 0;
    };
}

void page3()
{
    display.setCursor(0, 0);
    display.print("TEMP. CONTROL   ");
    display.setCursor(0, 1);
    display.print("Treshold:   ");
    display.print(temperatureTreshold);
    display.print(char(1));
    display.print("C");
};

void editPage3()
{
    display.setCursor(0, 0);
    display.print("Edit Treshold:  ");
    display.setCursor(0, 1);
    display.print("Value:");
    display.print("     ");
    display.print(">");
    display.print(temperatureTreshold);
    display.print(char(1));
    display.print("C");
    display.print("    ");
}

void page4()
{
    display.setCursor(0, 0);
    display.print("AUTO FEEDING SET");
    display.setCursor(0, 1);
    display.print("           >EDIT");
}

void editPage4()
{
    switch (editPage4List)
    {
    case 0:
        editPage4_1();
        break;
    case 1:
        editPage4_2();
        break;
    case 2:
        editPage4_3();
        break;
    case 3:
        editPage4_4();
        break;
    default:
        editPage4_1();
    };
}

void editPage4_1()
{
    display.setCursor(0, 0);
    display.print("Feed. time:");
    if (editPageFlag == 0)
    {
        display.print("  >");
    }
    else
    {
        display.print("   ");
    }
    display.print(feedingTime);
    display.print("s");
    display.setCursor(0, 1);
    if (editPageFlag == 1)
    {
        display.print(">Back       Next");
    }
    else if (editPageFlag == 2)
    {
        display.print("Back       >Next");
    }
    else
    {
        display.print("Back        Next");
    };
}

void editPage4_2()
{
    display.setCursor(0, 0);
    if (editPageFlag == 0)
    {
        display.print("TIME 1:   ");
        display.print(">");
        timeParser(feedingHour1);
        display.print(":");
        timeParser(feedingMinute1);
    }
    else if (editPageFlag == 1)
    {
        display.print("TIME 1:   ");
        timeParser(feedingHour1);
        display.print(":");
        display.print(">");
        timeParser(feedingMinute1);
    }
    else
    {
        display.print("TIME 1:    ");
        timeParser(feedingHour1);
        display.print(":");
        timeParser(feedingMinute1);
    };
    display.setCursor(0, 1);
    if (editPageFlag == 2)
    {
        display.print(">Back       Next");
    }
    else if (editPageFlag == 3)
    {
        display.print("Back       >Next");
    }
    else
    {
        display.print("Back        Next");
    };
}

void editPage4_3()
{
    display.setCursor(0, 0);
    if (editPageFlag == 0)
    {
        display.print("TIME 2:   ");
        display.print(">");
        timeParser(feedingHour2);
        display.print(":");
        timeParser(feedingMinute2);
    }
    else if (editPageFlag == 1)
    {
        display.print("TIME 2:   ");
        timeParser(feedingHour2);
        display.print(":");
        display.print(">");
        timeParser(feedingMinute2);
    }
    else
    {
        display.print("TIME 2:    ");
        timeParser(feedingHour2);
        display.print(":");
        timeParser(feedingMinute2);
    };
    display.setCursor(0, 1);
    if (editPageFlag == 2)
    {
        display.print(">Back       Next");
    }
    else if (editPageFlag == 3)
    {
        display.print("Back       >Next");
    }
    else
    {
        display.print("Back        Next");
    };
}

void editPage4_4()
{
    display.setCursor(0, 0);
    if (editPageFlag == 0)
    {
        display.print("TIME 3:   ");
        display.print(">");
        timeParser(feedingHour3);
        display.print(":");
        timeParser(feedingMinute3);
    }
    else if (editPageFlag == 1)
    {
        display.print("TIME 3:   ");
        timeParser(feedingHour3);
        display.print(":");
        display.print(">");
        timeParser(feedingMinute3);
    }
    else
    {
        display.print("TIME 3:    ");
        timeParser(feedingHour3);
        display.print(":");
        timeParser(feedingMinute3);
    };
    display.setCursor(0, 1);
    if (editPageFlag == 2)
    {
        display.print(">Back       Save");
    }
    else if (editPageFlag == 3)
    {
        display.print("Back       >Save");
    }
    else
    {
        display.print("Back        Save");
    };
}

void renderFrame5()
{
    if (editMode == 0)
    {
        page5();
    }
    else
    {
        editPage5();
    };
};

void editPage5()
{
    switch (editPage5List)
    {
    case 0:
        editPage5_1();
        break;
    case 1:
        editPage5_2();
        break;
    default:
        editPage5_1();
        break;
    };
};

void page5()
{
    display.setCursor(0, 0);
    display.print("LIGHTS SETTINGS ");
    display.setCursor(0, 1);
    timeParser(lightsOnHour);
    display.print(":");
    timeParser(lightsOnMinutes);
    display.print(" <--> ");
    timeParser(lightsOffHour);
    display.print(":");
    timeParser(lightsOffMinutes);
};

void editPage5_1()
{
    display.setCursor(0, 0);
    if (editPageFlag == 0)
    {
        display.print("ON:");
        display.print("       ");
        display.print(">");
        timeParser(lightsOnHour);
        display.print(":");
        timeParser(lightsOnMinutes);
    }
    else if (editPageFlag == 1)
    {
        display.print("ON:");
        display.print("       ");
        timeParser(lightsOnHour);
        display.print(":");
        display.print(">");
        timeParser(lightsOnMinutes);
    }
    else
    {
        display.print("ON:");
        display.print("        ");
        timeParser(lightsOnHour);
        display.print(":");
        timeParser(lightsOnMinutes);
    }

    display.setCursor(0, 1);
    if (editPageFlag == 2)
    {
        display.print(">Back       Next");
    }
    else if (editPageFlag == 3)
    {
        display.print("Back       >Next");
    }
    else
    {
        display.print("Back        Next");
    };
};

void editPage5_2()
{
    display.setCursor(0, 0);
    if (editPageFlag == 0)
    {
        display.print("OFF:");
        display.print("      ");
        display.print(">");
        timeParser(lightsOffHour);
        display.print(":");
        timeParser(lightsOffMinutes);
    }
    else if (editPageFlag == 1)
    {
        display.print("OFF:");
        display.print("      ");
        timeParser(lightsOffHour);
        display.print(":");
        display.print(">");
        timeParser(lightsOffMinutes);
    }
    else
    {
        display.print("OFF:");
        display.print("       ");
        timeParser(lightsOffHour);
        display.print(":");
        timeParser(lightsOffMinutes);
    }

    display.setCursor(0, 1);
    if (editPageFlag == 2)
    {
        display.print(">Back       Save");
    }
    else if (editPageFlag == 3)
    {
        display.print("Back       >Save");
    }
    else
    {
        display.print("Back        Save");
    };
};

void renderFrame6()
{
    if (editMode == 0)
    {
        page6();
    };
    if (editMode == 1)
    {
        editPage6_1();
    };
};

void page6()
{
    display.setCursor(0, 0);
    display.print("MANUAL FEEDING  ");
    display.setCursor(0, 1);
    display.print("          >ENTER");
};

void editPage6_1()
{
    display.setCursor(0, 0);
    if (feedingStatus == 0)
    {
        display.print("Hold <BUTTON>   ");
    }
    else if (feedingStatus == 1)
    {
        display.print("Feeding now...  ");
    };
    display.setCursor(0, 1);
    if (editPageFlag == 0)
    {
        display.print(">Feeding    Exit");
    }
    else if (editPageFlag == 1)
    {
        display.print("Feeding    >Exit");
    };
};

void renderTime()
{
    display.setCursor(0, 0);
    display.print("Time:   ");
    timeParser(hours);
    display.print(":");
    timeParser(minutes);
    display.print(":");
    timeParser(seconds);
};

void renderTemperature(int line)
{
    display.print("t:");
    display.print("        ");
    display.print(temperature, DEC);
    display.setCursor(14, line);
    display.print(char(1));
    display.print("C");
};

void getTime()
{
    if (millis() - tempClockUpdateTime > CLOCK_UPDATE_TIME)
    {
        tempClockUpdateTime = millis();
        DateTime now = rtc.now();
        hours = now.hour();
        minutes = now.minute();
        seconds = now.second();
        day = now.day();
        month = now.month();
        year = now.year();
    };
};

void timeParser(int time)
{
    if (time < 10)
    {
        display.print("0");
        display.print(time);
    }
    else
    {
        display.print(time);
    };
};

void detectTemperature()
{
    byte data[2];
    tempSensor.reset();
    tempSensor.write(0xCC);
    tempSensor.write(0x44);

    if (millis() - tempLastUpdateTime > TEMP_UPDATE_TIME)
    {
        tempLastUpdateTime = millis();
        tempSensor.reset();
        tempSensor.write(0xCC);
        tempSensor.write(0xBE);
        data[0] = tempSensor.read();
        data[1] = tempSensor.read();

        temperature = ((data[1] << 8) | data[0]) * 0.0625;
        temperature = round(temperature * 10.) / 10.;
    };
};
