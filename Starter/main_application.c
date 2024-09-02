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
#define led_punjac_naponski 0x08
#define LED_punjac_strujni 0x04

// TASK PRIORITIES 
#define	TASK_SERIAL_SEND_PRI		( tskIDLE_PRIORITY + 3 )
#define TASK_SERIAl_REC_PRI			( tskIDLE_PRIORITY + 4 )
#define	SERVICE_TASK_PRI			( tskIDLE_PRIORITY + 2 )
#define TASK_SERIAl_KANAL0			( tskIDLE_PRIORITY + 5 )
#define obrada		( tskIDLE_PRIORITY + 1 )

// TASKS: FORWARD DECLARATIONS 
void LEDBar_Task(void* pvParameters);
void LCD_Displej(void* pvParameters);
void SerialSend_Task(void* pvParameters);
void SerialReceive_Task(void* pvParameters);
void SerialReceive_kanal0(void* pvParameters);
void obrada_sa_PC(void* pvParameters);
void racunanje_napona_sa_kanala0(void* pvParams);
void PC_Ispis(void* pvParameters);



// TRASNMISSION DATA - CONSTANT IN THIS APPLICATION 
volatile uint8_t novi_podatak_primljen = 0;
const char trigger[] = "XYZ";
unsigned volatile t_point;
double trenutni_napon = 0.0; // Trenutni napon akumulatora


// RECEPTION DATA BUFFER - COM 0
#define R_BUF_SIZE (5)
uint8_t r_buffer[R_BUF_SIZE];


// 7-SEG NUMBER DATABASE - ALL HEX DIGITS [ 0 1 2 3 4 5 6 7 8 9 A B C D E F ]
static const char hexnum[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 };


// GLOBAL OS-HANDLES 
SemaphoreHandle_t LED_INT_BinarySemaphore;
SemaphoreHandle_t TBE_BinarySemaphore;
SemaphoreHandle_t RXC_BinarySemaphore;
SemaphoreHandle_t RXC_kanal0_BinarySemaphore;
SemaphoreHandle_t displej;
SemaphoreHandle_t rec1;
SemaphoreHandle_t napon1;

QueueHandle_t LEDBar_Queue;
QueueHandle_t red_kanal0;
QueueHandle_t red_kanal1;
QueueHandle_t prikaz20_vrednosti;
QueueHandle_t prikaz_vrednosti;
QueueHandle_t trenutna_vrednost_serijska;
QueueHandle_t maks_min_napon;
//QueueHandle_t maks_min_napon_displej;
QueueHandle_t maksimalna_vrednost;
QueueHandle_t minimalna_vrednost;

TimerHandle_t tajmer;
TimerHandle_t tajmer_prijem;
//TimerHandle_t tajmer_slanje;



// STRUCTURES


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

static uint32_t prvProcessRXCInterrupt(void) {	// RXC - RECEPTION COMPLETE - INTERRUPT HANDLER 
	BaseType_t xHigherPTW = pdFALSE;

	if (get_RXC_status(0) != 0)
	{
		novi_podatak_primljen = 1;
		xSemaphoreGiveFromISR(RXC_kanal0_BinarySemaphore, &xHigherPTW);

	}
	if (get_RXC_status(1) != 0)
		xSemaphoreGiveFromISR(RXC_BinarySemaphore, &xHigherPTW);


	portYIELD_FROM_ISR(xHigherPTW);
}

/* PERIODIC TIMER CALLBACK */

static void TimerCallback(TimerHandle_t tajmer)
{
	xSemaphoreGive(displej);
}
static void TimerCallback_prijem(TimerHandle_t tajmer_prijem)
{
	xSemaphoreGive(RXC_kanal0_BinarySemaphore);
}
/*
static void TimerCallback_slanje(TimerHandle_t tajmer_slanje)
{
	//xSemaphoreGive();
}
*/


