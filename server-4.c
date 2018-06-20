
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <time.h>
#include "packet.h"
#define BUFF_SIZE 10


int checksum(PACKET * pkt){
	char buff=0;
	int i;
	unsigned char * pktptr=pkt;
	pktptr++;//skip checksum bit, which is the first bit of the packet
	for(i=1;i<sizeof(*pkt);++i){
		buff=buff^ *pktptr++;
	}
}
int main (int argc, char *argv[])
{
	int sock, nBytes,seqnum;//seqnum is the sequence number of packet expected
	char buffer[10];
	struct sockaddr_in serverAddr, clientAddr;
	struct sockaddr_storage serverStorage;
	socklen_t addr_size, client_addr_size;
	int i;
	int check=1;
	FILE* output;
	PACKET ack;
	PACKET * ackptr=&ack;
	if (argc != 2)
	{
		printf ("need the port number\n");
		return 1;
	}
	srand(time(NULL));// for rng
	PACKET pkt;
	PACKET * pktptr=&pkt;
	// init
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons ((short)atoi (argv[1]));
	serverAddr.sin_addr.s_addr = htonl (INADDR_ANY);
	memset ((char *)serverAddr.sin_zero, '\0', sizeof (serverAddr.sin_zero));
	memset((char*)buffer,'\0',sizeof(buffer));
	addr_size = sizeof (serverStorage);

	// create socket
	if ((sock = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf ("socket error\n");
		return 1;
	}
	// bind
	if (bind (sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != 0)
	{
		printf ("bind error\n");
		return 1;
	}

	while(1){//receive packet for name of outputfile
		seqnum=0;//set expected value of first packet
		recvfrom (sock, &pkt, sizeof(pkt),0, (struct sockaddr *)&serverStorage, &addr_size);

		
		if((rand()%100)<20)//20% chance of incorrect checksum
			pkt.header.checksum+=1;

		if((checksum(pktptr)==pkt.header.checksum) && (pkt.header.sequenceAck==seqnum)){//if file not corrupted && correct seq num, send ACK	
			ack.header.sequenceAck=seqnum;
			ack.header.checksum=checksum(ackptr);
			if((rand()%100)<95)//90% chance to send ack
				sendto(sock,&ack,sizeof(ack),0,(struct sockaddr *)&serverStorage, addr_size);
			seqnum=1-seqnum;//update next seq number
			break;
		}
		else{//send NACK
			ack.header.sequenceAck=1-seqnum;
			ack.header.checksum=checksum(ackptr);
			if((rand()%100)<95)//90% chance to send NACK
				sendto(sock,&ack,sizeof(ack),0,(struct sockaddr *)&serverStorage, addr_size);
		}
	}
	//open outputfile
	output=fopen(pkt.data,"w");//since we are using packet.h the output filename must be 10char max incuding .txt
	if(output==NULL){
		printf("couldn't open output file\n");
		exit(0);		
	}
	
	//start receiving input file contents, stop after recieving packet with fin bit of 1. 
	
	while(1){
		recvfrom (sock, &pkt, sizeof(pkt),0, (struct sockaddr *)&serverStorage, &addr_size);
		//printf("checksum is : %d and seqnum is %d\n",pkt.header.checksum,pkt.header.sequenceAck);
		//printf("expected checksum is :%d and expected seqnum is %d\n",checksum(pktptr),seqnum);

		if((rand()%100)<20)//20% chance of incorrect checksum
			pkt.header.checksum+=1;

		if((checksum(pktptr)==pkt.header.checksum) && (pkt.header.sequenceAck==seqnum)){//if file not corrupted && correct seq num, send ACK	
			fwrite(pkt.data,1,pkt.header.length,output);//write to output file			
			
			ack.header.sequenceAck=seqnum;
			ack.header.checksum=checksum(ackptr);
			if((rand()%100)<95)//80% chance to send ack	
				printf("send ack\n");
				sendto(sock,&ack,sizeof(ack),0,(struct sockaddr *)&serverStorage, addr_size);
			seqnum=1-seqnum;//update next seq number
		}
		else{//send NACK
			printf("send nack\n");
			ack.header.sequenceAck=1-seqnum;
			ack.header.checksum=checksum(ackptr);
			if((rand()%100)<95)//90% chance to send ack
				sendto(sock,&ack,sizeof(ack),0,(struct sockaddr *)&serverStorage, addr_size);
		}
		//if recieved last packet, break
		if(pkt.header.fin==1)
			break;
	}


	return 0;
}


