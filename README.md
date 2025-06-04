DOCUMENTAZIONE CODICE DEL WATTMETRO 
-

LA CORREZIONE CHE FACEVAMO COI GAIN NON LA FACCIAMO PIU' IN FASE DI ACQUISIZIONE MA IN FASE DI MISURA DIRETTAMENTE SULLE MISURE DI RMS E POTENZE 
---

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
  myTim->setOverflow(FC * 50600/51000, HERTZ_FORMAT);
  myTim->attachInterrupt(adc);
  myTim->resume();
}
```
la funzione di setup viene è una funzione che la scheda arduino esegue ogni volta all'accensione prima del loop. 

* Serial.begin() ci permette di selezionare il baud rate della comunicazione seriale tra scheda e computer, in questo caso 115200 baud. 

* analogResolution(12) imposta la risoluzione del modulo adc a 12 bit 

* myTim = new HardwareTimer(TIM1); inizializza un nuovo oggetto di tipo timer e gli associa il TIM1 della scheda 

* myTim->setOverflow(FC, HERTZ_FORMAT); con questa istruzione andiamo a specificare ogni quanti secondi il timer dovrà aumentare di uno il conteggio, in questo caso glielo specifichiamo in HERTZ con FC avremmo anche potuto passargli il TC tempo di campionamento 

*  myTim->attachInterrupt(adc); ogni volta che il timer aumenterà di uno il conteggio ovvero ogni volta che passeranno TC (tempo di campionamento) secondi  la interrupt service routine adc verrà eseguita 

* myTim->resume(); fa partire il timer 

Il timer è una delle due periferiche che abbiamo usato per la raccolta dei campioni della corrente e della tensione, l'altra è l'adc . Utilizziamo un timer per sincronizzare il campionamento e assicurarci che avvenga a una frequenza costante pari a FC, per garantire che tale frequenza sia costante utilizziamo il meccanismo delle interrupt che ci permette di essere più precisi in termini temporali 

ADC
---
ogni volta che quindi il timer aumenta di uno il conteggio la chiama,questa funzione è quella che quindi ci servirà a gestire l'acqusizione dei campioni 

``` c++
void adc() {
  pReadV[i] = analogRead(voltagePin) - OFFSET_V;
  pReadC[i] = analogRead(currentPin) - OFFSET_C;
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
2. Applica la calibrazione sottraendo l'offset.
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

Se ne avessimo usato uno solo infatti il problema sarebbe stato che arrivati all'acquisizione dell'ultimo campione, subito la isr `adc()` sarebbe ripartita dal primo indice del buffer a inserire un nuovo valore di campionamento dandoci un tempo di soli TC secondi per poter elaborare tutto il buffer pieno degli N_SAMPLE raccolti 

```c++
// Puntatori per la scrittura
int16_t *pReadV = voltageSamples1;
int16_t *pReadC = currentSamples1;

// Puntatori per la lettura
int16_t *pPrintV = NULL;
int16_t *pPrintC = NULL;
```
questi sono i puntatori che usiamo per la lettura e la scrittura.Quello che succede infatti è che alla fine di ogni acqusizione di un buffer di N_SAMPLES i vettori che vengono usati per l'acqusizione dei dati diventano quelli per l'elaborazione e anche il viceversa quelli per l'elaborazione diventano quelli per l'acqusizione come riportato in queste righe di codice della isr `adc()`:
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

PERIOD()
---
La funzione period() è quella che sfruttando la funzione `trigger()` prende in ingresso un puntatore a un buffer `int16_t * pSamples`che contiene i campioni della funzione di tensione o corrente, un intero `int len` pari alla lunghezza del buffer e un puntatore a un intero `int *sumNpp` e ritorna un double che corrisponde al numero di campioni per peiodo e salva nella variabile passata come puntatore `sumNpp` il numero di campioni corrispondente a un numero finito di periodi della funzione di tensione o corrente passata come buffer `pSamples` alla funzione. Il numero di periodi che trova è pari a `np`
```c++
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
  double npp = (double) sum / np;
  *sumNpp = sum;
  return npp;
}
```

* `int ind = trigger(pSamples, len);` ritorna l'indice del primo evento di trigger e lo salva in ind, se non viene trovato nessun indice di trigger ind sarà uguale a -1 e quindi `return -1`, 

* dichiaro le tre variabili `np` che conterrà il numero di periodi trovati, `sum` che conterrà il numero di campioni totali corrispondenti a `np` periodi e `tmp` variabile di appoggio nella quale salvo gli indici di trigger che trovo 

* nel do while quello che faccio è:
    1. `tmp = trigger(&pSamples[ind], len - ind);` richiamare la funzione di trigger stavolta passando come puntatore al buffer lo stesso del buffer passato alla funzione period però `&pSamples[ind]` ovvero l'indirizzo dell'elemento alla posizione `ind` del buffer quella del primo evento di trigger trovato in precedenza. Inoltre come lunghezza passo `len - ind` ovvero la lunghezza del buffer di partenza - ind. Sostanzialmente richiamo la funzione trigger a partire dalla posizione ind ovvero quella dell'ultimo indice di trigger trovato.
    2. `if (tmp == -1) break;` se non trovo altri indici di trigger esco con il break dal do while, questo check rende la condizione del do while inutile in quanto se `tmp == -1` uscirei prima per questo che per il check del do while 
    3. 
        `np++; 
        sum += tmp;
        ind += tmp;`
    in caso tmp != -1 vuol dire che ho trovato un nuovo indice di trigger e quindi aumento il numero di periodi di 1 , aggiungo al numero di campioni quelli corrispondenti al nuovo trigger e all'interno di `ind` salvo il nuovo indice di trigger trovato 
* uscito dal do while `np == 0` vorrà dire che non ho trovato nuovi indici di trigger ma solo il primo quindi non ho trovato la lunghezza del periodo della funzione di conseguenza `return -1`
* in caso contrario salvo in `npp` `sum/npV` ovvero il numero di campioni per periodo e `*sumNpp = sum;` ovvero nella variabile puntata dal puntatore `sumNpp` salvo sum ovvero il numreo di campioni corrispondenti agli `np` periodi trovati e infine `return npp`

MEDIA(), RMS(), POTENZA()
---
Tutte e tre le funzioni implementano il calcolo di media, rms e potenza su un numero di campion pari a `len` parametro di ingresso di tutte e tre. I tre integrali nel tempo discreto si trasformano in semplici sommatorie 
```c++
double rms(int16_t *pSamples, int len) {
  int64_t q = 0;
  for (int j = 0; j < len; j++) {
    q += pSamples[j] * pSamples[j];
  }
  return sqrt((double)q / len);
}

double media(int16_t *pSamples, int len) {
  int64_t m = 0;
  for (int j = 0; j < len; j++) {
    m += pSamples[j];
  }
  return (double) m / len;
}

double potenza(int16_t *vSamples, int16_t *iSamples, int len) {
  int64_t sum = 0;
  for (int j = 0; j < len; j++) {
    sum += vSamples[j] * iSamples[j];
  }
  return (double)sum / len;
}
```

Tre funzioni molto simili:
* `rms()` prende in ingeresso un puntatore a un buffer di interi `pSamples` che corrisponderà al buffer contenente i campioni di tensione e corrente sui quali effettuare la misura di rms e un parametro di ingresso `int len` che corrisponde al numero di campioni sul quale effettuare la misura
    1. dichiaro una variabile di appoggio `q` = 0
    2. for che come da calcolo di rms scorre all'interno del buffer e somma alla variabile `q` il valore al quadrato degli elementi del buffer per poi salvarlo all'interno di `q`
    3. ritorno il valore di `q` diviso `len` e sotto radice 

* `media()` prende in ingeresso un puntatore a un buffer di interi `pSamples` che corrisponderà al buffer contenente i campioni di tensione o corrente sui quali effettuare la misura di media e un parametro di ingresso `int len` che corrisponde al numero di campioni sul quale effettuare la misura
    1. dichiaro una variabile di appoggio `m` = 0
    2. for che come da calcolo di media scorre all'interno del buffer e somma alla variabile `m` il valore degli elementi del buffer per poi salvarlo all'interno di `m`
    3. ritorno il valore di `m` diviso `len` 
* `potenza()` prende in ingeresso un puntatore a un buffer di interi `vSamples` che corrisponderà al buffer contenente i campioni di tensione e  un puntatore a un buffer di interi `iSamples` buffer per la corrente sui quali effettuare la misura di potenza e un parametro di ingresso `int len` che corrisponde al numero di campioni 
    1. dichiaro una variabile di appoggio `sum` = 0
    2. for che come da calcolo di potenza scorre all'interno del buffer e somma alla variabile `sum` il valore del prodotto degli elementi dei buffer per poi salvarlo all'interno di `sum`
    3. ritorno il valore di `sum` diviso `len` 

**ATTENZIONE TUTTE E TRE QUESTE FUNZIONI FANNO I CALCOLI SUI VALORI SALVATI NEI BUFFER DI TENSIONE E CORRENTE CHE ACQUISIAMO DALL'ADC QUINDI NUMERI INTERI CHE VANNO DA 0 A 4095 NON SU VALORI DI TENSIONE E CORRENTE EFFETTIVI. QUINDI DOPO AVERLE CALCOLATE QUESTE TRE MISURE VANNO OPPORTUNAMENTE CONVERTITe IN ANALOGICO COME MISURE**

LOOP()
---
```c++
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

    double Vrms = (rms(pPrintV, measuringSamples)) * 3.3 / 4095 * GAIN_V;
    double Irms = (rms(pPrintC, measuringSamples)) * 3.3 / 4095 * GAIN_I;
    double P = (potenza(pPrintV, pPrintC, measuringSamples)) * 3.3 / 4095 * 3.3 / 4095 * GAIN_V * GAIN_I;

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
```

* la variabile `mesauringSamples` è quella corrispondente al numero di campioni sui quali effettuare le misure all'inzio viene inizializzata alla costante `N_SAMPLES` 

*  `if ((pPrintV != NULL) && (pPrintC != NULL))` serve a verificare che i due puntatori di elaborazione dei dati non siano null infatti vengono inizializzati come null e poi quando viene riempito per la prima volta il buffer in acquiszione dei campioni vengono cambiati. Quindi acquisito il primo buffer di campioni la condizione di questo if è soddisfatta e passo a printare i campioni e a misurare 

* `double nppV = period(pPrintV, N_SAMPLE, &measuringSamples);`
   
    `if (nppV == -1) {`

      Serial.print("Out of sync measuremnt");
      nppV = FC / 50;
    `}`

    richiamo la funzione di period sul buffer della tensione in modo da salvare in `measuringSamples` il numero di campioni corrispondente a `nppV` periodi e in caso la funzione `period()` ritorni -1 in caso di errore printo un messaggio per far capire che non ho trovato la misura del periodo di `pPrintV` e `measuringSamples` rimarrà di base uguale a `N_SAMPLE` e come frequenza base assumerò `50`

* Avendo adesso in measuringSamples il numero di campioni corrispondente a un numero intero di periodi in caso period non abbia ritorna -1 quello che rimane da fare e richiamare le funzioni di misura implementate con len = measuringSamples

OFFSET E GAIN
---

Le costanti OFFSET_V/C E GAIN_V/C sono state calcolate rispettivamente facendo la media dei segnali in ingresso all'arduino dai circuiti di condizionamento e confrontando le rms dei segnali in ingresso all'arduino e all'uscita. Essendo caratteristiche statiche dei circuti sono inserite come costanti all'interno del programma e fanno la correzione dei campioni in ingresso alla scheda arduino 
