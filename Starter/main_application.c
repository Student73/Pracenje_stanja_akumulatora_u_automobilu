// STANDARD INCLUDES
#include <stdio.h>l.g 
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// KERNEL INCLUDES
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "extint.h"

// HARDWARE SIMULATOR UTILITY FUNCTIONS  
#include "HW_access.h"

// SERIAL SIMULATOR CHANNEL TO USE 
#define COM_CH_0 (0)
#define COM_CH_1 (1)
#define COM_CH_2 (2)

#define led_klima_aktivna 0x01

// TASK PRIORITIES 
#define	TASK_SERIAL_SEND_PRI		( tskIDLE_PRIORITY + 3 )
#define TASK_SERIAl_REC_PRI			( tskIDLE_PRIORITY + 4 )
#define	SERVICE_TASK_PRI			( tskIDLE_PRIORITY + 2 )
#define TASK_SERIAl_KANAL0			( tskIDLE_PRIORITY + 5 )
#define obrada		                ( tskIDLE_PRIORITY + 1 )

typedef double MyDouble;
typedef int MyInt;
static uint32_t volatile t_point; 
const char trigger[] = "XYZ"; 
static uint32_t volatile r_point; 
const char trigger1[] = "PT"; 
// TASKS: FORWARD DECLARATIONS 
void LEDBar_Task(void* pvParameters);

void LCD_Displej(void* pvParameters);

void SerialReceive_Task(void* pvParameters);
void SerialReceive_kanal0(void* pvParameters);
void SerialReceive_kanal1(void* pvParameters);
void obrada_sa_PC(void* pvParameters);

void racunanje_temperature_sa_kanala(void* pvParams);

void PC_Ispis(void* pvParameters);
void main_demo(void);

// TRASNMISSION DATA - CONSTANT IN THIS APPLICATION 
static uint32_t volatile t_point;


// 7-SEG NUMBER DATABASE - ALL HEX DIGITS [ 0 1 2 3 4 5 6 7 8 9 A B C D E F ]
static const char hexnum[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 };


// GLOBAL OS-HANDLES 
static SemaphoreHandle_t LED_INT_BinarySemaphore;
static SemaphoreHandle_t TBE_BinarySemaphore;
static SemaphoreHandle_t RXC_kanal1_BinarySemaphore;
static SemaphoreHandle_t RXC_kanal0_BinarySemaphore;
static SemaphoreHandle_t RXC_kanal2_BinarySemaphore;
static SemaphoreHandle_t displej;

static QueueHandle_t LEDBar_Queue;
static QueueHandle_t red_kanal0;
static QueueHandle_t red_kanal1;
static QueueHandle_t red_kanal2;
static QueueHandle_t stanje_klime;
static QueueHandle_t temperatura;
static QueueHandle_t histerezis;
static QueueHandle_t trenutna;
static QueueHandle_t maksimalna_vrednost;
static QueueHandle_t minimalna_vrednost;
static QueueHandle_t trenutna_displej;
static QueueHandle_t rec_displej;
static QueueHandle_t rec_serijska;
static QueueHandle_t trenutna_serijska;
static QueueHandle_t ventilator;

static TimerHandle_t tajmer;
static TimerHandle_t tajmer_prijem;///////
static TimerHandle_t tajmer_prijem1; ///////
// INTERRUPTS //
static uint32_t OnLED_ChangeInterrupt(void) {	// OPC - ON INPUT CHANGE - INTERRUPT HANDLER 
	BaseType_t xHigherPTW = pdFALSE;

	xSemaphoreGiveFromISR(LED_INT_BinarySemaphore, &xHigherPTW);
	portYIELD_FROM_ISR(xHigherPTW);
}

