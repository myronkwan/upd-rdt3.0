#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>
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
	// format: ./client port ip input output
	int sock, portNum, nBytes;
	char buff[BUFF_SIZE];
	char *inputname, *outputname;
	FILE* input;
	struct sockaddr_in serverAddr;
	socklen_t addr_size;
	int seq=0;
	PACKET pkt,ack;
	PACKET * pktptr=&pkt;//set pointer to pkt
	PACKET * ackptr=&ack;
	int check=1;
	int timeouts;//track of # of timeouts for each packet. if 3, terminate.
	struct timeval tv;
	int rv;

	if (argc != 5)
	{
		printf ("need the port number, machine, inputfilename, outputfilename \n");
		return 1;
	}
	// configure address
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons (atoi (argv[1]));
	inet_pton (AF_INET, argv[2], &serverAddr.sin_addr.s_addr);
	memset (serverAddr.sin_zero, '\0', sizeof (serverAddr.sin_zero));
	memset(buff,'\0',sizeof(buff));
	addr_size = sizeof(serverAddr);
	/*Create UDP socket*/
	sock = socket (PF_INET, SOCK_DGRAM, 0);

	inputname=argv[3];
	outputname=argv[4]+'\0';
	srand(time(NULL));//for rng	
	
	input=fopen(inputname,"r");
	if(input==NULL){
		printf("cant open input file");
		exit(0);
	}
	timeouts=0;//set # of timeouts. if 3 terminate
	while(check==1){
		if(timeouts>=3){
			printf("3 timeouts...terminating client.\n");
			return 1;
		}
		//create packet containing outputfilename
		pkt.header.sequenceAck=seq;
		pkt.header.length=sizeof(outputname);
		pkt.header.fin=0;
		memcpy(pkt.data,outputname,sizeof(outputname));
		pkt.header.checksum=checksum(pktptr);

		//send outputfile name to server
		if((rand()%100)<95)//90% chance
			sendto (sock, &pkt,sizeof(pkt), 0, (struct sockaddr *)&serverAddr, addr_size);
		
		//recieve ack/nack	
		fd_set readfds;
		fcntl(sock, F_SETFL, O_NONBLOCK);
		FD_ZERO(&readfds);
		FD_SET(sock,&readfds);
		tv.tv_sec=1;
		tv.tv_usec=0;
	
		rv=select(sock+1,&readfds,NULL,NULL,&tv);
		if(rv==0){//timeout
			timeouts++;
			//loop back and resend packet
		}
		else if(rv==1){//data to be recieved
			//check the checksum and seq num
			recvfrom(sock,&ack,sizeof(ack),0,(struct sockaddr *)&serverAddr,addr_size);
			if((rand()%100)<20)//20% chance of incorrect checksum
				ack.header.checksum+=1;
			if((ack.header.checksum==checksum(ackptr))&&(ack.header.sequenceAck==seq)){
				printf("recieved ack\n");				
				seq=1-seq;				
				check=0;
			}
			else
				printf("recieved nack\n");
				
		}
	
	}
	
	//start sending inputfile contents...set fin bit to 1 for last packet
	while(1){
		nBytes=fread(buff,1,BUFF_SIZE,input);
		timeouts=0;
		check=1;
		if(nBytes==0)
			pkt.header.fin=1;//if last packet
		while(check==1){
			if(timeouts>=3){
				printf("3 timeouts...terminating client.\n");
				return 1;			
			}
			//create packet
			pkt.header.sequenceAck=seq;
			pkt.header.length=nBytes;
			memcpy(pkt.data,buff,BUFF_SIZE);
			pkt.header.checksum=checksum(pktptr);
			
			if((rand()%100)<95)//90% chance
				sendto (sock, &pkt,sizeof(pkt), 0, (struct sockaddr *)&serverAddr, addr_size);
			
			//set timer
			fd_set readfds;
			fcntl(sock, F_SETFL, O_NONBLOCK);
			FD_ZERO(&readfds);
			FD_SET(sock,&readfds);
			tv.tv_sec=1;
			tv.tv_usec=0;

			//recieve ack/nack					
			rv=select(sock+1,&readfds,NULL,NULL,&tv);
			if(rv==0)
				timeouts++;
			else if(rv==1){
				recvfrom(sock,&ack,sizeof(ack),0,(struct sockaddr *)&serverAddr,addr_size);	
				if((rand()%100)<20)//20% chance of incorrect checksum
					ack.header.checksum+=1;	
				if((ack.header.checksum==checksum(ackptr))&&(ack.header.sequenceAck==seq)){
					printf("recieved ack\n");				
					seq=1-seq;				
					check=0;
				}
				else
					printf("recieved nack\n");
			}
			
		}
		if(nBytes==0)//exit after final packet...or timeout if ack for finish packet not recieved
			break;
	}
	
	
	fclose(input);
	close(sock);
	return 0;
}
