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
#define led_punjac_naponski 0x03
#define LED_punjac_strujni 0x05

// TASK PRIORITIES 
#define	TASK_SERIAL_SEND_PRI		( tskIDLE_PRIORITY + 2 )
#define TASK_SERIAl_REC_PRI			( tskIDLE_PRIORITY + 4 )
#define	SERVICE_TASK_PRI			( tskIDLE_PRIORITY + 1 )
#define TASK_SERIAl_KANAL0			( tskIDLE_PRIORITY + 3 )
#define obrada		( tskIDLE_PRIORITY + 0 )

// TASKS: FORWARD DECLARATIONS 
void LEDBar_Task(void* pvParameters);
void SerialSend_Task(void* pvParameters);
void SerialSend_kanal0(void* pvParameters);
void SerialReceive_Task(void* pvParameters);
void SerialReceive_kanal0(void* pvParameters);
void obrada_sa_PC(void* pvParameters);
void racunanje_napona_sa_kanala0(void* pvParams);


// TRASNMISSION DATA - CONSTANT IN THIS APPLICATION 
//const char trigger[] = "X";
//unsigned volatile t_point;


// RECEPTION DATA BUFFER - COM 0
#define R_BUF_SIZE (5)
uint8_t r_buffer[R_BUF_SIZE];
//unsigned volatile r_point;


// 7-SEG NUMBER DATABASE - ALL HEX DIGITS [ 0 1 2 3 4 5 6 7 8 9 A B C D E F ]
static const char hexnum[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 };


// GLOBAL OS-HANDLES 
SemaphoreHandle_t LED_INT_BinarySemaphore;
SemaphoreHandle_t TBE_BinarySemaphore;
SemaphoreHandle_t RXC_BinarySemaphore;
SemaphoreHandle_t RXC_kanal0_BinarySemaphore;
SemaphoreHandle_t TBE_kanal0_BinarySemaphore;
QueueHandle_t LEDBar_Queue;
QueueHandle_t red_kanal0;
QueueHandle_t red_kanal1;
QueueHandle_t prikaz20_vrednosti;
TimerHandle_t tajmer;



// STRUCTURES


// INTERRUPTS //
static uint32_t OnLED_ChangeInterrupt(void) {	// OPC - ON INPUT CHANGE - INTERRUPT HANDLER 
	BaseType_t xHigherPTW = pdFALSE;

	xSemaphoreGiveFromISR(LED_INT_BinarySemaphore, &xHigherPTW);
	portYIELD_FROM_ISR(xHigherPTW);
}

static uint32_t prvProcessTBEInterrupt(void) {	// TBE - TRANSMISSION BUFFER EMPTY - INTERRUPT HANDLER 
	BaseType_t xHigherPTW = pdFALSE;

	if (get_TBE_status(0) != 0)
		xSemaphoreGiveFromISR(TBE_kanal0_BinarySemaphore, &xHigherPTW);

	if (get_TBE_status(1) != 0)
		xSemaphoreGiveFromISR(TBE_BinarySemaphore, &xHigherPTW);

	portYIELD_FROM_ISR(xHigherPTW);
}

static uint32_t prvProcessRXCInterrupt(void) {	// RXC - RECEPTION COMPLETE - INTERRUPT HANDLER 
	BaseType_t xHigherPTW = pdFALSE;

	if (get_RXC_status(0) != 0)
		xSemaphoreGiveFromISR(RXC_kanal0_BinarySemaphore, &xHigherPTW);

	if (get_RXC_status(1) != 0)
		xSemaphoreGiveFromISR(RXC_BinarySemaphore, &xHigherPTW);


	portYIELD_FROM_ISR(xHigherPTW);
}

/* PERIODIC TIMER CALLBACK */






// MAIN - SYSTEM STARTUP POINT 
void main_demo(void) {
	// INITIALIZATION OF THE PERIPHERALS
	//init_7seg_comm();
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
	TBE_kanal0_BinarySemaphore = xSemaphoreCreateBinary();
	// QUEUES
	LEDBar_Queue = xQueueCreate(1, sizeof(uint8_t));
	red_kanal0 = xQueueCreate(30, sizeof(uint8_t[30]));
	red_kanal1 = xQueueCreate(1, sizeof(uint8_t[20]));
	//prikaz20_vrednosti = xQueueCreate(20,sizeof(double));

	// TASKS 
	xTaskCreate(SerialSend_Task, "STx", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAL_SEND_PRI, NULL);	// SERIAL TRANSMITTER TASK 
	xTaskCreate(SerialReceive_Task, "SRx", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAl_REC_PRI, NULL);	// SERIAL RECEIVER TASK 
	//r_point = 0;
	xTaskCreate(LEDBar_Task, "ST", configMINIMAL_STACK_SIZE, NULL, SERVICE_TASK_PRI, NULL);				// CREATE LED BAR TASK  

	xTaskCreate(SerialSend_kanal0, "STx", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAL_SEND_PRI, NULL);	// SERIAL TRANSMITTER TASK 
	xTaskCreate(SerialReceive_kanal0, "SRx", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAl_REC_PRI, NULL);
	xTaskCreate(obrada_sa_PC, NULL, configMINIMAL_STACK_SIZE, NULL, TASK_SERIAl_KANAL0, NULL);
	xTaskCreate(racunanje_napona_sa_kanala0, NULL, configMINIMAL_STACK_SIZE, NULL, obrada, NULL);

	// START SCHEDULER
	vTaskStartScheduler();
	while (1);
}

