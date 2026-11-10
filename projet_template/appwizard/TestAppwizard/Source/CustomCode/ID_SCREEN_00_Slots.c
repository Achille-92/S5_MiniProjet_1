/*********************************************************************
*                     SEGGER Microcontroller GmbH                    *
*        Solutions for real time microcontroller applications        *
**********************************************************************
*                                                                    *
*        (c) 1996 - 2025  SEGGER Microcontroller GmbH                *
*                                                                    *
*        Internet: www.segger.com    Support:  support@segger.com    *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File        : ID_SCREEN_00_Slots.c
Purpose     : AppWizard managed file, function content could be changed
---------------------------END-OF-HEADER------------------------------
*/

#include "Application.h"
#include "../Generated/Resource.h"
#include "../Generated/ID_SCREEN_00.h"

/*** Begin of user code area ***/
#include "stm32f7xx.h"                  // Device header
#include <stdio.h>
/*** End of user code area ***/

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/
/*********************************************************************
*
*       cbID_SCREEN_00
*/
void cbID_SCREEN_00(WM_MESSAGE * pMsg) {
  GUI_USE_PARA(pMsg);
}

/*********************************************************************
*
*       ID_SCREEN_00__ID_SWITCH_00__WM_NOTIFICATION_VALUE_CHANGED
*/
void ID_SCREEN_00__ID_SWITCH_00__WM_NOTIFICATION_VALUE_CHANGED(APPW_ACTION_ITEM * pAction, WM_HWIN hScreen, WM_MESSAGE * pMsg, int * pResult) {
  int Val;
  Val = APPW_GetValue(ID_SCREEN_00, ID_SWITCH_00, NULL);
  //Val vaut 0 ou 1
  //code à exécuter à mettre ici
	GPIOI->BSRR |=(1<<18); // Ativer le reset de la bascule
	if(Val == 1){
		GPIOI->BSRR |=(1<<2); // Mettre à 1 la sortie
	}
	else{
		GPIOI->BSRR &=~(0<<2); // Mettre à 0 la sortie
	}
}

/*********************************************************************
*
*       ID_SCREEN_00__ID_SLIDER_00__WM_NOTIFICATION_VALUE_CHANGED
*/
void ID_SCREEN_00__ID_SLIDER_00__WM_NOTIFICATION_VALUE_CHANGED(APPW_ACTION_ITEM * pAction, WM_HWIN hScreen, WM_MESSAGE * pMsg, int * pResult) {
  int Val;
  Val = APPW_GetValue(ID_SCREEN_00, ID_SLIDER_00, NULL);
  //Val vaut une valeur entre 0 et 100
  //code à exécuter à mettre ici
	DAC->DHR12R1 = 4095.0*Val/100.0;
}

/*********************************************************************
*
*       ID_SCREEN_00__ID_ROTARY_00__WM_NOTIFICATION_VALUE_CHANGED
*/
void ID_SCREEN_00__ID_ROTARY_00__WM_NOTIFICATION_VALUE_CHANGED(APPW_ACTION_ITEM * pAction, WM_HWIN hScreen, WM_MESSAGE * pMsg, int * pResult) {
  int Val;
  Val = APPW_GetValue(ID_SCREEN_00, ID_ROTARY_00, NULL);
  //Val vaut une valeur entre -300 et 300
  //code à exécuter à mettre ici
	
	//TIM13->CCR1 = (40000.0*(Val+300))/600.0; //rapport cyclique CCR/ARR
}

/*********************************************************************
*
*       ID_SCREEN_00__APPW_NOTIFICATION_INITDIALOG__ID_TIMER_00__APPW_JOB_START
*/
void ID_SCREEN_00__APPW_NOTIFICATION_INITDIALOG__ID_TIMER_00__APPW_JOB_START(APPW_ACTION_ITEM * pAction, WM_HWIN hScreen, WM_MESSAGE * pMsg, int * pResult) {
  GUI_USE_PARA(pAction);
  GUI_USE_PARA(hScreen);
  GUI_USE_PARA(pMsg);
  GUI_USE_PARA(pResult);
}

/*********************************************************************
*
*       ID_SCREEN_00__ID_TIMER_00__APPW_NOTIFICATION_TIMER
*/
extern int angle;
extern float distance_cm;
void ID_SCREEN_00__ID_TIMER_00__APPW_NOTIFICATION_TIMER(APPW_ACTION_ITEM * pAction, WM_HWIN hScreen, WM_MESSAGE * pMsg, int * pResult) {
	APPW_SetValue(ID_SCREEN_00,ID_GAUGE_00,ADC3->DR);
	ADC3->CR2 |=(1<<30); // Démarrer la conversion
	char texte[50];
	sprintf(texte, "A: %d deg d: %3.1f cm",angle,distance_cm);
	TEXT_SetText(WM_GetDialogItem(hScreen, ID_TEXT_00),texte);
	
}

/*************************** End of file ****************************/
