#include "HX711.h"

// Terminology: Pin Configuration
const int DOUT_PIN = 4;
const int SCK_PIN = 5;

HX711 scale;

// 알려진 추의 무게를 Constant(상수)로 선언 (예: 500g 아령의 경우 0.5 입력)
const float KNOWN_WEIGHT = -0.7200 * 9.80665; 
long tare_offset = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("HX711 Hardcoded Auto-Calibration 시작");
  
  scale.begin(DOUT_PIN, SCK_PIN);
  
  // 1단계: Zero Balance 측정 (Tare)
  Serial.println("1단계: Load Cell을 비워주세요. (Tare 진행 중...)");
  delay(3000); 
  scale.read_average(20); // 안정화를 위한 빈 읽기 (Dummy Read)
  tare_offset = scale.read_average(20);
  scale.set_offset(tare_offset);
  Serial.println("Tare 완료!");
  
  // 2단계: Known Weight 측정 준비 및 Countdown
  Serial.print("2단계: ");
  Serial.print(KNOWN_WEIGHT);
  Serial.println(" N의 정확한 물체를 Load Cell에 올리세요.");
  Serial.println("10초 후에 자동으로 측정을 시작합니다...");
  
  for(int i = 10; i > 0; i--) {
    Serial.print(i);
    Serial.println("초 남았습니다...");
    delay(1000);
  }
  
  Serial.println("데이터 측정 중... 흔들지 마세요.");
  delay(2000); // 기계적 Vibration 안정화 시간 (Delay)
  
  // Raw Data 측정 후 Calibration Factor 계산
  long raw_data = scale.read_average(20);
  long current_value = raw_data - tare_offset;
  float calculated_factor = (float)current_value / KNOWN_WEIGHT;
  
  Serial.println("----------------------------------------");
  Serial.print("산출된 Calibration Factor: ");
  Serial.println(calculated_factor);
  Serial.println("----------------------------------------");
  Serial.println("이 값을 Main Code의 scale.set_scale(해당 값); 에 적용하세요.");
  
  // Factor 적용 후 실제 측정 모드(Loop)로 전환
  scale.set_scale(calculated_factor);
}

void loop() {
  // 지속적인 Weight 출력
  Serial.print("Current Weight: ");
  Serial.print(scale.get_units(5), 3);
  Serial.println(" kg");
  delay(1000);
}