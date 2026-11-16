#include "mythread.h"
#include <winsock2.h>
SOCKET sock;
char reponse [500];
int recv_size;
SOCKADDR_IN addr;
Mythread::Mythread(QObject *parent)
    : QThread(parent){
}
void Mythread::run(){
    printf("4");
    WSADATA WSAData;

    WSAStartup(MAKEWORD(2,0), &WSAData);
    //Fin spécificité WINDOWS

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("192.168.184.160"); //IP Serveur destinataire de la demande de connexion
    addr.sin_port = htons(8067); //Port d'écoute du serveur destinataire
    sock=socket(AF_INET,SOCK_DGRAM,0);
    printf("3");
    while(1){

        QThread::msleep(100);
    }
}
void Mythread::bANGLE(int angle){
    char message[30];
    sprintf((char*)message,"ANGLE %d",angle);
    if( sendto(sock , message , strlen(message) , 0,(SOCKADDR*) &addr,sizeof(addr) ) < 0)
    {
        puts("Send failed");

    }
    puts("Data Send\n");
    QThread::msleep(100);
    if((recv_size = recvfrom(sock , reponse , 500 , 0, NULL,NULL)) == SOCKET_ERROR)
    {
        puts("recv failed");
    }else{
        puts("Reply received\n");
        //Add a NULL terminating character to make it a proper string
        reponse[recv_size] = '\0';
        puts(reponse);}

}

void Mythread::bDIST(){printf("2");
    char message[5];
    sprintf((char*)message,"DIST");
    if( send(sock , message , strlen(message) , 0) < 0)
    {
        puts("Send failed");

    }
    puts("Data Send\n");
    QThread::msleep(100);
    if((recv_size = recv(sock , reponse , 500 , 0)) == SOCKET_ERROR)
    {
        puts("recv failed");
    }else{
        puts("Reply received\n");
        //Add a NULL terminating character to make it a proper string
        reponse[recv_size] = '\0';
        puts(reponse);}

}