#include "stm32f7xx.h"                  // Device header
#include "cmsis_os2.h"
#include "GUI.h"
#include "rl_net.h"
#include "AppWizard.h"
#include <stdio.h>

extern void configuration_overclock(void);
extern int Init_GUIThread (void);
extern void Init_Ethernet (void);

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
void EXTI0_IRQHandler(void){
	
	if(EXTI->PR & (1<<0)){
		
		EXTI->PR |= (1<<0); // clear le bit
		
		if( GPIOA->IDR & (1<<0)){ // etat haut
			
			TIM6->CNT = 0;
			TIM6->CR1 |=(1<<0);
		}
		else{  // etat bas
			TIM6->CR1 &= ~(1<<0); 
			temp_tick=TIM6->CNT;
			fin_mesure=1;
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
			distance_cm=temp_tick/(2.0*58.0);
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
	RCC->APB2ENR |= (1<<17); // Enable TIM10
	RCC->APB2ENR |= (1<<14); // Activer SYSCFG pour IRQ sur ECHO
	RCC->APB1ENR |= (1<<4);//Enable TIM6
	

	GPIOF->MODER |= (2<<16); //PF8 in AF mode
	GPIOF->AFR[1]|=(9<<0); //PF8 in AF9 mode
	GPIOI->MODER |= (1<<4); //PI2 in Output Mode
	GPIOA->MODER |=(3<<8); //PA4 (DAC) en anaog mode
	GPIOF->MODER |=(3<<20); //FP10 potentiomètre en analog mode
	GPIOF->MODER |= (2<<12); // PF6 in AF Mode
	GPIOF->AFR[0] |= (3<<24); // PF6 in AF3 Mode (IM10_CH1) 
	// Metre IRQ EXTI0 sur PA0/A0
	//SYSCFG->EXTICR[0] |= (0<<0); Inutile l'idée est là, on choisit PA[x] pin
	
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
	
	// TIM10 pour Trig
	TIM10->CCMR1 |= (6<<4);
	TIM10->CCER |= (1<<0);
	TIM10->PSC = 99;
	TIM10->ARR=39999;
	TIM10->CCR1 = 20; // Période de 20ms/40000 = Temps haut à 0.5 µs. Pour 10µs, il faut CCR1 à 200. (ça devrait être 20, mais à l'oscillo c'est 200)
	TIM10->CR1 |=(1<<0);
	
	//interruption Pa0
	EXTI->IMR |= (1<<0);
	EXTI->RTSR |= (1<<0);
	EXTI->FTSR |= (1<<0);
	NVIC_SetPriority(EXTI0_IRQn,5);
	NVIC_EnableIRQ(EXTI0_IRQn);
	
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