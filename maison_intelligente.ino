#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Stepper.h>

// Configuration matérielle
#define TRIG_PIN 5
#define ECHO_PIN 6
#define STEPS_PER_REV 2048
#define MOTOR_PIN1 31
#define MOTOR_PIN2 33
#define MOTOR_PIN3 35
#define MOTOR_PIN4 37
#define BUZZER_PIN 7
#define RGB_RED_PIN 8
#define RGB_GREEN_PIN 9
#define RGB_BLUE_PIN 11

// Paramètres système
#define OPEN_ANGLE 170
#define CLOSED_ANGLE 10
#define TRANSITION_TIME 2000
#define OPEN_DISTANCE 30
#define CLOSE_DISTANCE 60
#define ALARM_DISTANCE 15
#define ALARM_TIMEOUT 3000

LiquidCrystal_I2C lcd(0x27, 16, 2);
Stepper stepper(STEPS_PER_REV, MOTOR_PIN1, MOTOR_PIN3, MOTOR_PIN2, MOTOR_PIN4);

// États du système
enum DoorState {
  INIT,
  CLOSED,
  OPENING,
  OPEN,
  CLOSING,
  ALERTE
};
DoorState doorState = INIT;

// Variables globales
unsigned long currentMillis = 0;
unsigned long moveStartTime = 0;
unsigned long lastDetection = 0;
int currentAngle = CLOSED_ANGLE;
int previousAngle = CLOSED_ANGLE;
const float STEPS_PER_DEGREE = STEPS_PER_REV / 360.0;

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RGB_RED_PIN, OUTPUT);
  pinMode(RGB_GREEN_PIN, OUTPUT);
  pinMode(RGB_BLUE_PIN, OUTPUT);

  lcd.init();
  lcd.backlight();
  stepper.setSpeed(15);

  showStartScreen();
  doorState = CLOSED;
}

void loop() {
  currentMillis = millis();
  manageDoorState();
  updateDisplay();
  sendSerialData();
}

void showStartScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("2384980");
  lcd.setCursor(0, 1);
  lcd.print("Labo 5");
  delay(2000);
  lcd.clear();
}

void manageDoorState() {
  int distance = getDistance();


  switch (doorState) {
    case CLOSED:
      if (distance <= ALARM_DISTANCE) {
        doorState = ALERTE;
        lastDetection = currentMillis;
      } else if (distance < OPEN_DISTANCE) {
        doorState = OPENING;
        moveStartTime = currentMillis;
      }
      // Éteindre toutes les couleurs
      digitalWrite(RGB_RED_PIN, LOW);
      digitalWrite(RGB_GREEN_PIN, LOW);
      digitalWrite(RGB_BLUE_PIN, LOW);
      break;

    case OPENING:
      currentAngle = map(currentMillis - moveStartTime, 0, TRANSITION_TIME, CLOSED_ANGLE, OPEN_ANGLE);
      moveStepper();
      if (currentAngle >= OPEN_ANGLE) {
        currentAngle = OPEN_ANGLE;
        doorState = OPEN;
      }
      break;

    case OPEN:
      if (distance <= ALARM_DISTANCE) {
        doorState = ALERTE;
        lastDetection = currentMillis;
      } else if (distance >= CLOSE_DISTANCE) {
        doorState = CLOSING;
        moveStartTime = currentMillis;
      }
      // Pas de couleur lorsque la porte est ouverte
      // digitalWrite(RGB_GREEN_PIN, HIGH);
      // digitalWrite(RGB_RED_PIN, HIGH);
      // digitalWrite(RGB_BLUE_PIN, HIGH);
      break;

    case CLOSING:
      currentAngle = map(currentMillis - moveStartTime, 0, TRANSITION_TIME, OPEN_ANGLE, CLOSED_ANGLE);
      moveStepper();
      if (currentAngle <= CLOSED_ANGLE) {
        currentAngle = CLOSED_ANGLE;
        doorState = CLOSED;
      }
      digitalWrite(RGB_RED_PIN, LOW);
      digitalWrite(RGB_GREEN_PIN, LOW);
      digitalWrite(RGB_BLUE_PIN, LOW);
      // Éteindre RGB
      // digitalWrite(RGB_RED_PIN, HIGH);
      // digitalWrite(RGB_GREEN_PIN, HIGH);
      // digitalWrite(RGB_BLUE_PIN, HIGH);
      break;

    case ALERTE:
      if (distance <= ALARM_DISTANCE) {
        lastDetection = currentMillis;
      }
      // Alternance RGB rouge/bleu (LOW pour allumer car anode commune)
      bool rougeActif = ((currentMillis / 300) % 2 == 0);
      digitalWrite(RGB_RED_PIN, rougeActif ? LOW : HIGH);
      digitalWrite(RGB_BLUE_PIN, rougeActif ? LOW : HIGH);
      digitalWrite(RGB_GREEN_PIN, HIGH);

      digitalWrite(BUZZER_PIN, HIGH);

      if (currentMillis - lastDetection > ALARM_TIMEO-UT) {
        // digitalWrite(RGB_RED_PIN, HIGH);
        // digitalWrite(RGB_BLUE_PIN, HIGH);
        // digitalWrite(RGB_GREEN_PIN, HIGH);
        digitalWrite(BUZZER_PIN, LOW);
        doorState = (currentAngle > (CLOSED_ANGLE + 5)) ? OPEN : CLOSED;
      }
      break;
  }
}

void moveStepper() {
  int stepsToMove = (currentAngle - previousAngle) * STEPS_PER_DEGREE;

  if (stepsToMove != 0) {
    stepper.step(stepsToMove);
    previousAngle = currentAngle;

    digitalWrite(MOTOR_PIN1, LOW);
    digitalWrite(MOTOR_PIN2, LOW);
    digitalWrite(MOTOR_PIN3, LOW);
    digitalWrite(MOTOR_PIN4, LOW);
  }
}

int getDistance() {
  static unsigned long lastMeasure = 0;
  static int lastDistance = 0;

  if (currentMillis - lastMeasure < 50) return lastDistance;
  lastMeasure = currentMillis;

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 25000);
  int distance = (duration * 0.0343) / 2;
  if (distance < 2 || distance > 300) return lastDistance;

  lastDistance = distance;
  return distance;
}

void updateDisplay() {
  static unsigned long lastUpdate = 0;

  if (currentMillis - lastUpdate < 100) return;
  lastUpdate = currentMillis;

  int distance = getDistance();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Distance: ");
  lcd.print(distance);
  lcd.print("cm");

  lcd.setCursor(0, 1);
  switch (doorState) {
    case CLOSED:
      lcd.print("Porte : Fermee");
      break;
    case OPEN:
      lcd.print("Porte : Ouverte");
      break;
    case OPENING:
    case CLOSING:
      lcd.print("Porte: ");
      lcd.print(currentAngle);
      lcd.print(" deg");
      break;
    case ALERTE:
      lcd.print("!! ALARME !!");
      break;
    default:
      lcd.print("Etat inconnu");
      break;
  }
}

void sendSerialData() {
  static unsigned long lastSend = 0;

  if (currentMillis - lastSend < 100) return;
  lastSend = currentMillis;

  Serial.print("etd:2384980,dist:");
  Serial.print(getDistance());
  Serial.print(",deg:");
  Serial.println(currentAngle);
}