static uint32_t prvProcessTBEInterrupt(void) {	// TBE - TRANSMISSION BUFFER EMPTY - INTERRUPT HANDLER 
	BaseType_t xHigherPTW = pdFALSE;

	xSemaphoreGiveFromISR(TBE_BinarySemaphore, &xHigherPTW);

	portYIELD_FROM_ISR(xHigherPTW);
}
static void prvProcessRXCInterrupt(void) {
	BaseType_t xHigherPTW = pdFALSE;

	// Kanal0
	if (get_RXC_status(0) != 0) {
		xSemaphoreGiveFromISR(RXC_kanal0_BinarySemaphore, &xHigherPTW);
	}

	// Kanal1
	if (get_RXC_status(1) != 0) {
		xSemaphoreGiveFromISR(RXC_kanal1_BinarySemaphore, &xHigherPTW);
	}

	//Kanal2
	if (get_RXC_status(2) != 0) {
		xSemaphoreGiveFromISR(RXC_kanal2_BinarySemaphore, &xHigherPTW);
	}

	portYIELD_FROM_ISR(xHigherPTW);

}

/* PERIODIC TIMER CALLBACK */

static void TimerCallback(TimerHandle_t tajmer)
{
	xSemaphoreGive(displej);
}

static void TimerCallback_prijem(TimerHandle_t tajmer_prijem)
{
	if (t_point > (sizeof(trigger) - 1))
		t_point = 0;
	send_serial_character(COM_CH_0, trigger[t_point++]);
	xSemaphoreGive(RXC_kanal0_BinarySemaphore);

}
static void TimerCallback_prijem1(TimerHandle_t tajmer_prijem1)
{
	if (r_point > (sizeof(trigger1) - 1))
		r_point = 0;
	send_serial_character(COM_CH_1, trigger1[r_point++]);
	xSemaphoreGive(RXC_kanal1_BinarySemaphore);

}

// MAIN - SYSTEM STARTUP POINT 
 void main_demo(void) {
	// INITIALIZATION OF THE PERIPHERALS
	init_7seg_comm();
	init_LED_comm();
	init_serial_uplink(COM_CH_1);		// inicijalizacija serijske TX na kanalu 1
	init_serial_downlink(COM_CH_1);	// inicijalizacija serijske RX na kanalu 1
	init_serial_uplink(COM_CH_0);		// inicijalizacija serijske TX na kanalu 1
	init_serial_downlink(COM_CH_0);
	init_serial_uplink(COM_CH_2);		
	init_serial_downlink(COM_CH_2);

	// INTERRUPT HANDLERS
	vPortSetInterruptHandler(portINTERRUPT_SRL_OIC, OnLED_ChangeInterrupt);		// ON INPUT CHANGE INTERRUPT HANDLER 
	vPortSetInterruptHandler(portINTERRUPT_SRL_TBE, prvProcessTBEInterrupt);	// SERIAL TRANSMITT INTERRUPT HANDLER 
	vPortSetInterruptHandler(portINTERRUPT_SRL_RXC, prvProcessRXCInterrupt);	// SERIAL RECEPTION INTERRUPT HANDLER 

	// BINARY SEMAPHORES
	LED_INT_BinarySemaphore = xSemaphoreCreateBinary();	// CREATE LED INTERRUPT SEMAPHORE 
	TBE_BinarySemaphore = xSemaphoreCreateBinary();		// CREATE TBE SEMAPHORE - SERIAL TRANSMIT COMM 
	RXC_kanal1_BinarySemaphore = xSemaphoreCreateBinary();		// CREATE RXC SEMAPHORE - SERIAL RECEIVE COMM
	RXC_kanal0_BinarySemaphore = xSemaphoreCreateBinary();
	RXC_kanal2_BinarySemaphore = xSemaphoreCreateBinary();
	displej = xSemaphoreCreateBinary();

	// QUEUES
	LEDBar_Queue = xQueueCreate(1, sizeof(uint8_t));
	red_kanal0 = xQueueCreate(1, sizeof(uint8_t));
	red_kanal1 = xQueueCreate(1, sizeof(uint8_t));
	red_kanal2 = xQueueCreate(1, sizeof(uint8_t[20]));
	temperatura = xQueueCreate(1, sizeof(MyDouble));
	histerezis = xQueueCreate(1, sizeof(MyDouble));
	maksimalna_vrednost = xQueueCreate(1, sizeof(uint8_t));
	minimalna_vrednost = xQueueCreate(1, sizeof(uint8_t));
	trenutna = xQueueCreate(1, sizeof(MyDouble));
	trenutna_displej= xQueueCreate(1, sizeof(MyDouble));
	rec_displej= xQueueCreate(1, sizeof(uint8_t));
	rec_serijska = xQueueCreate(1, sizeof(uint8_t));
	trenutna_serijska = xQueueCreate(1, sizeof(MyDouble));
	stanje_klime = xQueueCreate(1, sizeof(uint8_t));
	ventilator= xQueueCreate(1, sizeof(uint8_t));

	// TASKS 
	xTaskCreate(SerialReceive_Task, "SRx", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAl_REC_PRI, NULL);	// SERIAL RECEIVER TASK 

	xTaskCreate(LCD_Displej, NULL, configMINIMAL_STACK_SIZE, NULL, SERVICE_TASK_PRI, NULL);
	xTaskCreate(LEDBar_Task, "ST", configMINIMAL_STACK_SIZE, NULL, SERVICE_TASK_PRI, NULL);	// CREATE LED BAR TASK  

	xTaskCreate(SerialReceive_kanal0, "SRx", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAl_REC_PRI, NULL);
	xTaskCreate(SerialReceive_kanal1, "SRx", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAl_REC_PRI, NULL);
	xTaskCreate(obrada_sa_PC, NULL, configMINIMAL_STACK_SIZE, NULL, obrada, NULL);
	xTaskCreate(racunanje_temperature_sa_kanala, NULL, configMINIMAL_STACK_SIZE, NULL, TASK_SERIAl_KANAL0, NULL);
	xTaskCreate(PC_Ispis, NULL, configMINIMAL_STACK_SIZE, NULL, TASK_SERIAl_KANAL0, NULL);
	

	tajmer = xTimerCreate("Timer", pdMS_TO_TICKS(250), pdTRUE, NULL, TimerCallback);
	xTimerStart(tajmer, 0);
	tajmer_prijem = xTimerCreate("Timer", pdMS_TO_TICKS(200), pdTRUE, NULL, TimerCallback_prijem);
	xTimerStart(tajmer_prijem, 0);
	tajmer_prijem1 = xTimerCreate("Timer", pdMS_TO_TICKS(200), pdTRUE, NULL, TimerCallback_prijem1);
	xTimerStart(tajmer_prijem1, 0);
	// START SCHEDULER
	vTaskStartScheduler();
	for (;;);
}
 

