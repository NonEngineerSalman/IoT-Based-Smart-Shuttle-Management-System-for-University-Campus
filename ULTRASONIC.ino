// include the library code
#include <LiquidCrystal.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(13, 12, 11, 10, 9, 8);

// defines pins numbers
const int trigPin = 2;
const int echoPin = 3;

// defines variables
long duration;
float distance_cm;
float distance_m;

void setup()
{
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  lcd.begin(20, 4);

  lcd.setCursor(0,0);
  lcd.print(" THE BRIGHT LIGHT ");

  lcd.setCursor(0,1);
  lcd.print("Distance Measure");
}

void loop()
{
  // Clear the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Send 10us pulse
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read echo
  duration = pulseIn(echoPin, HIGH);

  // Calculate distance in centimeters
  distance_cm = duration * 0.0343 / 2;

  // Convert centimeters to meters
  distance_m = distance_cm / 100.0;

  // Display centimeters
  lcd.setCursor(0,2);
  lcd.print("Distance: ");
  lcd.print(distance_cm, 1);   // One decimal place
  lcd.print(" cm   ");

  // Display meters
  lcd.setCursor(0,3);
  lcd.print("Distance: ");
  lcd.print(distance_m, 2);    // Two decimal places
  lcd.print(" m    ");

  delay(200);
}