// TASKS: IMPLEMENTATIONS
void LEDBar_Task(void* pvParameters)
{
	uint8_t rec;
	uint8_t dioda;

	while (1)
	{
		get_LED_BAR(0, &dioda);

		// Čekanje podataka iz reda
		if (xQueueReceive(LEDBar_Queue, &rec, portMAX_DELAY) == pdPASS)
		{
			// Kontrola LED bara na osnovu primljenih podataka
			if (rec == 1 && dioda == 0x01)
			{
				set_LED_BAR(1, led_punjac_naponski);
			}
			else if (rec == 2 && dioda == 0x02)
			{
				set_LED_BAR(1, LED_punjac_strujni);
			}
			else if (rec == 0 && dioda == 0x03)
			{
				set_LED_BAR(1, led_punjac_naponski);
			}
			else if (rec == 15 && dioda == 0x04)
			{
				set_LED_BAR(1, LED_punjac_strujni);
			}


		}
	}
}


void SerialSend_Task(void* pvParameters) {
	uint8_t t_point = 0;
	double niz_brojeva[30] = { 0 };



	while (1) {
		xSemaphoreTake(TBE_BinarySemaphore, portMAX_DELAY);

		// Proverite da li je duzina veća od nule i da li je rec inicijalizova
		if (t_point > 30)
			t_point = 0;
		send_serial_character(COM_CH_1, niz_brojeva[t_point++]);



		vTaskDelay(pdMS_TO_TICKS(100)); // Ako je potrebno vremensko kašnjenje
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


/*
void SerialReceive_kanal0(void* pvParameters)
{
	double ad_konv = 0;
	uint8_t broj_karaktera = 0;
	uint8_t napon[6] = { 0 };
	uint8_t c = 0;

	while (1)
	{
		xSemaphoreTake(RXC_kanal0_BinarySemaphore, portMAX_DELAY);
		get_serial_character(COM_CH_0, &c);

		if (c == 0x0d)
		{

			ad_konv = atof(napon);
			broj_karaktera = 0;
			xQueueSend(red_kanal0, &ad_konv, 0U);

		}
		else
			napon[broj_karaktera++] = c;



	}

}*/

void SerialReceive_kanal0(void* pvParameters)
{
	uint8_t vrednost_napona[20] = { 0 };
	uint8_t c = 0;
	uint8_t broj_karaktera = 0;

	while (1)
	{
		xSemaphoreTake(RXC_kanal0_BinarySemaphore, portMAX_DELAY);
		get_serial_character(COM_CH_0, &c);

		if (c == 0x0d) // Ako je primljen karakter za kraj niza
		{
			/*for (uint8_t i = 0; i < 20; i++) {
				printf("vrijednost: %u\n", (uint8_t)vrednost_napona[i]);
			}
			broj_karaktera = 0; // Resetovanje brojača za sledeći niz
		}*/
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
}

void racunanje_napona_sa_kanala0(void* pvParams)
{
	double prijem_napona[30] = { 0 };
	uint8_t pomocna = 0;
	double suma = 0;
	double srednja_vrednost = 0;
	double potrebne_vrednosti[20] = { 0 };
	uint8_t i = 0;

	while (1)
	{
		if (xQueueReceive(red_kanal0, &pomocna, 0U) == pdPASS)
		{
			printf("PRIMLJENI NAPON: %d\n\r", pomocna);
			prijem_napona[i] = pomocna;
			i++;
		}

		if (i == 30)
		{
			for (i = 10; i <= 29; i++) {
				suma += prijem_napona[i];
			}
			srednja_vrednost = suma / 20;
			printf("Srednja_vr: %f \r\n", srednja_vrednost);
			i = 0;
			pomocna = 0;
			suma = 0;
		}
		//xQueueSend(prikaz20_vrednosti, &srednja_vrednost, 0U);
	}
}


void SerialSend_kanal0(void* pvParameters)
{
	uint8_t niz_brojeva[20] = { 0 };
	uint8_t indeks_brojeva = 0;

	while (1) {
		xSemaphoreTake(TBE_kanal0_BinarySemaphore, portMAX_DELAY);
		//	xQueueReceive(prikaz20_vrednosti,&niz_brojeva,0U);

		if (indeks_brojeva > 20)
			indeks_brojeva = 0;

		send_serial_character(COM_CH_0, niz_brojeva[indeks_brojeva++]);
		vTaskDelay(pdMS_TO_TICKS(100)); // Ako je potrebno vremensko kašnjenje
	}

}




void obrada_sa_PC(void* pvParameters)
{
	char prijem_rec[13] = { 0 }; // Promenjena veličina da bude char niz za string
	uint8_t rec = 0;

	while (1)
	{
		xQueueReceive(red_kanal1, prijem_rec, portMAX_DELAY); // Promenjen timeout na portMAX_DELAY

		// Upisivanje null-terminatora (nije potrebno ako je već nulti-terminisan)
		// prijem_rec[12] = '\0'; 

		// Upoređivanje stringa
		if (strcmp(prijem_rec, "KONTINUALNO") == 0)
		{
			rec = 1;
		}
		else if (strcmp(prijem_rec, "KONTROLISANO") == 0)
		{
			rec = 2; // Ako nije "KONTINUALNO", postavi rec na 0 ili neku drugu default vrednost
		}
		else if (strcmp(prijem_rec, "Admin12.5") == 0) {
			rec = 0;
		}
		else if (strcmp(prijem_rec, "Admax14.5") == 0) {
			rec = 15;
		}


		// Slanje podataka u red
		xQueueSend(LEDBar_Queue, &rec, portMAX_DELAY); // Promenjen timeout na portMAX_DELAY
	}
}
