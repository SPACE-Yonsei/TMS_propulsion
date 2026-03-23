// 모든 pin에서 Interrupt를 사용할 수 있도록 만들어주는 EnableInterrupt Library, 개별적으로 설치 필요
#include <EnableInterrupt.h>

// HX711과 Arduino Uno를 연결하는 핀 번호
const int LOADCELL_DT_PIN = 4;
const int LOADCELL_SCK_PIN = 5;

//const int transferPin = 7;  // 무슨 pin인지 모르겠음

// 스위치박스 Arduino와 연결된 핀 번호 (점화 지연 시간 측정용)
const int INPUT_PIN = 2;

// 압력센서와 연결된 핀 번호
const int PRESSURE_PIN = A5;  // Digital Pin에 연결

// 압력센서 데이터시트
const float MIN_VOLTAGE = 0.0;    // 최소 출력 전압 (V)
const float MAX_VOLTAGE = 5.0;    // 최대 출력 전압 (V)
const float MIN_PRESSURE = 0.0;   // 최소 전압일 때의 압력 (예: bar, psi, kPa)
const float MAX_PRESSURE = 70.0;  // 최대 전압일 때의 압력 (예: 10 bar)

// 센서로부터 입력된 값을 저장하는 변수들
int RawPressureValue = 0;  // 압력 센서의 Raw Value, 이를 Post Processing하여 Pressure값으로 변환함
float pressureVoltage = 0.0;    // 읽은 값을 전압(0.0V ~ 5.0V)으로 변환한 값을 담는 변수
float finalPressure = 0.0;  // 실제로 계산된 압력 (단위: bar)

volatile long RawForceValue = 0;  // 로드셀의 측정값, 이는 Post Processing이 필요 없음
volatile bool newForce = false; // 성능 최적화를 위한 변수
float finalWeight = 0.0;  // 계산된 실제 무게 (단위: N)

volatile unsigned long ignitedTime = 0; // 점화 시간을 저장하는 변수

// Load Cell Calibration Factor
// 다른 Load Cell을 사용할 때는 Calibration Factor를 다시 측정해서 사용해야 함
const float CALIBRATION_FACTOR = -4371.18f; // 가장 마지막으로 측정한 Calibration factor

// Load Cell Offset
long OFFSET = 0;  // Load Cell의 Offset을 시작할 때마다 측정

void setup() {
  Serial.begin(115200);   // Serial Monitor를 115200으로 시작
  
  // pinMode 설정
  pinMode(PRESSURE_PIN, INPUT);
  //pinMode(transferPin, INPUT_PULLUP);
  pinMode(INPUT_PIN, INPUT_PULLUP);

  pinMode(LOADCELL_DT_PIN, INPUT);
  pinMode(LOADCELL_SCK_PIN, OUTPUT);
  digitalWrite(LOADCELL_SCK_PIN, LOW);  // SCK Pin Initialization

  // Interrupt 핀 설정
  enableInterrupt(LOADCELL_DT_PIN, load_cell_event, FALLING);
  enableInterrupt(INPUT_PIN, ignition_event, FALLING);

  Serial.print("Start Calibrating Load Cell Offset");
  delay(1000); // 장치들이 전기를 공급받아 초기 안정화가 될 수 있도록 시간을 줌
  // Loadcell Offset Calibration
  long temp_sumRaw = 0;
  newForce = false; // 변수 초기화

  for (int i=0; i<10; i++) {  // Noise를 고려하여 10번 측정하여 Calibration 진행
    while (!newForce) {
      // 새로운 데이터를 측정할 때까지 대기
    }
    // 데이터 처리 중 값이 중복되는 현상을 방지
    noInterrupts();
    temp_sumRaw += RawForceValue;
    newForce = false;
    interrupts();
    //Serial.println(RawForceValue); // Only Use For Debugging
  }

  OFFSET = temp_sumRaw / 10;   // 최초 Load Cell 측정값의 평균
  Serial.print("New OFFSET: ");
  Serial.println(OFFSET);

  Serial.println("Initialization Completed");
}

