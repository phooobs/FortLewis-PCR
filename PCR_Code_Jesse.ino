#include <math.h>
const int inA = 13; // pin connected to INA on VHN5019
const int inB = 12; // pin connected to INB on VHN5019
const int ppwm = 11; // pin connected to PWM on VHN5019
const int fpwm = 10; // pin connected to PWM on fan
const int thermP = A0; // pin connected to thermal resistor neetwork. see elegooThermalResistorSch.png
const int LidP = A1; // pin for thermal resistor conneccted to lid
bool power = false; // software on/off
const int ssr = 8; // solid state relay signal


unsigned long time; // jd
unsigned long timeS; // jd

double pieltierDelta = 0; // the change in degrees celcius of pieltier wanted over the next time step
int kPieltierAverage = 10;

// for converting pieltierDelta to pwm
const double pwmDivPieltierDelta = 1023.0 / 60.0;

double targetPieltierTemp = 40; // the tempature the system will try to move to, in degrees C
double currentPieltierTemp; // the tempature curently read from the thermoristor connected to thermP, in degrees C
double currentLidTemp; 
double LastLidTemp;

// class for creating a pid system
class pid {
  private:
  double kp; // higher moves faster
  double ki; // higher fixes ofset and faster
  double kd; // higher settes faster but creastes ofset
  double kAmbiant; // experimental, fixes ofset?, may remove. leave at zero if unshure
  unsigned long currentTime, lastTime; // the time in millisecconds of this timesep and the last timestep
  double pError, lError, iError, dError; // error values for pid calculations. p: porportional, l: last, i: integerl, d: derivitive
    
  public:
  pid (double proportionalGain = 1, double integralGain = 0, double derivativeGain = 0) {
    kp = proportionalGain; // higher moves faster
    ki = integralGain; // higher fixes ofset and faster
    kd = derivativeGain; // higher settes faster but creastes ofset
  }
  
  double calculate(double currentTemp, double targetTemp){ // gets tempature and performs pid calculation returns error in degrees C
    lastTime = currentTime;
    currentTime = millis();
    lError = pError;
    pError = targetTemp - currentTemp;
    iError = pError * (double)(currentTime - lastTime);
    dError = (pError - lError) / (double)(currentTime - lastTime);
    return kp * pError + ki * iError + kd * dError;
  }
};

// for creating a tempature sensor
// includes noise reduction
// call resetTemp() before a series of getTemp() calls
// see elegooThermalResistorSch.png for wireing
class TempSensor {
  private:
  int pin;
  double temp;
  double lastK;
  
  public:
  TempSensor(int iPin) { 
    pin = iPin; // the pin that conected to the tempature network
  }
  
  void resetTemp() { // resets saved privois tempature, nessasary if it has been a while since last getTemp()
    int tempReading = analogRead(pin);
    double tempK = log(10000.0 * ((1024.0 / tempReading - 1)));
    lastK = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * tempK * tempK )) * tempK );
  }

  double getTemp() { // returns the tempature from thermoristor connected to thermP in degrees C, includes noise reduction
    static double spike0u;
    int tempReading = analogRead(pin);
    double tempK = log(10000.0 * ((1024.0 / tempReading - 1)));
    tempK = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * tempK * tempK )) * tempK ); // kelvin

    //Serial.print(tempK - 273.15);
    //Serial.print(" ");
    
    // noise peak removal
    
    if (tempK - lastK > 0.5 + abs(lastK -299.15) * 0.1) {
      if (spike0u == 0) {
        spike0u = tempK - lastK;
      }
      tempK -= spike0u;
    } else {
      spike0u = 0;
    }

    if (tempK - lastK < -10) {
      tempK += spike0u;
      spike0u = 0;
    }
    
    lastK = tempK;
    
    return (tempK - 273.15); // convert kelvin to celcius
  }
};


// setup pieltier tempature sensor
TempSensor pieltierT(thermP);
TempSensor LidT(LidP); // JD setup for thermo resistor temp 
// setup pieltier PID
pid pieltierPID(1, 1, 4000);

