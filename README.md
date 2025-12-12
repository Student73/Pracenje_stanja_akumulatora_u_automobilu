# **Upravljanje_klimom_u_automobilu**

## **Opis projekta** 
- Potrebno je realizovati softver za simulaciju upravljanja klimom u automobilu. 
- Klima poseduje dva rezima rada:manuelno i automatski. 
- Ukoliko je rezim rada manuelno klima se ukljucuje kada je ukljucena dioda(prekidac), a ukoliko je automatski klima se ukljucuje po potrebi.
- Automatski rezim rada funkcionise po principu histerezisa.
- Trenutna vrednost temperature se dobija usrednjavanjem vrednosti koje dobijamo ocitavanjem sa dva senzora koja se zajedno sa minimalnom ili maksimalnom vrednosti ili rezimom rada prikazuje na displeju. 
- Rezim rada klime, trenutnu vrednost temperature, da li je ukljucena ili iskljucena i jacinu duvanja ventilatora prikazujemo pomocu serijske komunikacije. 
- Pomocu led bara odredjujemo da li ce biti prikazana maksimalna ili minimalna temperatura i koja ce biti jacina duvanja ventilatora. 
- Takodje pomocu led bara prikazujemo da li je klima ukljucena.

## **Koriscene periferije**
- _AvdUniCom_ je simulator serijske komunikacije na kom smo koristili tri kanala. Kanali 0 i 1 sluze za ocitavanje trenutne vrednosti temperature. 
  Na kanalu 2 se prikazuje rezim rada klime, trenutna vrednost temperature, da li je ukljucena i jacina duvanja ventilatora.  Takodje, ovaj kanal sluzi za slanje rezima rada, histerezisa i zeljene temperature. 

- _Seg7Mux_ prikazuje trenutnu vrednost temperature i maksimalnu ili minimalnu vrednost temperature u zavisnosti koja od dioda je ukljucena, takodje prikazuje i rezim rada. 

- _LED bars plus_ poseduje dva stupca. Prvi stubac predstavlja ulaz, a drugi izlaz. Drugi stubac zavisi od ulaznog stupca, rezima rada, trenutne vrednosti temperature u automobilu, histerezisa i zeljene temperature. 

## **Pokretanje programa**
- Preuzeti repozitorijum sa Github sajta. Neophodan je instaliran Visual Studio 2019 kao okruzenje u kom se radi i PC lint kao program za staticku analizu koda.
- Otvoriti Visual Studio, kliknuti na komandu Open a local folder i otvoriti preuzeti repozitorijum. Zatim, sa desne strane kliknuti na FreeRTOS_simulator_final.sln, a onda na foldere Starter i Source Files i na fajl main.application kako bi se prikazao kod u radnom prostoru.
- U gornjem srednjem dijelu ispod Analyze podesiti x86 i pokrenuti softver pomocu opcije Local Windows Debugger koja se nalazi pored.

## **Pokretanje periferija**
- Preko Command Prompt-a uci u folder Periferije. 
- U folderu AdvUniCom pokrenuti tri puta aplikaciju AdvUniCom i kao argument proslediti po jedan broj (0,1,2) da bi se otvorio odgovarajuci kanal. 
- Zatim u folderu LEDbars pokrenuti aplikaciju LED_bars_plus i kao argument proslediti rG
- Zatim u folderu 7SegMux pokrenuti aplikaciju Seg7_Mux i kao argument proslediti broj 5
- Sada ste spremni za testiranje softvera

## **Upustvo za testiranje softvera**
- Preko COM0 i COM1 se salju vrednosti koje predstavljaju ocitavanje temperature u automobilu. 
  Vrednosti sa COM0 i COM1  usrednjavamo i dobijamo trenutnu temperaturu, minimalnu i maksimalnu vrednost temperature koje prikazujemo na displeju. 
- Preko COM2 se salje rezim rada klime, vrednost histerezisa i zeljena temperatura. Takodje u polju Reciver se ispisuje rezim rada klime, trenutna vrednost temperature, jacina duvanja ventilatora i da li je klima ukljucena. 
  Ako je rezim rada manuelno i ako je prva dioda ulaznog stupca ukljucena ukljucuje se prva dioda drugog stupca.(klima ukljucena) 
  Ako je rezim rada automatski: 
	1. Ako je trenutna vrednost veca od zbira zeljene temperature i histerezisa klima je ukljucena sa maksimalnom jacinom ventilatora;
	2. Ako je trenutna vrednost manja od razlike zeljene vrednosti temperature i histerezisa klima je iskljucena;	
        3. Ako se trenutna vrednost nalazi izmedju ove dve pomenute tada ce klima zadrzati stanje(ukljucena ili iskljucena) u kojem se nalazi.
- Podesavanje jacine duvanja ventilatora u manuelnom rezimu vrsi se pomocu dioda:Gornje cetiri ledovke predstavljaju redom jacinu 1, 2, 3 i 4 zajedno sa prvom diodom(ukljucena klima, kada je iskljucena ventilator je na 0).
- Podesavanje jacine duvanja ventilatora u automatskom rezimu:Ako vazi 1. onda je jacina 4, kada temperatura padne ispod te vrednosti jacina ce biti 2(optimalna jacina).
- Ako je ukljucena druga dioda prvog stupca, na displeju se prikazuje trenutna i maksimalna vrednost, a ukoliko su ukljucena prva i druga dioda prvog stupca na displeju se prikazuje trenutna vrednost i minimalna. 