void loop() {
  // Load Cell의 센서 값이 준비되었을 경우
  if (newForce) {
    // Interrupt와 공유하는 변수값의 무결성 유지를 위해 잠시 모든 Interrupt 중단, Interrupt와 공유하는 변수를 복사하고 다시 Interrupt 시작
    // 일반적으로 Interrupt가 중단되는 시간은 1ms 미만이므로 측정에 영향을 주지 않음
    noInterrupts();
    long copiedForce = RawForceValue;  // 데이터의 유실을 막기 위한 변수
    unsigned long copiedIgnitedTime = ignitedTime;  // 점화 지연 시간 저장 변수의 무결성 유지를 위한 변수
    newForce = false;
    interrupts();

    // Raw Value로부터 [N] 단위로 값 변환
    finalWeight = (copiedForce - OFFSET) / CALIBRATION_FACTOR;  // 계산된 실제 무게 (단위: N)

    // Load Cell 값을 읽었을 때 압력 센서 값도 같이 측정
    RawPressureValue = analogRead(PRESSURE_PIN);

    // Raw Value로부터 [bar] 단위로 값 변환
    // 읽은 값을 전압(0.0V ~ 5.0V)으로 변환하고, 1023.0으로 나누어 부동소수점 연산을 수행
    pressureVoltage = RawPressureValue * (MAX_VOLTAGE / 1023.0);  // 0.05 voltage = 0.1MPa = 1bar
    finalPressure = MIN_PRESSURE + (pressureVoltage - MIN_VOLTAGE) * (MAX_PRESSURE - MIN_PRESSURE) / (MAX_VOLTAGE - MIN_VOLTAGE); // 실제로 계산된 압력 (단위: bar)

    // 센서가 측정 범위를 벗어났을 때 값이 이상하게 표시되는 것을 방지
    if (finalPressure < MIN_PRESSURE) {
      finalPressure = MIN_PRESSURE;
    }
    if (finalPressure > MAX_PRESSURE) {
      finalPressure = MAX_PRESSURE;
    }

    // Serial Monitor에 출력할 Text 출력
    unsigned long currentTime = millis();   // 현재 시간

    // F()를 사용할 시 프로그램 실행 시 변수와 같은 동적 데이터를 저장하는 SRAM에 Flash Memory에 저장된 String을 올리는 대신,
    // 프로그램이 저장되는 Flash Memory에만 String을 저장하고 이를 바로 읽도록 강제하여
    // 메모리를 절약할 수 있음
    Serial.print(F("millis: "));  Serial.print(currentTime);  // 현재 시간 출력
    Serial.print(F(" | Pressure Sensor Voltage: "));  Serial.print(pressureVoltage, 2); Serial.print(F("[V]")); // 압력 센서 Voltage 출력
    Serial.print(F(" | Pressure: ")); Serial.print(finalPressure, 2); Serial.print(F("[bar]")); // 후처리된 압력 센서 값 출력 (실제 압력)
    Serial.print(F(" | Force: "));  Serial.print(finalWeight * -1.0, 3);  Serial.print(F("[N]")); // 로드셀에서 측정되어 후처리된 추력 출력 (실제 측정된 힘)
    Serial.print(F(" | Ignition Time: "));  Serial.println(copiedIgnitedTime);  // 스위치를 누른 시간 출력
  }
}

void ignition_event() { // 점화 지연 시간 측정용 ISR
  ignitedTime = millis();
}

void load_cell_event() {  // Load Cell의 ISR
  disableInterrupt(LOADCELL_DT_PIN);  // DT Pin의 신호 처리로 인한 Interrupt 신호가 재귀적으로 전달되지 않도록 방지
  RawForceValue = read_hx711_register();
  newForce = true;
  enableInterrupt(LOADCELL_DT_PIN, load_cell_event, FALLING); // DT Pin을 통한 데이터 전송이 완료되었으므로, 다시 Interrupt 활성화
}

long read_hx711_register() {
  // Load Cell의 HX711에 직접 신호를 보내 메모리를 읽음
  long count = 0;
  for (int i = 0; i < 24; i++) {
    digitalWrite(LOADCELL_SCK_PIN, HIGH);
    count = count << 1;           
    digitalWrite(LOADCELL_SCK_PIN, LOW);
    if (digitalRead(LOADCELL_DT_PIN)) { count++; }
  }
  digitalWrite(LOADCELL_SCK_PIN, HIGH);
  digitalWrite(LOADCELL_SCK_PIN, LOW);
  
  if (count & 0x800000) { count |= 0xFF000000; }
  
  return count;
}