// TASKS: IMPLEMENTATIONS
void LEDBar_Task(void* pvParameters)
{
	uint8_t rec = 0;
	uint8_t dioda = 0;
	uint8_t broj = 0;
	MyDouble temp= 0;
	MyDouble hiz = 0;
	MyDouble trenutna_vr = 0;
	MyDouble klima_on = 0;
	MyDouble klima_off = 0;
	uint8_t stanje = 0;
	for (;;)
	{
		
		get_LED_BAR(0, &dioda);
		xQueueReceive(LEDBar_Queue, &rec, pdMS_TO_TICKS(20));
		xQueueReceive(trenutna, &trenutna_vr, pdMS_TO_TICKS(20));
		xQueueReceive(temperatura, &temp, pdMS_TO_TICKS(20));
		xQueueReceive(histerezis, &hiz, pdMS_TO_TICKS(20));

		if (rec == 2 && dioda == 0x01)
		{

			set_LED_BAR(1, led_klima_aktivna);
			stanje = 1;
			broj = 2;

		}
		else
		{
			set_LED_BAR(1, 0x00);
			stanje = 0;
			broj = 0;
		}
		
		if (rec == 1)
		{
			klima_on = temp + hiz;
			klima_off = temp - hiz;

			if (trenutna_vr >= klima_on && stanje==0)
			{

				set_LED_BAR(1, led_klima_aktivna);
				broj = 4;
				stanje = 1;

			}
			else if (trenutna_vr <= klima_off && stanje==1)
			{
				set_LED_BAR(1, 0x00);
				broj = 0;
				stanje = 0;
			}

			else 
			{
				if (stanje == 1)
				{
					set_LED_BAR(1, led_klima_aktivna);
					broj = 2;
					
				}
				else
				{
					set_LED_BAR(1, 0x00);
					broj = 0;
				}
			}	
		}

		if (dioda == 0x81 && rec==2)
		{
			
			set_LED_BAR(1, led_klima_aktivna);
			broj = 1;
			stanje = 1;
			
		}
		else if (dioda == 0x41 && rec==2)
		{
			
			set_LED_BAR(1, led_klima_aktivna);
			broj = 2;
			stanje = 1;
			
		}
		else if (dioda == 0x21 && rec==2)
		{
			
			set_LED_BAR(1, led_klima_aktivna);
			broj = 3;
			stanje = 1;
			
		}
		else if (dioda == 0x11 && rec==2)
		{
			
			set_LED_BAR(1, led_klima_aktivna);
			broj = 4;
			stanje = 1;
		}
		   
		xQueueSend(stanje_klime, &stanje, 0U);
		xQueueSend(ventilator, &broj, 0U);

	}
}

