
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>

#define M 5
#define KSEC 5
typedef enum
{

REGISTER_REQUEST=1,
REGISTER_RESPONSE=2,
TOPOLOGY_UPDATE=3,
ROUTE_UPDATE=4,
KEEP_ALIVE=5

} MSG;

//WANG 0131
#include "topo_config.h"
char END_SIG[] = "/END\n";


int main(int argc, char *argv[])
{

	MSG recvHeader;
	int swSock, length, fromlen, matchID;
	int m=0;
	struct sockaddr_in server, from, swtch;
	struct hostent *hp;
	char buffer[512];
	char recvBuffer[512];
	struct timeval tv;
	fd_set sw_fds;
	fd_set read_fds;
	char buffer2[20]="Keep Alive";
	const char s[2]="/";
	char *token;
	
	int selectValue;
	int j, f, k;
	
	char mySwitchID[3];
	int numNeighbor;
	
	
	char responseBuffer[512];
	char tempBuffer[50];
	char keepAliveBuffer[20];
	
	int findNBR;
	int fdmax;
	int recvID;
	int sendReturn;
	
	int numSwitch;
	int routingTable[2][numSwitch];
	
	struct sockaddr_in actvNbrAddr;
	
	bzero(&actvNbrAddr, sizeof(actvNbrAddr));
	actvNbrAddr.sin_family=AF_INET;
	
	
	if (argc < 4)
	{
		printf("Usage: server IP, server port#, switch ID#\n");
		exit(1);
	}
	
	if (argc > 4 && strcmp (argv[4], "-f") == 0){
	
	printf("Entering Special Case-FaultLink\n");
	
	
	}
	
	
	
	//bzero(mySwitchID,5);
	strcpy(mySwitchID,argv[3]);
	
	/*create socket for switch*/
	swSock = socket(AF_INET, SOCK_DGRAM, 0);

	if(swSock < 0)
	{
		perror("Error in creating socket");
		exit(1);
	}
	
	
	/*filling server information*/
	bzero(&server, sizeof(server));
	inet_pton(AF_INET, argv[1], &server.sin_addr);
	server.sin_port = htons(atoi(argv[2]));
	length=sizeof(struct sockaddr_in);
	

	/*sending 1.switch ID and 2. switch port# information to server*/
	bzero(buffer,256);//empty buffer in case there will be junk msgs
	sprintf(buffer,"%i",REGISTER_REQUEST);
	strcat(buffer,"/");
	strcat(buffer,argv[3]);//switch
	//strcat(buffer,"/");
	//strcat(buffer,argv[4]);
	strcat(buffer, END_SIG);

	if(sendto(swSock, buffer, strlen(buffer),0 , (struct sockaddr*)&server, sizeof(server))<0)
	{
		perror("Error in sending switch ID and switch port# information to controller");
		exit(1);
	}

	//receiving and checking if it is neighbor information from controller
	
	if(recvfrom(swSock, buffer, sizeof(buffer), 0, (struct sockaddr*)&from, &fromlen)<0)
	{
		perror("Error in receiving intial information(neighbors) from controller");
		exit(1);		
	}
	//write(1,buffer,sizeof(buffer));//for debugging
	puts(buffer);

	token=strtok(buffer, s);
	if(token==NULL)//checking the received data have information
	{
		perror("Not receiving anything");
		exit(1);

	}

	if(atoi(token)!=REGISTER_RESPONSE)//checking the received data are the right response
	{
		perror("Error: Initial packet is not REGISTER RESPONSE!");
		exit(1);
	}
	


//WANG 0131
//Should know the numSwitch/numNeighbor after REG_REQ
	
	
	sprintf(keepAliveBuffer, "%i/", KEEP_ALIVE);
	strcat(keepAliveBuffer, mySwitchID);
	strcat(keepAliveBuffer, END_SIG);
	//puts(keepAliveBuffer);
//Lisa 0201 start
	
	token=strtok( NULL, s);
	if(token!=NULL)
	{
		numNeighbor=atoi(token);
	}
//Lisa 0201 end
	printf("Client: My Neighbor Number = %i.\n", numNeighbor);
	struct sockaddr_in nbrAddr[numNeighbor];
	neighborInfo_t neighborTable[numNeighbor];
	int flag[numNeighbor];
	length = sizeof(nbrAddr[0]);
	//topoTable & nodeTable Initialize by the REG_RES

	for(j = 0; j < numNeighbor; j++) {
			
			//printf("Client: Which neighbor table entry now = %i.\n", j);
			bzero(&nbrAddr[j], length);
			nbrAddr[j].sin_family=AF_INET;
			
			token=strtok( NULL, s);
			(token == NULL)? (perror("toke = NULL")):(neighborTable[j].switchID = atoi(token));
			//printf("Client: Neighbor[%i]'s switchID = %i.\n", j, neighborTable[j].switchID);
			
			token=strtok( NULL, s);
			(token == NULL)? (perror("toke = NULL")):(neighborTable[j].swIP = atol(token));//strtoul(token, NULL, 0)
			nbrAddr[j].sin_addr.s_addr=neighborTable[j].swIP;
			//printf("Client: Neighbor[%i]'s IP = %lu.\n", j, neighborTable[j].swIP);
			
			token=strtok( NULL, s);
			(token == NULL)? (perror("toke = NULL")):(neighborTable[j].switchPort = atoi(token));
			nbrAddr[j].sin_port=neighborTable[j].switchPort;
			//printf("Client: Neighbor[%i]'s Port = %i.\n", j, neighborTable[j].switchPort);
			
			token=strtok( NULL, s);
			(token == NULL)? (perror("toke = NULL")):(neighborTable[j].link = atoi(token));
			//printf("Client: Neighbor[%i]'s Link = %i.\n", j, neighborTable[j].link);
			
			token=strtok( NULL, s);
			(token == NULL)? (perror("toke = NULL")):(neighborTable[j].faultLink = atoi(token));
			
			token=strtok( NULL, s);
			(token == NULL)? (perror("toke = NULL")):(neighborTable[j].swActive = atoi(token));	
			//printf("Client: Neighbor[%i]'s Active = %i.\n", j, neighborTable[j].swActive);					
			
			//neighborTable[j].checkPeriod = 0;
			flag[j]=0;// initialize flag
	
	}
	
//Lisa 0206 start
	//set faultLink
	for(j = 0; j < numNeighbor; j++) {
		if(neighborTable[j].switchID == atoi(argv[5])){
			neighborTable[j].faultLink=1;
			printf("Link between %s and %i is disabled\n", argv[3], neighborTable[j].switchID);
				}
	}

//Lisa 0206 end

	
	
	

	tv.tv_sec = KSEC;
 	tv.tv_usec = 500000;
	

	FD_ZERO(&read_fds);
	FD_ZERO(&sw_fds);
	
	
	FD_SET(swSock, &sw_fds);
	//read_fds = sw_fds;
	fdmax = swSock;
	
	while (1)
	{
		read_fds = sw_fds;
		
		
		
		selectValue = select(fdmax+1, &read_fds, NULL, NULL, &tv);
		if(selectValue == -1) {
			perror("select fails!");
			exit(1);
		}
		
		bzero(keepAliveBuffer,20);
		if(selectValue > 0) {
			//printf("Client: select() is OK with value = %i.\n", selectValue);
			for(j = 0; j <= fdmax; j++) {	
				if(FD_ISSET(j, &read_fds)) {
					bzero(recvBuffer,512);
					bzero(responseBuffer,512);
					bzero(tempBuffer,50);
					recvfrom(swSock, recvBuffer, 512,0,(struct sockaddr*)&from, &fromlen);

					token=strtok(recvBuffer, s);
					//puts(token);
					recvHeader=atoi(token);
					switch(recvHeader){
					// receiving and processing neighbors' information from controller 
					case REGISTER_RESPONSE:
						//write(1,"REGISTER_RESPONSE Again?\n",512);//debug
						token=strtok( NULL, s);
						if(strcmp(token, "0") != 0) {
							//printf("Got a wrong REGISTER_RESPONSE refreshing.\n");
							exit(1);
						}
						
						token=strtok( NULL, s);
						
						for(findNBR = 0; findNBR < numNeighbor; findNBR++) {
							if(atoi(token) == neighborTable[findNBR].switchID)
								break;
						}
						printf("Client:Update switch with switchID = %i.\n", atoi(token));
						
						token=strtok( NULL, s);
						(token == NULL)? (perror("toke = NULL")):(neighborTable[findNBR].swIP = atol(token));//strtoul(token, NULL, 0)
						nbrAddr[findNBR].sin_addr.s_addr=(neighborTable[findNBR].swIP);
						printf("Client: Neighbor[%i]'s IP = %lu.\n", findNBR + 1, neighborTable[findNBR].swIP);
						
						token=strtok( NULL, s);
						(token == NULL)? (perror("toke = NULL")):(neighborTable[findNBR].switchPort = atoi(token));
						nbrAddr[findNBR].sin_port=(neighborTable[findNBR].switchPort);
						printf("Client: Neighbor[%i]'s Port = %i.\n", findNBR + 1, neighborTable[findNBR].switchPort);
						
						token=strtok( NULL, s);
						(token == NULL)? (perror("toke = NULL")):(neighborTable[findNBR].link = atoi(token));
						printf("Client: Neighbor[%i]'s Link = %i.\n", findNBR+ 1, neighborTable[findNBR].link);
						
						token=strtok( NULL, s);
						if(token != NULL){
							if(neighborTable[findNBR].faultLink == 0){
								neighborTable[findNBR].faultLink = atoi(token);
							}
						}
						else{
							perror("toke = NULL");
						}
						
						token=strtok( NULL, s);
						(token == NULL)? (perror("toke = NULL")):(neighborTable[findNBR].swActive = atoi(token));
						printf("Client: Neighbor[%i]'s Active = %i.\n", findNBR+ 1, neighborTable[findNBR].swActive);	
						
						// updating routing table length
					case ROUTE_UPDATE:
			//WANG 0131			
						printf("Client:ROUTE_UPDATE from controller.\n");
						//write(1,buffer,512);
						
						token=strtok( NULL, s);
						printf("numSwitch:%s", token);
						numSwitch=atoi(token);
						
						
						for(j = 0; j < numSwitch; j++){
							token=strtok( NULL, s);
							printf("Dest:%s", token);
							routingTable[0][j]=atoi(token);
							token=strtok( NULL, s);
							printf("Next Hop:%s", token);
							routingTable[1][j]=atoi(token);
							printf("Dest:%i  Next Hop:%i\n", routingTable[0][j], routingTable[1][j]);
						}
						
						break;
						
						// mark neighbor active (if neighbor was previously inactive, then also send TOPOLOGY_UPDATE to controller)
					case KEEP_ALIVE:
						token=strtok( NULL, s);
						matchID=atoi(token);//n now is the switch ID from its neighbor
						printf("Client:KEEP_ALIVE from switch %i.\n", matchID);
						for(f = 0; f < numNeighbor; f++){

							if(neighborTable[f].switchID == matchID && neighborTable[f].swActive==0 && neighborTable[f].faultLink!=0){
								neighborTable[f].swActive=1;//set the neighbor switch alive
								sprintf(responseBuffer, "%i/", TOPOLOGY_UPDATE);
								strcat(responseBuffer, mySwitchID);
								for(k = 0; k < numNeighbor; k++) {
										sprintf(tempBuffer, "/%i", neighborTable[k].switchID);
										strcat(responseBuffer, tempBuffer);
										sprintf(tempBuffer, "/%i", neighborTable[k].swActive);
										strcat(responseBuffer, tempBuffer);
										sprintf(tempBuffer, "/%i", neighborTable[k].link);
										strcat(responseBuffer, tempBuffer);
										sprintf(tempBuffer, "/%i", neighborTable[k].faultLink);
										strcat(responseBuffer, tempBuffer);
								}	
								strcat(responseBuffer, END_SIG);

								sendto(swSock, responseBuffer, strlen(responseBuffer), 0, (struct sockaddr*)&server, sizeof(server));//sending topology_update
								//sleep(KSEC);
							}
							if(neighborTable[f].switchID== matchID && flag[f]==0 && neighborTable[f].faultLink!=0){
								flag[f]=1;
							}
						}
						
						break;
					default:
						puts("No header found.\n");

					}
				}
			}	    		

		}
	 	else if (selectValue == 0){
	 		bzero(responseBuffer,512);
	 		//printf("Client: select() timeout.\n");
			if(m==M){//checking if there is any switch that not sending KEEP_ALIVE in k*M seconds and reset m & other counting variables
				//printf("Client: TOPOLOGY_UPDATE with m = %i.\n", m);
				m=0;//reset m
				
				for(f = 0; f < numNeighbor; f++){
					printf("flag %i is %i.\n", neighborTable[f].switchID, flag[f]);
					
					if(flag[f]==0){
						neighborTable[f].link=0;//declare link is down
						sprintf(responseBuffer, "%i/", TOPOLOGY_UPDATE);
						strcat(responseBuffer, mySwitchID);
						for(j = 0; j < numNeighbor; j++) {
								sprintf(tempBuffer, "/ID=%i", neighborTable[j].switchID);
								strcat(responseBuffer, tempBuffer);
								sprintf(tempBuffer, "/Active=%i", neighborTable[j].swActive);
								strcat(responseBuffer, tempBuffer);
								sprintf(tempBuffer, "/link=%i", neighborTable[j].link);
								strcat(responseBuffer, tempBuffer);
								sprintf(tempBuffer, "/fault=%i", neighborTable[j].faultLink);
								strcat(responseBuffer, tempBuffer);
						}	
						strcat(responseBuffer, END_SIG);

						sendto(swSock, responseBuffer, strlen(responseBuffer), 0, (struct sockaddr*)&server, sizeof(server));//sending topo_update that declares link is down
					}
					flag[f]=0;//reset the flag

				}
					
			}
			else{
				m++;
				//printf("Client: TOPOLOGY_UPDATE with m = %i.\n", m);
				// sending TOPO_UPDATE to the server
				
				sprintf(responseBuffer, "%i/", TOPOLOGY_UPDATE);
				strcat(responseBuffer, mySwitchID);
				for(j = 0; j < numNeighbor; j++) {
						sprintf(tempBuffer, "/%i", neighborTable[j].switchID);
						strcat(responseBuffer, tempBuffer);
						sprintf(tempBuffer, "/%i", neighborTable[j].swActive);
						strcat(responseBuffer, tempBuffer);
						sprintf(tempBuffer, "/%i", neighborTable[j].link);
						strcat(responseBuffer, tempBuffer);
						sprintf(tempBuffer, "/%i", neighborTable[j].faultLink);
						strcat(responseBuffer, tempBuffer);
				}	
				strcat(responseBuffer, END_SIG);

				sendto(swSock, responseBuffer, strlen(responseBuffer), 0, (struct sockaddr*)&server, sizeof(server));//sending topo_update that declares link is down
				
				// sending KEEP_ALIVE to its neighbors 
				for(k = 0; k < numNeighbor; k++){
					
					//printf("Client: neighborTable[%i].faultLink = %i.\n", k, neighborTable[k].faultLink);
					if(neighborTable[k].link == 1 && neighborTable[k].faultLink == 0){//check if its neighbor is active so that ip addr is valid
						//write(1, keepAliveBuffer, strlen(keepAliveBuffer));
						bzero(keepAliveBuffer,20);
						sprintf(keepAliveBuffer, "%i/", KEEP_ALIVE);
						strcat(keepAliveBuffer, mySwitchID);
						strcat(keepAliveBuffer, END_SIG);
						sendReturn = sendto(swSock, keepAliveBuffer, strlen(keepAliveBuffer), 0,(struct sockaddr*)&nbrAddr[k], sizeof(nbrAddr[k]));
						
						if(sendReturn < 0)
						{
							//printf("Client: Neighbor %i is currently not available.\n", neighborTable[k].switchID);
							
						} else {
							//printf("Client:Sending KEEP_ALIVE to neighbor: %i.\n", neighborTable[k].switchID);
							
						}

					}
				}
			}
			//reset timeout
			tv.tv_sec = KSEC;
		 	tv.tv_usec = 500000;

			
		  	
		}

		

	
	
	}

	return 1;
}
