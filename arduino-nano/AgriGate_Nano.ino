// AgriGate Arduino Nano
// Water-level sensing and ESP32 communication

#define CHANNEL_LOW   2
#define CHANNEL_MED   3
#define CHANNEL_HIGH  4

#define FIELD_LOW     5
#define FIELD_MED     6
#define FIELD_HIGH    7

int getLevel(int lowPin, int medPin, int highPin) {
  bool low  = !digitalRead(lowPin);
  bool med  = !digitalRead(medPin);
  bool high = !digitalRead(highPin);

  if (high) return 2;
  if (med) return 1;
  if (low) return 0;
  return 0;
}

void setup() {
  Serial.begin(9600);

  pinMode(CHANNEL_LOW, INPUT_PULLUP);
  pinMode(CHANNEL_MED, INPUT_PULLUP);
  pinMode(CHANNEL_HIGH, INPUT_PULLUP);

  pinMode(FIELD_LOW, INPUT_PULLUP);
  pinMode(FIELD_MED, INPUT_PULLUP);
  pinMode(FIELD_HIGH, INPUT_PULLUP);
}

void loop() {
  int channelLevel = getLevel(CHANNEL_LOW, CHANNEL_MED, CHANNEL_HIGH);
  int fieldLevel = getLevel(FIELD_LOW, FIELD_MED, FIELD_HIGH);

  Serial.print("C:");
  Serial.print(channelLevel);
  Serial.print(",F:");
  Serial.println(fieldLevel);

  delay(500);
}
