#include "stm32f7xx.h"                  // Device header
#include "cmsis_os2.h"
#include "GUI.h"
#include "rl_net.h"
#include "AppWizard.h"
#include <stdio.h>

extern void configuration_overclock(void);
extern int Init_GUIThread (void);
extern void Init_Ethernet (void);
void balayage (int debut,int fin,int pas);
void servo_set_angle(int deg);


#ifdef _RTE_
#include "RTE_Components.h"             // Component selection
#endif
#ifdef RTE_CMSIS_RTOS2                  // when RTE component CMSIS RTOS2 is used
#include "cmsis_os2.h"                  // ::CMSIS:RTOS2
#endif

/*----------------------------------------------------------------------------
 *      Tâche de test (vide) : à compléter par les étudiants
 *---------------------------------------------------------------------------*/
osThreadId_t tid_Test;
int angle;
int temp_tick=0;
int fin_mesure=0;
float distance_cm=0;
float tab_distance_cm[181];
extern uint32_t message;
void EXTI9_5_IRQHandler(void){
		if(EXTI->PR & (1<<6)){ // interruption venant de la ligne 6 (PG6)
			EXTI->PR = (1<<6); // clear le bit
			
			if( GPIOG->IDR & (1<<6)){ // etat haut
				TIM6->CR1 |=(1<<0);
			}
			else{  // etat bas
				temp_tick=TIM6->CNT;
				TIM6->CR1 &= ~(1<<0); 
				fin_mesure=1;
				TIM6->CNT = 0;
			}
		}
}
static void Thread_Test (void *argument)
{
  (void)argument;
  // Initialisations propres à la tâche (si besoin) ici
	int sens = 0;

  while (1)
  {
    // Votre boucle de test ici (mesure de distance, balayage du servo, etc.)
		/*if(sens == 0){
			angle+=2;
		}
		else{
			angle-=2;
		}
		
		if(angle>180){
			angle = 180;
			sens = 1;
		}
		if(angle<0){
			angle=0;
			sens=0;
		}*/
		
		if(fin_mesure){
			
			fin_mesure=0;
			distance_cm=temp_tick/(58.0);
		}
		
		servo_set_angle(angle);
		
		
		
    osDelay(100);   // période de la boucle : 100 ms
  }
}

static int Init_TestThread (void)
{
  tid_Test = osThreadNew(Thread_Test, NULL, NULL);
  if (tid_Test == NULL) {
    return -1;
  }
  return 0;
}

void servo_set_angle(int deg){
	int val;
	val = 4000.0*deg/180.0;
	TIM13->CCR1 = 1000+val; //rapport cyclique CCR/ARR
	
}
void balayage (int debut,int fin,int pas){
	
	for(int i=debut;i<=fin;i+=pas){
		servo_set_angle(i);
		osDelay(50);
		tab_distance_cm[i]=temp_tick/(58.0);
		
	}
	
}

int main(void)
{
  configuration_overclock(); //pour pouvoir utiliser l'écran LCD

	/********* ,n ,                                                          ,,,*************************
	Début Initialisation périphérique
	**********************************/
	//Initialiser vos périphériques ici

	RCC->AHB1ENR|=(1<<5); //Enable GPIOF (pour Timer13 et potentiomètre)
	RCC->APB1ENR|=(1<<7); //Enable TIM13
	RCC->AHB1ENR|=(1<<8); // Enable GPIO I pour Led
	RCC->APB1ENR |=(1<<29); //activation DAC
	RCC->AHB1ENR |=(1<<0);	// activation GPIOA pour DAC, et ECHO
	RCC->APB2ENR |=(1<<10); // Activation ADC3 pour le potentiomètre
	RCC->AHB1ENR |= (1<<1); // Enable GPIOB pour TRIG (PB14)
	RCC->AHB1ENR |= (1<<6); // Enable GPIOG pour ECHO (PG6)
	RCC->APB1ENR |= (1<<6); // Enable TIM12
	RCC->APB2ENR |= (1<<14); // Activer SYSCFG pour IRQ sur ECHO
	RCC->APB1ENR |= (1<<4);//Enable TIM6
	

	GPIOF->MODER |= (2<<16); //PF8 in AF mode
	GPIOF->AFR[1]|=(9<<0); //PF8 in AF9 mode
	GPIOI->MODER |= (1<<4); //PI2 in Output Mode
	GPIOA->MODER |=(3<<8); //PA4 (DAC) en anaog mode
	GPIOF->MODER |=(3<<20); //FP10 potentiomètre en analog mode
	GPIOB->MODER |= (2<<28); // PB14 in AF Mode (TRIG)
	GPIOB->AFR[1] |= (9<<24); // PB14 in AF9 Mode (TIM12_CH1)
	// PG6 (ECHO) reste en entrée (MODER = 00 par défaut)
	
	// TIM13
	TIM13->CCMR1 |= (6<<4); //TIM13_CH1 PWM1
	TIM13->CCER |= (1<<0); 
	TIM13->PSC = 49; //prescaler
	TIM13->ARR = 39999; //Periode
	TIM13->CCR1 = 20000; //rapport cyclique CCR/ARR
	TIM13->CR1 |= (1<<0);
	
	//TIM6 
	TIM6->PSC = 99;
	TIM6->ARR = 59999;
	
	//Dac
	DAC->CR |=(1<<0); // output channel 1 sur PA4
	
	//Potentiomètre
	//ADC_SQR1 a laisser sur 0
	ADC3->SQR3 |=(8<<0); // Permet d'indiquer que c'est IN8 qu'on associe à la conversion N°0
	ADC3->CR2 |=(1<<0); // Activer l'ADC3
	ADC3->CR2 |=(1<<30); // Démarrer la conversion
	
	// TIM12 pour Trig sur PB14 (APB1 : 100 MHz)
	TIM12->CCMR1 |= (6<<4); // PWM1 sur CH1
	TIM12->CCER |= (1<<0);
	TIM12->PSC = 149; // 100 MHz / 150 -> 1,5 µs par tick
	TIM12->ARR = 39999; // 40000 x 1,5 µs = 60 ms
	TIM12->CCR1 = 7; // Temps haut = 7 x 1,5 µs = 10,5 µs
	TIM12->CR1 |= (1<<0);
	
	//Interruption ECHO sur PG6 (EXTI6)
	SYSCFG->EXTICR[1] &= ~(0xF<<8); // EXTI6 : effacer bits 11:8
	SYSCFG->EXTICR[1] |= (6<<8); // EXTI6 relié au port G
	EXTI->IMR |= (1<<6);
	EXTI->RTSR |= (1<<6); // front montant
	EXTI->FTSR |= (1<<6); // front descendant
	NVIC_SetPriority(EXTI9_5_IRQn,5);
	NVIC_EnableIRQ(EXTI9_5_IRQn);
	
	/**********************************
	Fin Initialisation périphérique
	**********************************/

	/**********************************
	Utilisation RTOS (Keil RTX5)
	**********************************/ 
	#ifdef RTE_CMSIS_RTOS2
	  // Initialize CMSIS-RTOS2 
	  osKernelInitialize ();

	  // Create thread functions that start executing 
		Init_Ethernet();
		Init_GUIThread();
		Init_TestThread();   // tâche de test (boucle avec osDelay)

	  // Start thread execution 
	  osKernelStart();
	#endif

  /* Infinite loop  (ne s'exécute pas avec le RTOS!!!)*/
  while (1)
  {
		
  }
}