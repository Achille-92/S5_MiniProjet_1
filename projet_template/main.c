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

static void Thread_Test (void *argument)
{
  (void)argument;
  // Initialisations propres à la tâche (si besoin) ici
	int angle=0, sens = 0;

  while (1)
  {
    // Votre boucle de test ici (mesure de distance, balayage du servo, etc.)
		if(sens == 0){
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
	int val,min=0,max=180;
	val = (40000.0*deg)/180.0;
	TIM10->CCR1 = val; //rapport cyclique CCR/ARR
	
}


int main(void)
{
  configuration_overclock(); //pour pouvoir utiliser l'écran LCD

	/**********************************
	Début Initialisation périphérique
	**********************************/
	//Initialiser vos périphériques ici

	RCC->AHB1ENR|=(1<<5); //Enable GPIOF (pour Timer13 et potentiomètre)
	RCC->APB1ENR|=(1<<7); //Enable TIM13
	RCC->AHB1ENR|=(1<<8); // Enable GPIO I pour Led
	RCC->APB1ENR |=(1<<29); //activation DAC
	RCC->AHB1ENR |=(1<<0);	// activation GPIOA pour DAC
	RCC->APB2ENR |=(1<<10); // Activation ADC3 pour le potentiomètre
	RCC->APB2ENR |= (1<<17); // Enable TIM10

	GPIOF->MODER |= (2<<16); //PF8 in AF mode
	GPIOF->AFR[1]|=(9<<0); //PF8 in AF9 mode
	GPIOI->MODER |= (1<<4); //PI2 in Output Mode
	GPIOA->MODER |=(3<<8); //PA4 (DAC) en anaog mode
	GPIOF->MODER |=(3<<20); //FP10 potentiomètre en analog mode
	GPIOF->MODER |= (2<<12); // PF6 in AF Mode
	GPIOF->AFR[0] |= (3<<24); // PF6 in AF3 Mode (IM10_CH1) 
	
	// TIM13
	TIM13->CCMR1 |= (6<<4); //TIM13_CH1 PWM1
	TIM13->CCER |= (1<<0); 
	TIM13->PSC = 7; //prescaler
	TIM13->ARR = 39999; //Periode
	TIM13->CCR1 = 20000; //rapport cyclique CCR/ARR
	TIM13->CR1 |= (1<<0);
	
	//Dac
	DAC->CR |=(1<<0); // output channel 1 sur PA4
	
	//Potentiomètre
	//ADC_SQR1 a laisser sur 0
	ADC3->SQR3 |=(8<<0); // Permet d'indiquer que c'est IN8 qu'on associe à la conversion N°0
	ADC3->CR2 |=(1<<0); // Activer l'ADC3
	ADC3->CR2 |=(1<<30); // Démarrer la conversion
	
	// TIM10
	TIM10->CCMR1 |= (6<<4);
	TIM10->CCER |= (1<<0);
	TIM10->PSC = 7;
	TIM10->ARR=39999;
	TIM10->CCR1 = 0;
	TIM10->CR1 |=(1<<0);
	
	
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