void LCD_Displej(void* pvParameters) {

	uint8_t dioda = 0;
	uint8_t pomocna = 0;
	uint8_t max = 0;
	uint8_t min = 255;
	MyDouble Trenutna = 0;
	uint8_t rec = 0;

	for (;;) {
		get_LED_BAR(0, &dioda);
		xSemaphoreTake(displej, portMAX_DELAY);

		if (xQueueReceive(trenutna_displej, &Trenutna, pdMS_TO_TICKS(20)) == pdPASS)
		{
			pomocna = (uint8_t)Trenutna / 10;
			select_7seg_digit(0);
			set_7seg_digit(hexnum[pomocna]);

			pomocna = (uint8_t)Trenutna % 10;
			select_7seg_digit(1);
			set_7seg_digit(hexnum[pomocna]);
		}

		select_7seg_digit(2);
		set_7seg_digit(0x40);

		if (xQueueReceive(rec_displej, &rec, pdMS_TO_TICKS(20)) == pdPASS)
		{

			if (rec == 2)
			{

				select_7seg_digit(3);
				set_7seg_digit(hexnum[0]);
				select_7seg_digit(4);
				set_7seg_digit(hexnum[1]);
			}
			else if (rec == 1)
			{

				select_7seg_digit(3);
				set_7seg_digit(hexnum[0]);
				select_7seg_digit(4);
				set_7seg_digit(hexnum[0]);

			}
		}

		if (xQueueReceive(maksimalna_vrednost, &max, pdMS_TO_TICKS(20)) == pdPASS)
		{
			if (dioda == 0x02)
			{
				pomocna = max / 10;
				select_7seg_digit(3);
				set_7seg_digit(hexnum[pomocna]);

				pomocna = max % 10;
				select_7seg_digit(4);
				set_7seg_digit(hexnum[pomocna]);
			}
		}
		if (xQueueReceive(minimalna_vrednost, &min, pdMS_TO_TICKS(20)) == pdPASS)
		{
			if (dioda == 0x03)
			{
				pomocna = min / 10;
				select_7seg_digit(3);
				set_7seg_digit(hexnum[pomocna]);

				pomocna = min % 10;
				select_7seg_digit(4);
				set_7seg_digit(hexnum[pomocna]);
			}

		}


	}

}