// MAIN - SYSTEM STARTUP POINT 
void main_demo(void) {
	// INITIALIZATION OF THE PERIPHERALS
	init_7seg_comm();
	init_LED_comm();
	init_serial_uplink(COM_CH_1);		// inicijalizacija serijske TX na kanalu 1
	init_serial_downlink(COM_CH_1);	// inicijalizacija serijske RX na kanalu 1
	init_serial_uplink(COM_CH_0);		// inicijalizacija serijske TX na kanalu 1
	init_serial_downlink(COM_CH_0);

	// INTERRUPT HANDLERS
	vPortSetInterruptHandler(portINTERRUPT_SRL_OIC, OnLED_ChangeInterrupt);		// ON INPUT CHANGE INTERRUPT HANDLER 
	vPortSetInterruptHandler(portINTERRUPT_SRL_TBE, prvProcessTBEInterrupt);	// SERIAL TRANSMITT INTERRUPT HANDLER 
	vPortSetInterruptHandler(portINTERRUPT_SRL_RXC, prvProcessRXCInterrupt);	// SERIAL RECEPTION INTERRUPT HANDLER 

	// BINARY SEMAPHORES
	LED_INT_BinarySemaphore = xSemaphoreCreateBinary();	// CREATE LED INTERRUPT SEMAPHORE 
	TBE_BinarySemaphore = xSemaphoreCreateBinary();		// CREATE TBE SEMAPHORE - SERIAL TRANSMIT COMM 
	RXC_BinarySemaphore = xSemaphoreCreateBinary();		// CREATE RXC SEMAPHORE - SERIAL RECEIVE COMM
	RXC_kanal0_BinarySemaphore = xSemaphoreCreateBinary();
	displej = xSemaphoreCreateBinary();
	rec1 = xSemaphoreCreateMutex();
	napon1 = xSemaphoreCreateMutex();
	xTaskCreate(SerialSend_Task, "STx", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAL_SEND_PRI, NULL);	// SERIAL TRANSMITTER TASK 
	xTaskCreate(SerialReceive_Task, "SRx", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAl_REC_PRI, NULL);	// SERIAL RECEIVER TASK 

	// QUEUES
	LEDBar_Queue = xQueueCreate(1, sizeof(uint8_t));
	red_kanal0 = xQueueCreate(30, sizeof(uint8_t[30]));
	red_kanal1 = xQueueCreate(1, sizeof(uint8_t[20]));
	prikaz20_vrednosti = xQueueCreate(1, sizeof(double));
	prikaz_vrednosti = xQueueCreate(1, sizeof(double));
	maks_min_napon = xQueueCreate(1, sizeof(uint8_t));
	//maks_min_napon_displej = xQueueCreate(1, sizeof(uint8_t));
	maksimalna_vrednost = xQueueCreate(1, sizeof(uint8_t));
	minimalna_vrednost = xQueueCreate(1, sizeof(uint8_t));
	trenutna_vrednost_serijska = xQueueCreate(1, sizeof(double));

	// TASKS 

	xTaskCreate(LCD_Displej, NULL, configMINIMAL_STACK_SIZE, NULL, SERVICE_TASK_PRI, NULL);
	xTaskCreate(LEDBar_Task, "ST", configMINIMAL_STACK_SIZE, NULL, SERVICE_TASK_PRI, NULL);	// CREATE LED BAR TASK  

	xTaskCreate(SerialReceive_kanal0, "SRx", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAl_REC_PRI, NULL);

	xTaskCreate(obrada_sa_PC, NULL, configMINIMAL_STACK_SIZE, NULL, obrada, NULL);
	xTaskCreate(racunanje_napona_sa_kanala0, NULL, configMINIMAL_STACK_SIZE, NULL, TASK_SERIAl_KANAL0, NULL);
	xTaskCreate(PC_Ispis, NULL, configMINIMAL_STACK_SIZE, NULL, TASK_SERIAl_KANAL0, NULL);

	tajmer = xTimerCreate("Timer", pdMS_TO_TICKS(100), pdTRUE, NULL, TimerCallback);
	xTimerStart(tajmer, 0);
	tajmer_prijem = xTimerCreate("Timer", pdMS_TO_TICKS(100), pdTRUE, NULL, TimerCallback_prijem);
	xTimerStart(tajmer_prijem, 0);

	// START SCHEDULER
	vTaskStartScheduler();
	while (1);
}

// TASKS: IMPLEMENTATIONS
void LEDBar_Task(void* pvParameters)
{
	uint8_t rec = 0;
	uint8_t dioda = 0;
	uint8_t napon = 0;
	double srednja_vrednost = 0;


	while (1)
	{

		get_LED_BAR(0, &dioda);
		xQueueReceive(LEDBar_Queue, &rec, pdMS_TO_TICKS(20));
		xQueueReceive(prikaz_vrednosti, &srednja_vrednost, pdMS_TO_TICKS(20));

		if (rec == 1 && dioda == 0x01)
		{

			set_LED_BAR(1, led_punjac_naponski);

		}

		else if (srednja_vrednost > 13.5 && rec == 2)
		{
			set_LED_BAR(1, led_punjac_naponski);
		}
		else if (srednja_vrednost < 12.5 && rec == 2)
		{
			set_LED_BAR(1, LED_punjac_strujni);
		}

		else if (srednja_vrednost > 14 && rec == 2)
		{

			set_LED_BAR(1, 0x00);

		}
		if (srednja_vrednost < 10)
		{
			set_LED_BAR(2, 0xff);
			vTaskDelay(pdMS_TO_TICKS(500));
			set_LED_BAR(2, 0x00);
			vTaskDelay(pdMS_TO_TICKS(500));
		}
	}
}

