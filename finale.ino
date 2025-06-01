#include <HardwareTimer.h>
#include <stdint.h>
#include <math.h>

#define F 50 // frequenza assunta dei segnali di i e v assunta standard senza usare il trigger
#define FC 1000 // frequenza di campionamento
#define TC (1 / FC) // tempo di campionamento 
#define N_SAMPLE (10 * FC / F) // numero di campioni raccolti 
#define OFFSET_C 2115 // offset del circuito di condizionamento della corrente 
#define OFFSET_V 1685 // del circuito di condizionamento della tensione
#define GAIN_C 5.08 // guadagno del circuito di condizionamento della corrente 
#define GAIN_V 3.77 // guadagno del circuito di condizionamento della tensione
#define TRESHOLD 0 // valore di treshold del trigger 
#define NOISE_PEAK_TO_PEAK 100 // valore di tensione del rumore picco-picco serve al trigger per isteresi 

int voltagePin = A0; // pin analogico usato per la tensione 
int currentPin = A1; // pin analogico usato per la corrente 
HardwareTimer *myTim; // timer usato per la interrupt che ci permette di campionare a una frequenza pari a FC

// array usati per il ping pong buffer 
int16_t voltageSamples1[N_SAMPLE]; 
int16_t currentSamples1[N_SAMPLE];
int16_t voltageSamples2[N_SAMPLE];
int16_t currentSamples2[N_SAMPLE];

// Puntatori per la scrittura
int16_t *pReadV = voltageSamples1;
int16_t *pReadC = currentSamples1;

// Puntatori per la lettura
int16_t *pPrintV = NULL;
int16_t *pPrintC = NULL;

int ovverun = 0; // ovverun usato in caso non riesca a consumare in tempo i dati del buffer di lettura 
int i = 0; // i usato per scorrere il vettore in fase di scrittura 


void adc() {
  pReadV[i] = GAIN_V * ((double)analogRead(voltagePin) - OFFSET_V);
  pReadC[i] = GAIN_C * ((double)analogRead(currentPin) - OFFSET_C);
  i++;
  if (i >= N_SAMPLE) {
    i = 0;
    if (pPrintV != NULL) {
      ovverun = 1;
    }
    pPrintV = pReadV;
    pPrintC = pReadC;
    if (pReadV == voltageSamples2) {
      pReadV = voltageSamples1;
      pReadC = currentSamples1;
    } else {
      pReadV = voltageSamples2;
      pReadC = currentSamples2;
    }
  }
}

int trigger(int16_t *pSamples, int len) {
  int j = 0;
  while ((j < len) && (pSamples[j] > TRESHOLD - NOISE_PEAK_TO_PEAK)) j++;
  while ((j < len) && (pSamples[j] < TRESHOLD + NOISE_PEAK_TO_PEAK)) j++;
  if (j == len) return -1;
  return j;
}

int periodNostro(int16_t *pSamples, int len) {
  int primo = trigger(pSamples, len);
  if (primo == 1) return -1;
  int secondo = trigger(&pSamples[primo], len - primo);
  if (secondo == 1) return -1;
  return secondo;
}

double period(int16_t *pSamples, int len, int *sumNpp) {
  int ind = trigger(pSamples, len);
  if (ind == -1) {
    return -1;
  }

  int np = 0;
  int sum = 0;
  int tmp;
  do {
    tmp = trigger(&pSamples[ind], len - ind);
    if (tmp == -1) break;
    np++;
    sum += tmp;
    ind += tmp;
  } while (tmp != -1);

  if (np == 0) return -1;
  double npp = sum / np;
  *sumNpp = sum;
  return npp;
}

double rms(int16_t *pSamples, int len) {
  double q = 0;
  for (int j = 0; j < len; j++) {
    q += pSamples[j] * pSamples[j];
  }
  return sqrt(q / len);
}

double media(int16_t *pSamples, int len) {
  double m = 0;
  for (int j = 0; j < len; j++) {
    m += pSamples[j];
  }
  return m / len;
}

double potenza(int16_t *vSamples, int16_t *iSamples, int len) {
  int sum = 0;
  for (int j = 0; j < len; j++) {
    sum += vSamples[j] * iSamples[j];
  }
  return (float)sum / len;
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  myTim = new HardwareTimer(TIM1);
  myTim->setOverflow(FC, HERTZ_FORMAT);
  myTim->attachInterrupt(adc);
  myTim->resume();
}

void loop() {
  int measuringSamples = N_SAMPLE;
  if ((pPrintV != NULL) && (pPrintC != NULL)) {
    for (int j = 0; j < N_SAMPLE; j++) {
      Serial.print("V:");
      Serial.print(pPrintV[j]);
      Serial.print(" ");
      Serial.print("I:");
      Serial.print(pPrintC[j]);
      Serial.println(" ");
    }

    double nppV = period(pPrintV, N_SAMPLE, &measuringSamples);
    if (nppV == -1) {
      Serial.print("Out of sync measuremnt");
      nppV = FC / 50;
    }

    double Vrms = (rms(pPrintV, measuringSamples)) * 3.3 / 4095;
    double Irms = (rms(pPrintC, measuringSamples)) * 3.3 / 4095;
    double P = (potenza(pPrintV, pPrintC, measuringSamples)) * 3.3 / 4095 * 3.3 / 4095;

    Serial.println("NPP:");
    Serial.print(nppV);
    Serial.println(" ");

    pPrintV = NULL;
    pPrintC = NULL;

    Serial.print("Vrms:");
    Serial.print(Vrms);
    Serial.print(" ");
    Serial.print("Irms:");
    Serial.print(Irms);
    Serial.println(" ");
  }
  delay(50);
}