void PC_Ispis(void* pvParameters) {

	char manuelno_niz[] = "Rezim rada klime: MANUELNO\r\n";
	char automatski_niz[] = "Rezim rada klime: AUTOMATSKI\r\n";
	char stanje_klime1[] = "\r\nKlima: UKLJUCENA\r\n";
	char stanje_klime0[] = "\r\nKlima: ISKLJUCENA\r\n";
	char poruka[37];
	char poruka1[20];
	uint8_t rec = 0;
	uint8_t stanje = 0;
	uint8_t broj = 0;
	MyDouble temperatura = 0;

	MyInt i = 0;
    TickType_t lastWakeTime = xTaskGetTickCount();
	for (;;) {
		
		if (xQueueReceive(rec_serijska, &rec, pdMS_TO_TICKS(20)) == pdPASS) {
			if (rec == 1) {
				i = 0;
				send_serial_character(COM_CH_2, '\n');
				while (i < sizeof(automatski_niz) - 1) {
					vTaskDelay(pdMS_TO_TICKS(100));
					send_serial_character(COM_CH_2, automatski_niz[i++]);
				}
			}
			else if (rec == 2) {
				i = 0;
				send_serial_character(COM_CH_2, '\n');
				while (i < sizeof(manuelno_niz) - 1) {
					vTaskDelay(pdMS_TO_TICKS(100));
					send_serial_character(COM_CH_2, manuelno_niz[i++]);
				}
			}
		}

       
		// Primanje vrednosti temperature iz reda
		if (xQueueReceive(trenutna_serijska, &temperatura, pdMS_TO_TICKS(20)) == pdPASS) {
			// Formatirajte celu poruku u jedan string
			snprintf(poruka, sizeof(poruka), "Trenutna vrednost temperature: %f\r\n", temperatura);

			// Slanje kompletne poruke
			i = 0;
			while (i < strlen(poruka)) {
				vTaskDelay(pdMS_TO_TICKS(100));
				send_serial_character(COM_CH_2, poruka[i++]);
			}
			
		}

		
		if (xQueueReceive(stanje_klime, &stanje, pdMS_TO_TICKS(20)) == pdPASS)
		{
			
			if (stanje == 1) {
				i = 0;
				send_serial_character(COM_CH_2, '\n');
				while (i < sizeof(stanje_klime1) - 1) {
					vTaskDelay(pdMS_TO_TICKS(100));
					send_serial_character(COM_CH_2, stanje_klime1[i++]);
				}
			}
			else if (stanje == 0) {
				i = 0;
				send_serial_character(COM_CH_2, '\n');
				while (i < sizeof(stanje_klime0) - 1) {
					vTaskDelay(pdMS_TO_TICKS(100));
					send_serial_character(COM_CH_2, stanje_klime0[i++]);
				}
			}

		}

		if (xQueueReceive(ventilator, &broj, pdMS_TO_TICKS(20)) == pdPASS) {
			// Formatirajte celu poruku u jedan string
			snprintf(poruka1, sizeof(poruka1), "VENT %u\r\n", broj);

			// Slanje kompletne poruke
			i = 0;
			while (i < strlen(poruka1)) {
				vTaskDelay(pdMS_TO_TICKS(100));
				send_serial_character(COM_CH_2, poruka1[i++]);
			}

		}
		vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(1000));
	}
}

void SerialReceive_Task(void* pvParameters) 
{
	uint8_t cc = 0;
	uint8_t rec[13] = { 0 };
	uint8_t c_point = 0;
	for (;;) {
		xSemaphoreTake(RXC_kanal2_BinarySemaphore, portMAX_DELAY); // čeka na serijski prijemni interapt
		get_serial_character(COM_CH_2, &cc); // učitava primljeni karakter u promenjivu cc

		if (cc == 0x0d) { // za svaki KRAJ poruke, prikazati primljene bajtove direktno na displeju 3-4
			rec[c_point] = '\0'; // dodajte nulti karakter na kraj stringa
			printf("Primio: %s\n\r", rec);
			xQueueSend(red_kanal2, &rec, 0U);
			c_point = 0;
			memset(rec, 0, sizeof(rec)); // resetujte buffer posle slanja poruke
		}
		else if (c_point < sizeof(rec) - 1) { // pamti karaktere između 0x0D
			rec[c_point++] = cc;
		}
	}
}

void obrada_sa_PC(void* pvParameters)
{
	char prijem_rec[13] = { 0 };
	uint8_t rec = 0;
	MyDouble histerezis1 = 0;
	MyDouble temperatura1 = 0;
	for (;;)
	{
		xQueueReceive(red_kanal2, &prijem_rec, portMAX_DELAY);

		// Upoređivanje stringa
		if (strcmp(prijem_rec, "AUTOMATSKI") == 0)
		{
			printf("OK,rezim je: %s\n\r",prijem_rec);
			rec = 1;
			xQueueSend(LEDBar_Queue, &rec, 0U);
			xQueueSend(rec_displej, &rec, 0U);
			xQueueSend(rec_serijska, &rec, 0U);
		}
		else if (strcmp(prijem_rec, "MANUELNO") == 0)
		{
			printf("OK,rezim je: %s\n\r", prijem_rec);
			rec = 2;
			xQueueSend(LEDBar_Queue, &rec, 0U);
			xQueueSend(rec_displej, &rec, 0U);
			xQueueSend(rec_serijska, &rec, 0U);
		}
		else if (isdigit(prijem_rec[0]))
		{
			float vrednost = atof(prijem_rec);
             
			if (vrednost >= 0.1 && vrednost <= 5)
			{
				histerezis1 =(MyDouble)vrednost;
				xQueueSend(LEDBar_Queue, &rec, 0U);
				xQueueSend(histerezis, &histerezis1, 0U);
			}
			else
			{
				temperatura1 = (MyDouble)vrednost;

				xQueueSend(LEDBar_Queue, &rec, 0U);
				xQueueSend(temperatura, &temperatura1, 0U);
			}

		}
	}
}