void LCD_Displej(void* pvParameters) {

	double trenutna_vrednost = 0;
	uint8_t dioda = 0;
	uint8_t pomocna = 0;
	uint8_t max = 0;
	uint8_t min = 0;
	while (1) {
		get_LED_BAR(0, &dioda);
		xSemaphoreTake(displej, portMAX_DELAY);

		if (xQueueReceive(prikaz20_vrednosti, &trenutna_vrednost, pdMS_TO_TICKS(20)) == pdPASS)
		{
			if (trenutna_vrednost > 15)
			{
				trenutna_vrednost = 15;
			}
			pomocna = (uint8_t)trenutna_vrednost / 10;
			select_7seg_digit(0);
			set_7seg_digit(hexnum[pomocna]);

			pomocna = (uint8_t)trenutna_vrednost % 10;
			select_7seg_digit(1);
			set_7seg_digit(hexnum[pomocna]);
		}
		select_7seg_digit(2);
		set_7seg_digit(0x40);

		if (xQueueReceive(maksimalna_vrednost, &max, pdMS_TO_TICKS(20)) == pdPASS)
		{
			if (dioda == 0x01)
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
			if (dioda == 0x04)
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


void SerialSend_Task(void* pvParameters) {
	t_point = 0;
	while (1) {
		if (t_point > (sizeof(trigger) - 1))
			t_point = 0;
		send_serial_character(COM_CH_1, trigger[t_point++]);
		xSemaphoreTake(TBE_BinarySemaphore, portMAX_DELAY);// kada se koristi predajni interapt
		//vTaskDelay(pdMS_TO_TICKS(100));// kada se koristi vremenski delay
	}
}

void PC_Ispis(void* pvParameters) {

	char kontinualno_niz[] = "Rezim punjenja: KONTINUALNO\r\n";
	char kontrolisano_niz[] = "Rezim punjenja: KONTROLISANO\r\n";
	//char trenutni_napon[] = "Trenutna vrednost napona: ";
	char poruka[30];
	uint8_t rec = 0;
	uint8_t napon = 0;
	int i = 0;

	for (;;) {
		vTaskDelay(pdMS_TO_TICKS(500)); // Povećano kašnjenje za bolju vidljivost promene

		if (xQueueReceive(LEDBar_Queue, &rec, pdMS_TO_TICKS(20)) == pdPASS) {
			if (rec == 1) {
				i = 0;
				while (i < sizeof(kontinualno_niz) - 1) {
					vTaskDelay(pdMS_TO_TICKS(100));
					send_serial_character(COM_CH_0, kontinualno_niz[i++]);
				}
			}
			else if (rec == 2) {
				i = 0;
				while (i < sizeof(kontrolisano_niz) - 1) {
					vTaskDelay(pdMS_TO_TICKS(100));
					send_serial_character(COM_CH_0, kontrolisano_niz[i++]);
				}
			}
		}

		vTaskDelay(pdMS_TO_TICKS(500));

		// Primanje vrednosti napona iz reda
		if (xQueueReceive(red_kanal0, &napon, pdMS_TO_TICKS(20)) == pdPASS) {
			// Formatirajte celu poruku u jedan string
			snprintf(poruka, sizeof(poruka), "Trenutna vrednost napona: %d\r\n", napon);

			// Slanje kompletne poruke
			i = 0;
			while (i < strlen(poruka)) {
				vTaskDelay(pdMS_TO_TICKS(100));
				send_serial_character(COM_CH_0, poruka[i++]);
			}
		}
	}
}




void SerialReceive_Task(void* pvParameters) {
	uint8_t cc = 0;
	uint8_t rec[13] = { 0 };
	uint8_t r_point = 0;
	while (1) {
		xSemaphoreTake(RXC_BinarySemaphore, portMAX_DELAY); // čeka na serijski prijemni interapt
		get_serial_character(COM_CH_1, &cc); // učitava primljeni karakter u promenjivu cc

		if (cc == 0x0d) { // za svaki KRAJ poruke, prikazati primljene bajtove direktno na displeju 3-4
			rec[r_point] = '\0'; // dodajte nulti karakter na kraj stringa
			printf("Primio: %s\n\r", rec);
			xQueueSend(red_kanal1, &rec, 0U);
			r_point = 0;
			memset(rec, 0, sizeof(rec)); // resetujte buffer posle slanja poruke
		}
		else if (r_point < sizeof(rec) - 1) { // pamti karaktere između 0x0D
			rec[r_point++] = cc;
		}
	}
}




void SerialReceive_kanal0(void* pvParameters)
{
	uint8_t vrednost_napona[30] = { 0 };
	uint8_t c = 0;
	uint8_t broj_karaktera = 0;

	while (1)
	{
		xSemaphoreTake(RXC_kanal0_BinarySemaphore, portMAX_DELAY);
		//get_serial_character(COM_CH_0, &c);
		if (novi_podatak_primljen) {
			if (get_serial_character(COM_CH_0, &c) == 0)
			{
				if (c == 0x0d) // Ako je primljen karakter za kraj niza
				{

					vrednost_napona[broj_karaktera] = '\0';
					//printf("PRIMIO:%s\n\r", vrednost_napona);
					xQueueSend(red_kanal0, &vrednost_napona, 0U);
					broj_karaktera = 0;
					memset(vrednost_napona, 0, sizeof(vrednost_napona));
				}
				else if (broj_karaktera < sizeof(vrednost_napona) - 1)
				{
					vrednost_napona[broj_karaktera++] = c;

				}


			}
			novi_podatak_primljen = 0;
		}
	}
}

void racunanje_napona_sa_kanala0(void* pvParams)
{
	double prijem_napona[30] = { 0 };
	uint8_t pomocna = 0;
	double suma = 0;
	double srednja_vrednost = 0;
	uint8_t i = 0;
	uint8_t max = 0;
	uint8_t min = 0;

	while (1)
	{
		if (xQueueReceive(red_kanal0, &pomocna, pdMS_TO_TICKS(100)) == pdPASS)
		{
			printf("PRIMLJENI NAPON: %d\n\r", pomocna);
			prijem_napona[i] = (double)pomocna;
			i++;
		}

		if (i == 32)
		{
			min = prijem_napona[2];
			max = prijem_napona[2];

			for (uint8_t j = 2; j < 32; j++)
			{
				printf("Vrednost prijem_napona[%d]: %f \n\r", j, prijem_napona[j]); // Ispis svake vrednosti radi debagovanja

				// Računanje sume za poslednjih 20 očitavanja
				if (j >= 12)
				{
					suma += prijem_napona[j];
				}

				// Računanje min i max za svih 30 vrednosti
				if (prijem_napona[j] < min)
				{
					min = prijem_napona[j];
				}
				if (prijem_napona[j] > max)
				{
					max = prijem_napona[j];
				}
			}

			srednja_vrednost = suma / 20;
			printf("Srednja vrednost: %f \r\n", srednja_vrednost);
			printf("Minimalna izmerena: %d \r\n", min);
			printf("Maksimalna izmerena: %d \r\n", max);


			i = 2;
			suma = 0;
		}

		xQueueSend(maksimalna_vrednost, &max, 0U);
		xQueueSend(minimalna_vrednost, &min, 0U);
		xQueueSend(prikaz20_vrednosti, &srednja_vrednost, 0U);
		xQueueSend(prikaz_vrednosti, &srednja_vrednost, 0U);
		xQueueSend(trenutna_vrednost_serijska, &srednja_vrednost, 0U);
	}
}


void obrada_sa_PC(void* pvParameters)
{
	char prijem_rec[13] = { 0 }; // Promenjena veličina da bude char niz za string
	uint8_t rec = 0;
	uint8_t napon = 0;

	while (1)
	{
		xQueueReceive(red_kanal1, prijem_rec, pdMS_TO_TICKS(50)); // Promenjen timeout na portMAX_DELAY 

		// Upoređivanje stringa
		if (strcmp(prijem_rec, "KONTINUALNO") == 0)
		{
			xSemaphoreTake(rec1, portMAX_DELAY);
			rec = 1;
			xQueueSend(LEDBar_Queue, &rec, 0U);
			xSemaphoreGive(rec1);
		}
		else if (strcmp(prijem_rec, "KONTROLISANO") == 0)
		{
			xSemaphoreTake(rec1, portMAX_DELAY);
			rec = 2;
			xQueueSend(LEDBar_Queue, &rec, 0U);// Ako nije "KONTINUALNO", postavi rec na 0 ili neku drugu default vrednost
			xSemaphoreGive(rec1);
		}
		if (strcmp(prijem_rec, "Admin12.5") == 0) {
			xSemaphoreTake(napon1, portMAX_DELAY);
			napon = 0;
			xQueueSend(maks_min_napon, &napon, 0U);
			xSemaphoreGive(napon1);

		}
		else if (strcmp(prijem_rec, "Admax14.5") == 0) {

			xSemaphoreTake(napon1, portMAX_DELAY);
			napon = 15;
			xQueueSend(maks_min_napon, &napon, 0U);
			xSemaphoreGive(napon1);
		}
	}
}