void setup() {
  // setup serial
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // setup pins
  pinMode(inA, OUTPUT);
  pinMode(inB, OUTPUT);
  pinMode(ppwm, OUTPUT);
  pinMode(fpwm, OUTPUT);
  pinMode(thermP, INPUT);
  pinMode(ssr, OUTPUT);

  // set initial pin state to off
  digitalWrite(inA, LOW);
  digitalWrite(inB, LOW);
  analogWrite(ppwm, 0);
  analogWrite(fpwm, 0);
  digitalWrite(ssr,LOW);
}

void loop() {

  // commands
  // off
  //   power off
  // on
  //   power on
  // pt[floatValue]
  //   sets targetPieltierTemp to floatValue

// Time stuff for graphing 
time = millis();
  timeS=time/1000; // conversion to seconds

  
  if (Serial.available() > 0) {
    String incomingCommand = Serial.readString();
    if (incomingCommand == "off\n") {
      power = false;
    }
    if (incomingCommand == "on\n") {
      power = true;
    }
    if (incomingCommand.substring(0,2) == "pt") {
      targetPieltierTemp = incomingCommand.substring(2).toFloat();
    }
  }




  

  currentPieltierTemp = pieltierT.getTemp(); // read pieltier temp
  LidT.resetTemp();
  currentLidTemp = LidT.getTemp();
  pieltierDelta = pieltierPID.calculate(currentPieltierTemp, targetPieltierTemp); // calculate pid and set to output
  pieltierDelta = min(60, max(-60, pieltierDelta)); // clamp output between -60 and 60

  // for graphing system state
 /* Serial.print(currentPieltierTemp);
  Serial.print(" ");
  Serial.print(pieltierDelta * pwmDivPieltierDelta);
  Serial.print("\n");

  Serial.print("LID temp = ");  
  */
 // Serial.print(timeS);
 // Serial.print(",");
 // Serial.print("cLIDTemp ");
  Serial.print(currentLidTemp);
  Serial.print("\n");
  //Serial.print("");
  //Serial.print(currentLidTemp-LastLidTemp);
//  Serial.print("\n");
 // delay(2000);  
  
// Turns ssr on/off based on temperature


  if((currentLidTemp- LastLidTemp)<=0){
  //  Serial.print("decreasing situation \n");
    
    if(currentLidTemp >=110){ // turn off pad if over 110
       digitalWrite(ssr,LOW);
    //   Serial.print("Off @ decreasing");
     //  Serial.print("\n");
       }
     else if(currentLidTemp <= 90){
      digitalWrite(ssr,HIGH);
      //  Serial.print("on @ decreasing");
   //  Serial.print("\n");
    }
    else{
    digitalWrite(ssr,LOW);
    }
} //end decreasinglid temp situation


 else if((currentLidTemp - LastLidTemp)>0){

 // Serial.print("increasing situation \n");
 
  
    if(currentLidTemp >=110){ // turn off pad if over 110... set to 30 for example
    digitalWrite(ssr,LOW);
   // Serial.print("Off --- increasing");
  //  Serial.print("\n");
      }
   else if(currentLidTemp <= 110){
    digitalWrite(ssr,HIGH);
 //   Serial.print("on --- increasing");
//    Serial.print("\n");
    }
    else{
    digitalWrite(ssr,LOW);
    }

 } // end increasing lid situation


  
 
 
  LastLidTemp = currentLidTemp;
  
  if (!power || currentPieltierTemp > 150) {// shut off system if over 150 degrees for safety
    digitalWrite(inA, LOW);
    digitalWrite(inB, LOW);
    analogWrite(fpwm, 1024);
    analogWrite(ppwm, 0);
    pieltierT.resetTemp();
    LidT.resetTemp();// resest temp upon reset
    return;
  }
  

  // convert pieltierDelta to pwm, inA, inB, and fan signals
  analogWrite(ppwm, abs(pieltierDelta * pwmDivPieltierDelta));
  if (pieltierDelta > 0) {
    digitalWrite(inA, HIGH);
    digitalWrite(inB, LOW);
    analogWrite(fpwm, 0);
  } else {
    digitalWrite(inA, LOW);
    digitalWrite(inB, HIGH);
    analogWrite(fpwm, abs(pieltierDelta * pwmDivPieltierDelta * 3));
  }

}
