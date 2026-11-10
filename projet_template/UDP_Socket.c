/*------------------------------------------------------------------------------
 * MDK Middleware - Component ::Network:Service
 * Copyright (c) 2004-2019 Arm Limited (or its affiliates). All rights reserved.
 *------------------------------------------------------------------------------
 * Name:    UDP_Socket.c
 * Purpose: UDP Socket Code Template
 * Rev.:    V7.0.0
 *----------------------------------------------------------------------------*/
//! [code_UDP_Socket]
#include "rl_net.h"
#include "stm32f7xx.h"                  // Device header
#include "AppWizard.h"
#include <stdio.h>
 
int32_t udp_sock;                       // UDP socket handle
extern int angle;
extern int distance_cm;
extern void servo_set_angle(int);
// Notify the user application about UDP socket events.
uint32_t udp_cb_func (int32_t socket, const  NET_ADDR *addr, const uint8_t *buf, uint32_t len) {
	char cmd[len+1]; // +1 pour le caractère de fin de chaîne /0
	uint8_t *sendbuf;
	uint32_t i,message;
	int a;
	
	for (i=0; i<len; i++){
		if(!(buf[i] == '\n' || buf[i] == '\r')){
			cmd[i] = (char)buf[i];
		}
	}
	cmd[i] = '\0';
	
  sendbuf = netUDP_GetBuffer(64);
	if(sendbuf == NULL) return (0);
	
	if(sscanf(cmd, "ANGLE %d", &a)==1){
		
		if((a>=0)&&(a<=180)){
			message = (uint32_t)sprintf((char *)sendbuf, "OK\n");
			angle = a;
		}
		else {
			message = (uint32_t)sprintf((char *)sendbuf, "ERR Angle pas dans la plage\n");
		}
	}
	else {
			message = (uint32_t)sprintf((char *)sendbuf, "Probleme\n");
	}
	
	netUDP_Send(socket,addr,sendbuf,message);
  
  return (0);
}
 
// Send UDP data to destination client.
void send_udp_data (void) {
 
  if (udp_sock > 0) {
    // Example
    //IPv4 address: 192.168.0.1
    NET_ADDR addr = { NET_ADDR_IP4, 8067, 192, 168, 0, 1 };
    uint8_t *sendbuf;
 
    sendbuf = netUDP_GetBuffer (2);
    sendbuf[0] = 0x01;
    sendbuf[1] = 0xAA;
 
    netUDP_Send (udp_sock, &addr, sendbuf, 2);
    
  }
}
 
// Allocate and initialize the socket.
void Init_Ethernet (void) {
 
  netInitialize ();
 
  // Initialize UDP socket and open port 5000
  udp_sock = netUDP_GetSocket (udp_cb_func);
  if (udp_sock > 0) {
    netUDP_Open (udp_sock, 5000);
  }
}

//! [code_UDP_Socket]
