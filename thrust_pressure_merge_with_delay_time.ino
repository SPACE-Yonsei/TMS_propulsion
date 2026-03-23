#include <HX711.h>

// HX711 회로에 연결된 핀 번호
const int LOADCELL_DOUT_PIN = 4;
const int LOADCELL_SCK_PIN = 5;
const int transferPin = 7;
const int INPUT_PIN = 2;

volatile unsigned long eventTime = 0;

//압력센서값 입력 핀 번호
const int PRESSURE_PIN = A5;

//압력센서 데이터시트에 따라 수정
const float MIN_VOLTAGE = 0.0;    // 최소 출력 전압 (V)
const float MAX_VOLTAGE = 5.0;    // 최대 출력 전압 (V)
const float MIN_PRESSURE = 0.0;   // 최소 전압일 때의 압력 (예: bar, psi, kPa)
const float MAX_PRESSURE = 70.0;  // 최대 전압일 때의 압력 (예: 10 bar)

HX711 scale;

//시간 기록용
unsigned long pre_t = 0, cur_t = 0;


void setup() {
  Serial.begin(115200);
  pinMode(PRESSURE_PIN, INPUT);
  pinMode(transferPin, INPUT);
  pinMode(INPUT_PIN, INPUT); // 입력 모드 설정
  attachInterrupt(digitalPinToInterrupt(INPUT_PIN), ignition_fired, RISING);

  //HX711 초기화
  Serial.println("HX711 초기화 중...");
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  //로드셀 스케일 보정
  Serial.println("로드셀 스케일 보정 전 평균값 읽는 중...");
  float calibration_factor = -4371.18;  // 이 값은 로드셀에 따라 조정해야 합니다
  scale.set_scale(calibration_factor);
  scale.tare();  // 영점 조정

  Serial.println("측정 시작...");

  //루프 시작 시간
  pre_t = millis();
}
  
void loop() {
  cur_t = millis();

  //로드셀
  if (cur_t - pre_t > 12) {

    //80hz인 HX711이 데이터 보내올 경우 시리얼 모니터에 결과 출력
    if (scale.is_ready()) {
      //scale.is_ready()의 값 반환에 시간 소요될 우려로 cur_t 재동기화
      cur_t = millis();

      int sensorValue = analogRead(PRESSURE_PIN);  // 10 = 0.05V 정도
      float Force = scale.get_units();

      //압력센서
      // 읽은 값을 전압(0.0V ~ 5.0V)으로 변환합니다.
      // 1023.0으로 나누어 부동소수점 연산을 수행합니다.
      float voltage = sensorValue * (5 / 1023.0);  // 0.05 voltage = 0.1MPa = 1bar

      // 전압 값을 실제 압력 값으로 변환합니다. (선형 비례식)
      // map() 함수는 정수형에서만 정확하므로, 부동소수점 계산을 직접 수행합니다.
      float pressure = MIN_PRESSURE + (voltage - MIN_VOLTAGE) * (MAX_PRESSURE - MIN_PRESSURE) / (MAX_VOLTAGE - MIN_VOLTAGE);

      // 센서가 측정 범위를 벗어났을 때 값이 이상하게 표시되는 것을 방지합니다.
      if (pressure < MIN_PRESSURE) {
        pressure = MIN_PRESSURE;
      }
      if (pressure > MAX_PRESSURE) {
        pressure = MAX_PRESSURE;
      }

      // 시리얼 모니터에 결과를 출력합니다.
      //pressure
      Serial.print("millis: ");
      Serial.print(cur_t);
      Serial.print(" | Raw Value: ");
      Serial.print(sensorValue);
      Serial.print(" | Voltage: ");
      Serial.print(voltage, 2);
      Serial.print(" V | Pressure: ");
      Serial.print(pressure, 2);
      Serial.print("bar");
      
      //force
      Serial.print(" | Force: ");
      Serial.print(Force, 3);
      Serial.print(" | Switch: ");
      Serial.print(digitalRead(transferPin));

      int pinState = digitalRead(INPUT_PIN);

      Serial.print(" | Triggered: ");
      Serial.println(eventTime);

    } else {
      Serial.println("HX711 준비되지 않음");
    }

    pre_t = cur_t;
  }
}

// 점화 스위치 작동 시간 측정용 함수 (실행 조건 만족 시 다른 작업을 멈추고 가장 우선적으로 실행됨)
void ignition_fired() {
  eventTime = millis();
}