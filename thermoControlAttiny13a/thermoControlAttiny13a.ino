#define THERMISTOR_ADC_CHANNEL 1 // PB2, physical pin 7
#define RELAY_PIN PB4            // physical pin 3
#define FAN_PIN PB3              // physical pin 2

// Divider: +5V -> thermistor -> ADC(PB2) -> 9.88k -> GND
// ADC thresholds for thermistor resistance (Vref = VCC):
// R=4.4k -> ADC ~708
// R=2.0k -> ADC ~851
#define ADC_THRESHOLD_4400R 680
#define ADC_THRESHOLD_2000R 790
static inline uint16_t adc_read(void) {
  ADCSRA |= _BV(ADSC);
  while (ADCSRA & _BV(ADSC));
  return ADC;
}

void setup(void) {
  DDRB = _BV(RELAY_PIN) | _BV(FAN_PIN);
  PORTB = 0;

  ADMUX = THERMISTOR_ADC_CHANNEL;      // Vref = VCC, ADC1 (PB2)
  ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1); // ADC on, clk/64
}

void loop(void) {
  static uint16_t filteredAdc = 0;
  static bool relayOn = false;

  uint16_t adc = adc_read();
  if (filteredAdc == 0) {
	filteredAdc = adc;
  } else {
	filteredAdc = (filteredAdc * 7u + adc) / 8u;
  }

  // Relay hysteresis:
  // ON when R < 2k (ADC > 851)
  // OFF when R > 4.4k (ADC < 708)
  if (relayOn) {
	if (filteredAdc < ADC_THRESHOLD_4400R) {
	  relayOn = false;
	}
  } else {
	if (filteredAdc > ADC_THRESHOLD_2000R) {
	  relayOn = true;
	}
  }

  // Fan control:
  // ON when R < 4.4k (ADC > 708), OFF otherwise
  bool fanOn = filteredAdc > ADC_THRESHOLD_4400R;

  if (relayOn) PORTB |= _BV(RELAY_PIN);
  else PORTB &= ~_BV(RELAY_PIN);

  if (fanOn) PORTB |= _BV(FAN_PIN);
  else PORTB &= ~_BV(FAN_PIN);

  delay(50);
}
