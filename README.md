DOCUMENTAZIONE CODICE DEL WATTMETRO 
-

```c++

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


```

SETUP
--

```c++
void setup() {
  Serial.begin(115200); 
  analogReadResolution(12);
  myTim = new HardwareTimer(TIM1);
  myTim->setOverflow(FC, HERTZ_FORMAT);
  myTim->attachInterrupt(adc);
  myTim->resume();
}
```
la funzione di setup viene è una funzione che la scheda arduino esegue ogni volta all'accenzione prima del loop. 

* Serial.begin() ci permette di selezionare il baud rate della comunicazione seriale tra scheda e computer in questo caso 115200 baud. 

* analogResolution(12) imposta la risoluzione del modulo adc a 12 bit 

* myTim = new HardwareTimer(TIM1); inizializza un nuovo oggetto di tipo timer e gli assoccia il TIM1 della scheda 

* myTim->setOverflow(FC, HERTZ_FORMAT); con questa istruzione andiamo a specificare ogni quanti secondi il timer dovrà aumentare di uno il conteggio, in questo caso glielo specifichiamo in HERTZ con FC avremmo anche potuto passargli il TC tempo di campionamento 

*  myTim->attachInterrupt(adc); ogni volta che il timer aumenterà di uno il conteggio ovvero ogni volta che passeranno TC (tempo di campionamento) secondi  la interrupt service routine adc verrà eseguita 

* myTim->resume(); fa partire il timer 

Il timer è una delle due perifiriche che abbiamo usato per la raccolta dei campioni della corrente e della tensione, l'altra è l'adc . Utilizziamo un timer per sincronizzare il campionamento e assicurarci che avvenga a una frequenza costante pari a FC, per garantire che tale frequenza sia costante utilizziamo il meccaniscmo delle interrupt che ci permette di essere più precisi in termini temporali 

ADC
---
ogni volta che quindi il timer aumenta di uno il conteggio la chiama,questa funzione è quella che quindi ci servirà a gestire l'acqusizione dei campioni 

``` c++
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

```
1. Legge i valori analogici da `voltagePin` e `currentPin` tramite `analogRead()`.
2. Applica la calibrazione moltiplicando per il guadagno e sottraendo l'offset.
3. Salva i campioni nei buffer `pReadV` e `pReadC` e incrementa la variabile `i`
4. Quando il numero di campioni raccolti raggiunge `N_SAMPLE`:
   - L'indice `i` viene azzerato per ricominciare il campionamento.
   - Se il buffer precedente (`pPrintV`) non è stato ancora elaborato, viene segnalato un overflow impostando `ovverun = 1`.
   - I puntatori ai buffer di stampa (`pPrintV`, `pPrintC`) vengono aggiornati per puntare ai buffer appena riempiti.
   - Viene effettuato il passaggio al buffer alternativo (`voltageSamples1` a `voltageSamples2`, `currentSamples1`a `currentSamples2`) per continuare il campionamento senza interruzioni.

PING PONG BUFFER
---
Come si può notare dal codice questa funzione sfrutta la tecnica del ping pong buffer:

```c++
int16_t voltageSamples1[N_SAMPLE]; 
int16_t currentSamples1[N_SAMPLE];
int16_t voltageSamples2[N_SAMPLE];
int16_t currentSamples2[N_SAMPLE];
```
questi sono gli effettivi buffer usati in questa tecnica da noi dichiarati. L'obbiettivo è utilizzarne due come buffer di scrittura nei quali andiamo a inserire i nuovi dati campionati dall'adc e due contemporaneamente come buffer di elaborazione dei dati per far si che possiamo usarli per esempio per: 

* scriverli sul monitor seriale
* effettuare la media
* calcolare la potenza istantanea 
* calcolare rms
* ecc...

Questa tecnica ci da un vantaggio fondamentale rispetto a usare lo stesso buffer per lettura e scrittura. Infatti ci da un tempo pari a N_SAMPLE * TC secondi per elaborare i dati presenti nel buffer prima che si inizi a riscrivere il buffer con i nuovi campioni.

Se ne avessimo usato uno solo infatti il problema sarebbe stato che arrivati all'acquisizione dell'utlimo campione, subito la isr `adc()` sarebbe ripartita dal primo indice del buffer a inserire un nuovo valore di campionamento dandoci un tempo di soli TC secondi per poter elaborare tutto il buffer pieno degli N_SAMPLES raccolti 

```c++
// Puntatori per la scrittura
int16_t *pReadV = voltageSamples1;
int16_t *pReadC = currentSamples1;

// Puntatori per la lettura
int16_t *pPrintV = NULL;
int16_t *pPrintC = NULL;
```
questi sono i puntatori che usiamo per la lettura e la scrittura quello che succede infatti è che alla fine di ogni acqusizione di un buffer di N_SAMPLES i vettori che vengono usati per l'acqusizione dei dati diventano quelli per l'elaborazione e anche il viceversa quelli per l'elaborazione diventano quelli per l'acqusizione come riportato in queste righe di codice della isr `adc()`:
```c++
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

```
TRIGGER()
---
Il trigger è essenziale nel nostro programma perchè le misure di rms e potenza vanno effettuate su un numero di campioni che corrispondano a un numero intero di periodi delle forme d'onda di corrente e tensione.
Questa funzione ha come parametri di ingresso un puntatore a un buffer e la sua lunghezza e come tipo di ritorno un intero. Quello che fa è , dato il buffer di tensione o corrente, ritornare il primo indice di attraversamento della soglia (TRESHOLD), il parametro NOISE_PEAK_TO_PEAK e la sua implementazione derivano dal fatto che abbiamo scelto di usare un trigger con isteresi 

```c++
int trigger(int16_t *pSamples, int len) {
  int j = 0;
  while ((j < len) && (pSamples[j] > TRESHOLD - NOISE_PEAK_TO_PEAK)) j++;
  while ((j < len) && (pSamples[j] < TRESHOLD + NOISE_PEAK_TO_PEAK)) j++;
  if (j == len) return -1;
  return j;
}
```
Gli attraversamenti che vogliamo rilevare sono quelli a pendenza positiva quindi:
* il primo ```while((j < len) && (pSamples[j] > TRESHOLD - NOISE_PEAK_TO_PEAK)) j++;``` scorre il buffer incrementando  j fin quando o `j == len` e quindi sono arrivato alla fine del buffer o se `pSamples[j] > TRESHOLD - NOISE_PEAK_TO_PEAK` ovvero se mi trovo al di sotto della soglia inferiore del trigger con isteresi 
* il secondo `while ((j < len) && (pSamples[j] < TRESHOLD + NOISE_PEAK_TO_PEAK)) j++;` scorre il buffer incrementando j fin quando o `j == len` e quindi sono arrivato alla fine del buffer o se `pSamples[j] < TRESHOLD + NOISE_PEAK_TO_PEAK` ovvero se ho attraversato la soglia superiore del trigger con isteresi 

quindi eseguiti entrambi i cicli while o sarò uscito per la condizione `j == len` e in quel caso vorrà dire che ho attraversato tutto il buffer senza aver rilevato attraversamenti e quindi `return -1` oppure se j != len vuol dire che sono prima partito da sotto la soglia inferiore e poi ho attraversato quella superiore scorrendo il buffer e quindi ho rilevato un attraversamento quindi `return j` e ne ritorno l'indice 

