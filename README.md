# **Pracenje_stanja_akumulatora_u_automobilu**

## **Opis projekta** 
- Potrebno je realizovati softver za simulaciju pracenja stanja akumulatora u automobilu. 
- Akumulator posjeduje dva rezima punjenja: kontinualno i kontrolisano. 
- Ukoliko je rezim punjenja kontinualno akumulator se puni kada je ukljucen automobil, a ukoliko je kontrolisano akumulator se puni po potrebi.
- Trenutna vrijednost akumulatora se dobija usrednjavanjem 20 posljednjih vrijednosti ocitavanja koja se zajedno sa minimalnom i maksimalnom vrijednosti prikazuje na displeju. 
- Rezim punjenja akumulatora i trenutnu vrijednost prikazujemo pomocu serijske komunikacije. 
- Slanjem Admin12.5 dobijamo minimalnu vrijednost napona, a slanjem Admax14.5 maksimalnu. 
- Pomocu led bara prikazujemo punjenje akumulatora paljenjem odredjene diode.

## **Koriscene periferije**
- _AvdUniCom_ je simulator serijske komunikacije na kom smo koristili dva kanala. Kanal 0 sluzi za ocitavanje trenutne vrijednosti napona akumulatora. 
  Na kanalu 1 se prikazuje rezim punjenja akumulatora i trenutna vrijednost napona.  Takodje, ovaj kanal sluzi za slanje rezima punjenja i minimalne i maksimalne vrijednosti. 

- _Seg7Mux_ prikazuje trenutnu vrijednost napona i maksimalnu ili minimalnu vrijednost u zavisnosti koja od dioda je ukljucena. 

- _LED bars plus_ posjeduje tri stupca. Prvi stubac predstavlja ulaz, a druga dva izlaze. Drugi stubac zavisi od ulaznog stupca kao i rezima punjenja i trenutne vrijednosti napona akumulatora. Treci stubac se ukljucuje kada je trenutni napon ispod 10 volti. 

## **Pokretanje programa**
- Preuzeti repozitorijum sa Github sajta. Neophodan je instaliran Visual Studio 2019 kao okruzenje u kom se radi i PC lint kao program za staticku analizu koda.
- Otvoriti Visual Studio, kliknuti na komandu Open a local folder i otvoriti preuzeti repozitorijum. Zatim, sa desne strane kliknuti na FreeRTOS_simulator_final.sln, a onda na foldere Starter i Source Files i na fajl main.application kako bi se prikazao kod u radnom prostoru.
- U gornjem srednjem dijelu ispod Analyze podesiti x86 i pokrenuti softver pomocu opcije Local Windows Debugger koja se nalazi pored.

## **Pokretanje periferija**
- Preko Command Prompt-a uci u folder Periferije. 
- U folderu AdvUniCom pokrenuti dva puta aplikaciju AdvUniCom i kao argument proslijediti po jedan broj (0,1) da bi se otvorio odgovarajuci kanal. 
- Zatim u folderu LEDbars pokrenuti aplikaciju LED_bars_plus i kao argument proslijediti rGB
- Zatim u folderu 7SegMux pokrenuti aplikaciju Seg7_Mux i kao argument proslijediti broj 5
- Sada ste spremni za testiranje softvera

## **Upustvo za testiranje softvera**
- Preko COM0 se salje 30 vrijednosti koje predstavljaju ocitavanje napona akumulatora. 
  Dvadeset posljednjih ocitavanja usrednjavamo i dobijamo trenutnu, minimalnu i maksimalnu vrijednost napona koje prikazujemo na displeju. 
- Preko COM1 se salje rezim punjenja akumulatora, kao i Admin i Admax. Takodje u polju Reciver se ispisuje rezim punjenja i trenutna vrijednost napona. 
  Ako je rezim punjenja kontrolisano i ako je prva dioda ulaznog stupca ukljucena ukljucuje se cetvrta dioda drugog stupca. 
  Ako je rezim punjenja kontinualno i ako je vrijednost: 
	1. ispod 12,5 V ukljuci se treca dioda drugog stupca;
	2. iznad 13.5 V ukljuci se cetvrta dioda drugog stupca;	
- Ako je napon akumulatora porastao preko 14V, iskljuci punjac.
- Ukoliko je napon akumulatora pao ispod 10V, ukljuci se treci stubac LED bara da blinka. 
- Ako je ukljucena treca dioda prvog stupca, na displeju se prikazuje trenutna i minimalna vrijednost, a ukoliko je ukljucena prva dioda prvog stupca na displeju se prikazuje trenutna vrijednost i maksimalna. 