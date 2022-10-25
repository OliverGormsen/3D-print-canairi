#include   
#include 
#include 
#include "Adafruit_SGP30.h"

// Initialize movingAvg lib
movingAvg avgECO2(60); 

// Define stepper
const int stepsPerRevolution = 512;  // change this to fit the number of steps per revolution for your motor
// initialize the stepper library on pins
Stepper myStepper(stepsPerRevolution, 2, 3, 4, 5);

// CO2 sensor reference
Adafruit_SGP30 sgp;


/* return absolute humidity [mg/m^3] with approximation formula
* @param temperature [°C]
* @param humidity [%RH]
*/
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
  // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
  const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
  const uint32_t absoluteHumidityScaled = static_cast(1000.0f * absoluteHumidity); // [mg/m^3]
  return absoluteHumidityScaled;
}

void setup() {
  // Moving avg setup
  avgECO2.begin();
  
  // CO2 sensor setup
  Serial.begin(115200);
  while (!Serial) { delay(10); } // Wait for serial console to open!

  Serial.println("SGP30 test");

  if (!sgp.begin()) {
    Serial.println("Sensor not found :(");
    while (1);
  }
  
  Serial.print("Found SGP30 serial #");
  Serial.print(sgp.serialnumber[0], HEX);
  Serial.print(sgp.serialnumber[1], HEX);
  Serial.println(sgp.serialnumber[2], HEX);

  // If you have a baseline measurement from before you can assign it to start, to 'self-calibrate'
  //sgp.setIAQBaseline(0x8E68, 0x8F41);  // Will vary for each sensor!

  myStepper.setSpeed(60);
}

// constants
const int TEN_SECONDS = 10000;
const int TEN_MINUTES = 600000;

// global variables
unsigned long lastCheck = 0;
unsigned long lastBaselineCalculation = 0;

int calAVG = 0;
bool birdUp = true;


void loop() {
  // If you have a temperature / humidity sensor, you can set the absolute humidity to enable the humditiy compensation for the air quality signals
  //float temperature = 22.1; // [°C]
  //float humidity = 45.2; // [%RH]
  //sgp.setHumidity(getAbsoluteHumidity(temperature, humidity));

  if (!sgp.IAQmeasure()) {
      Serial.println("Measurement failed");
      return;
    }

  //println to see the eCO2 in the plotter
  Serial.println(sgp.eCO2);
  //Serial.print(\);

  if (!sgp.IAQmeasureRaw()) {
    Serial.println("Raw Measurement failed");
    return;
  }

  unsigned long now = millis();

  // Calculating baseline values (read page 19 in the .pdf guide to see how this is done)
  if (lastBaselineCalculation + TEN_MINUTES < now) {
    Serial.print("TVOC "); Serial.print(sgp.TVOC); Serial.print(" ppb\t");
    Serial.print("eCO2 "); Serial.print(sgp.eCO2); Serial.println(" ppm");

    Serial.print("Raw H2 "); Serial.print(sgp.rawH2); Serial.print(" \t");
    Serial.print("Raw Ethanol "); Serial.print(sgp.rawEthanol); Serial.println("");
    
    uint16_t TVOC_base, eCO2_base;
    if (!sgp.getIAQBaseline(&eCO2_base, &TVOC_base)) {
      Serial.println("Failed to get baseline readings");
      return;
    }
    Serial.print("****Baseline values: eCO2: 0x"); Serial.print(eCO2_base, HEX);
    Serial.print(" & TVOC: 0x"); Serial.println(TVOC_base, HEX);

    lastBaselineCalculation = now;
  }
    
  //for calculating running avarage
  int eCO2Val = sgp.eCO2;
  int avg = avgECO2.reading(eCO2Val);
  //Serial.println(avg);

  if (lastCheck + TEN_MINUTES < now) {
    calAVG = avgECO2.getAvg();
    Serial.print("Average last 10 min = ");Serial.println(calAVG);
    
    // if the eCO2 value is above 1000 pmm && bird is up, initiate stepper to move the bird down
    if (calAVG >= 1000 && birdUp) {
      myStepper.step(stepsPerRevolution*2);
      delay(500);
      birdUp = !birdUp;
      Serial.print("Bird Down");
    } 
    // if the eCO2 value is below 800 pmm && bird is down, initiate stepper to move the bird up
    if (calAVG <= 800 && !birdUp) {
      myStepper.step(-stepsPerRevolution*2);
      birdUp = !birdUp;
      Serial.print("Bird up");
    }

    lastCheck = now;
  }  
  
  delay(TEN_SECONDS); // Loop delay
}