void SerialReceive_kanal0(void* pvParameters)
{
	uint8_t c;
	char temp[10] = { 0 };
	uint8_t broj_karaktera = 0;
	uint8_t vrednost_temperature = 0;
	
	for (;;) 
	{    
		xSemaphoreTake(RXC_kanal0_BinarySemaphore, portMAX_DELAY);

		if (get_serial_character(COM_CH_0, &c) == 0) 
		{
			if (c == 0x0d) 
			{
				temp[broj_karaktera++] = '\0';
				vrednost_temperature = atoi(temp);
				printf("Senzor0:%u\r\n",vrednost_temperature);
				xQueueSend(red_kanal0, &vrednost_temperature, 0U);


				broj_karaktera = 0;
				memset(temp, 0, sizeof(temp));
			}
			// kraj broja: CR ili LF
			else if (broj_karaktera < sizeof(temp) - 1)
			{
				temp[broj_karaktera++] = c;
			}
		}
		
	}
}

void SerialReceive_kanal1(void* pvParameters)
{
	uint8_t c;
	char temp[10] = { 0 };
	uint8_t broj_karaktera = 0;
	uint8_t vrednost_temperature = 0;
	
	for (;;)
	{
		xSemaphoreTake(RXC_kanal1_BinarySemaphore, portMAX_DELAY);

		if (get_serial_character(COM_CH_1, &c) == 0)
		{
			if (c == 0x0d)
			{
				temp[broj_karaktera++] = '\0';
				vrednost_temperature = atoi(temp);
				printf("Senzor1:%u\r\n", vrednost_temperature);
				xQueueSend(red_kanal1, &vrednost_temperature, 0U);
				broj_karaktera = 0;
				memset(temp, 0, sizeof(temp));
			}
			else if (broj_karaktera < sizeof(temp) - 1)
			{
				temp[broj_karaktera++] = c;
			}
		}
		
	}
}

void racunanje_temperature_sa_kanala(void* pvParams)
{
	uint8_t temp0 = 0, temp1 = 0;
	uint8_t vrednost0 = 0, vrednost1 = 0;
	uint8_t max = 0, min = 255;
	MyDouble trenutna_vrednost = 0;

	for (;;)
	{
		// Čekaj da kanál 0 pošalje nenultu vrednost
		do {
			xQueueReceive(red_kanal0, &temp0, portMAX_DELAY);
		} while (temp0 == 0);
		vrednost0 = temp0;

		// Čekaj da kanál 1 pošalje nenultu vrednost
		do {
			xQueueReceive(red_kanal1, &temp1, portMAX_DELAY);
		} while (temp1 == 0);
		vrednost1 = temp1;

		// MAX
		if (vrednost0 > max) max = vrednost0;
		else if (vrednost1 > max) max = vrednost1;

		// MIN
		if (vrednost0 < min && vrednost0>10) min = vrednost0;
		else if (vrednost1 < min && vrednost1>10) min = vrednost1;

		// Trenutna srednja vrednost
		trenutna_vrednost = ((MyDouble)vrednost0 + (MyDouble)vrednost1) / 2.0;

		// Slanje dalje
		if (trenutna_vrednost > min && trenutna_vrednost < max)
		{
			xQueueSend(trenutna_displej, &trenutna_vrednost, 0U);
			xQueueSend(trenutna_serijska, &trenutna_vrednost, 0U);
			xQueueSend(trenutna, &trenutna_vrednost, 0U);
		}
		else
		{
			//MISRA
		}
		xQueueSend(maksimalna_vrednost, &max, 0U);
		xQueueSend(minimalna_vrednost, &min, 0U);

	}
}









