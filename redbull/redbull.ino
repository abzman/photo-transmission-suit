#include <FastSPI_LED.h>
#include <Encabulator.h>
#include <Wire.h>

#define NUM_LEDS 32

int loopCounter = 0;

//int tempR = 0;
//int tempG = 0;
//int tempB = 0;

int armsR[] = {255,255,255,255,255,255,255,255,255,255,255,255,192,160,128,128,224,192,160,128,128,128,255,224,192,160,128,96, 80, 64, 64, 64 };
int armsG[] = {0,  0,  0,  0,  16, 32, 48, 64, 80, 96, 112,128,128,128,128,128,128,128,128,128,128,128,255,224,192,160,128,96, 80, 64, 64, 64 };
int armsB[] = {0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  64, 128,255,255,255,255,255,255,255,255,255,255};


boolean mode = 1; //1 = velocity, 0 = jerk
int red = 0;
int green = 0;
int blue = 0;
int jerkFloor = 2; //minimum jerk required to light up the suit
float rail = 60.0; //bounds set for velocity (so the blue-white-red scales to the end points)

float velocity = 0;
float previousVelocity = 0;
unsigned long lastReadJerk;
unsigned long lastReadVelocity;

int d = 10; //delay for arm chaser

struct ACCEL {
  int x;
  int y;
  int z;
};
struct ACCEL previousJerk = {0,0,0};
struct ACCEL jerk = {0,0,0};
struct CRGB { unsigned char b; unsigned char r; unsigned char g; };
struct CRGB *leds;

#define PIN 4

void setup() {
  pinMode(5, INPUT);
  digitalWrite(5,HIGH);
  
  Encabulator.upUpDownDownLeftRightLeftRightBA();
  Serial.begin(9600);
  Encabulator.setVerbose(true);
  FastSPI_LED.setLeds(NUM_LEDS);
  FastSPI_LED.setChipset(CFastSPI_LED::SPI_WS2811);
  FastSPI_LED.setPin(PIN);
  FastSPI_LED.init();
  FastSPI_LED.start();
  leds = (struct CRGB*)FastSPI_LED.getRGBData(); 
  
  lastReadJerk = millis();
  lastReadVelocity = millis();
  previousJerk = readAccelerometer();
}

struct ACCEL readAccelerometer() {
  struct ACCEL tmp = {abs(Encabulator.accelerometer.x()),
               abs(Encabulator.accelerometer.y()),
               abs(Encabulator.accelerometer.z()) };
  return tmp;
}

void updateVelocity() {
  
  //Update dT
  unsigned long currentTime = millis();
  unsigned long dT = currentTime - lastReadVelocity;
  lastReadVelocity = currentTime;
  
  //Read Acceleration
    float acc = (float)(Encabulator.accelerometer.z())/20.0;

  //Calculate kind of velocity
  velocity = previousVelocity + (acc * (dT/1000.0));
  previousVelocity = velocity;
  /*
  Serial.print("Velocity: ");
  Serial.print(velocity);
  Serial.print("    ");
  Serial.print("Acceleration: ");
  Serial.println(acc);
  */
 return;
}

void updateJerk() {
  //Update dT
  unsigned long currentTime = millis();
  unsigned long dT = currentTime - lastReadJerk;
  lastReadJerk = currentTime;
  
  //Read Acceleration
  struct ACCEL tmp = readAccelerometer();
  float accx = (float)(tmp.x - previousJerk.x)/10.0;
  float accy = (float)(tmp.y - previousJerk.y)/10.0;
  float accz = (float)(tmp.z - previousJerk.z)/10.0;

  //Calculate jerk
  jerk.x = (accx * (dT/40.0));
  jerk.y = (accy * (dT/40.0));
  jerk.z = (accz * (dT/40.0));
  //reset previous value
  previousJerk = tmp;
}

void lightAt(int i, int r, int g, int b) {
  leds[i].r = r;
  leds[i].g = b;
  leds[i].b = g;
  FastSPI_LED.show();
}

void loop() {
  updateVelocity();
  updateJerk();
  if (previousVelocity > rail)
  {
    previousVelocity = rail;
  }
  if (previousVelocity < -rail)
  {
    previousVelocity = -rail;
  }
  previousVelocity = previousVelocity*0.95;
  if (digitalRead(5) == LOW) 
  {
    
    if(mode)
    {//rail to rail red ramps up, blue ramps down, and green peaks in the middle for white instead of purple
      red = map(previousVelocity,-rail,rail,0.0,255.0);
      blue = map(previousVelocity,-rail,rail,255.0,0.0);
      if (previousVelocity < 0.0)
      {
        green = map(previousVelocity,((-rail)),0.0,0.0,128.0);    
      }
      else
      {
        green = map(previousVelocity,0.0,(rail),128.0,0.0); 
      }

    }
    else
    {
      red = abs(jerk.x);
      green = abs(jerk.y);
      blue = abs(jerk.z);
      if(red < jerkFloor)
        red = 0;
      if(green < jerkFloor)
        green = 0;
      if(blue < jerkFloor)
        blue = 0;
      Serial.println(red);
    }
    for (int i = 1 ; i < 5 ; i++) 
    {//sets body to all one color
    
     Encabulator.stripBankB.jumpHeaderToRGB(i,red,green,blue);
     //Encabulator.stripBankB.jumpHeaderToRGB(i,jerk.x,jerk.y,jerk.z);      
    }
    for(int i = 0;i<32;i++)//set arms to all one color
    {
      lightAt(i,red,green,blue);
      //lightAt(i,jerk.x,jerk.y,jerk.z);
    }
    
  } 
  else 
  {//arm button pressed
    for (int i = 1 ; i < 5 ; i++) //sets head, body, and legs to redish
    {
      Encabulator.stripBankB.jumpHeaderToRGB(i,255,16,0);        
    }
    
    for (int i = 0; i <32; ++i) //sets arms to off
    {
      lightAt(i,0,0,0);
    }
    for (int i = 0; i <32; ++i) //sets arms to black body chaser
    {
      lightAt(i,armsR[i],armsG[i],armsB[i]);
      delay(d);
    }
    delay(1000);//stays lit
    for (int i = 0; i <32; ++i) //chases down arms back to off
    {
      lightAt(i,0,0,0);
      delay(d);
    }
  delay(10);
  }
  

}


