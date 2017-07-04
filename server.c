/* create UDP server */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h> /* Internet Protocol family */
#include <netinet/in.h> /* in_addr structure */
#include <netdb.h> /* hostent struct, gethostbyname() */
#include <stdlib.h>
#include <string.h>
#include "topo_config.h"
#include <unistd.h>
#include <limits.h>

#define KSEC 10
#define MTIMES 3
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

typedef enum
{

REGISTER_REQUEST=1,
REGISTER_RESPONSE=2,
TOPOLOGY_UPDATE=3,
ROUTE_UPDATE=4,
KEP_ALIVE=5

} MSG;

char REG_REP[] = "REGREP";
char ROUTE_UP[] = "ROUTUP";
char REG_REQ[] = "REGREQ";
char TOPO_UP[] = "TOPOUP";
char KEEP_ALIVE[] = "KEPALV";
char END_SIG[] = "/END\n";


int maxBwidthminLeng(int bwidth[], int leng[],  int sptSet[], int V)
{
   // Initialize min value
   int minLeng = INT_MAX;
   int maxBW = 0;
   int min_index, v;
 
   for (v = 0; v < V; v++) {
     if (sptSet[v] == 0) {
    	if(bwidth[v] > maxBW) {
	        maxBW = bwidth[v];
	        min_index = v;
	} else if (bwidth[v] == maxBW) {
		if(leng[v] <= minLeng) {
	        	minLeng = leng[v];
		        min_index = v;
	        }
	}
     }
    }
 
   return min_index;
}

int routingForNextHop(int src, int dst, edgeInfo_t *topoTable, nodeInfo_t nodeTable[], int numSwitch)
{
	int i, u, v, count;
    	int prevSWID;
    	int routeTable[numSwitch];
    	int leng[numSwitch];     // The output array.  dist[i] will hold the shortest
                      // distance from src to i
	int bwidth[numSwitch];
 
	int sptSet[numSwitch]; // sptSet[i] will true if vertex i is included in shortest
                     // path tree or shortest distance from src to i is finalized
    	
    	for (i = 0; i < numSwitch; i++) {
    		routeTable[i] = -1;
    	}
    	
	for (i = 0; i < numSwitch ;i++) {
	        bwidth[i] = 0;
	        leng[i] = INT_MAX;
	        sptSet[i] = 0;
       }
 
     // Distance of source vertex from itself is always 0
	leng[src-1] = 0;
	bwidth[src-1] = INT_MAX;
 
	// Find shortest path for all vertices
	for (count = 0; count < numSwitch; count++) {
		// Pick the minimum distance vertex from the set of vertices not
		// yet processed. u is always equal to src in first iteration.
		u = maxBwidthminLeng(bwidth, leng, sptSet, numSwitch);
		// Mark the picked vertex as processed
		sptSet[u] = 1;

		// Update dist value of the adjacent vertices of the picked vertex.
		for (v = 0; v < numSwitch; v++) {
			if (((topoTable+u*numSwitch)+v)->edge && nodeTable[v].swActive && !(((topoTable+u*numSwitch)+v)->faultLink) && ((topoTable+u*numSwitch)+v)->link) {
				if(bwidth[u] != 0 && (MIN(bwidth[u], ((topoTable+u*numSwitch)+v)->bandwidth) >= bwidth[v]) && leng[u] != INT_MAX && leng[u]+((topoTable+u*numSwitch)+v)->length <= leng[v]) {
					bwidth[v] = MIN(bwidth[u], ((topoTable+u*numSwitch)+v)->bandwidth);
					leng[v] = leng[u]+((topoTable+u*numSwitch)+v)->length;
					routeTable[v] = u;
				} else if(bwidth[u] != 0 && (MIN(bwidth[u], ((topoTable+u*numSwitch)+v)->bandwidth) > bwidth[v])) {
					bwidth[v] = MIN(bwidth[u], ((topoTable+u*numSwitch)+v)->bandwidth);
					leng[v] = leng[u]+((topoTable+u*numSwitch)+v)->length;
					routeTable[v] = u;
				} else
					routeTable[v] = -1;
				
			}
			
		}
	}
	
	v = dst - 1;
	printf("Dst = %i goes back to ", v+1);
	for (count = 0; count < numSwitch; count++) {
		prevSWID = v + 1;
		v = routeTable[v];
		if(v == src - 1) {
			printf(" src = %i.\n", src);
			break;
		}
		else if(routeTable[v] != -1) {
			printf(" %i.\n Then goes back to", v+1);
		} else {
			printf("nowhere due to no path existing.\n");
			return -1;
		}
			
	}
	printf("And next hop for src is: %i.\n", prevSWID);	
	
	for (v=0; v < numSwitch; v++)
	     	printf("From SW%i to %i, length = %i, BW = %i.\n", src, v+1, leng[v], bwidth[v]);
	     	
	return prevSWID;
}



int main(int argc, char *argv[])
{

	int svSock, length, fromlen;
	MSG recvHeader;
	struct sockaddr_in server;
	struct sockaddr_in from;
	char recvBuffer[512];
	struct timeval tv;
	fd_set sv_fds;
	fd_set read_fds;
//Wang 0131 Start	
	FILE *topoFile;
	char *token;
	int numSwitch;
	char topoBuffer[20];
	int unkownValue, srcID, destID;
	int selectValue;
	char responseBuffer[512];
	char tempBuffer[20];
	int recvSWID = 1;
	int recvSWPort = 0;
	int numNBR = 0;
	int j, k, f;
	int dstID;
	int timeIterat = 0;
	int topoChanged = 0;
	int topoChangedPeriod = 0;
	struct sockaddr_in actvNbrAddr;
	

	int i, count, u, v;

	

	if (argc < 2)
	{
		fprintf(stderr, "ERROR, no port provided\n");
		exit(1);
	}
	
	topoFile=fopen("topo.txt","r");
	if(!topoFile)
	{
		printf("Error in opening file");
		exit(1);
	}
	
	//First line of topology file is the number of the switches
	topoConfig tableInputState = TOPO_SRC;
	fgets(topoBuffer, sizeof(topoBuffer), topoFile);
	token=strtok(topoBuffer, " \n");
	numSwitch = atoi(token);
	//printf("# of switches = %i\n", numSwitch);

	edgeInfo_t topoTable[numSwitch][numSwitch];
	nodeInfo_t nodeTable[numSwitch];
	int routingTable[numSwitch][numSwitch];
	int tableNBR[numSwitch];
	int activeNBR[numSwitch];
	int swStatus[numSwitch]; //0 is unchanged. 1 is changed.
	int liveSWFlag[numSwitch];
	
	int leng[numSwitch];     // The output array.  dist[i] will hold the shortest
                      // distance from src to i
	int bwidth[numSwitch];
 
	int sptSet[numSwitch]; // sptSet[i] will true if vertex i is included in shortest
                     // path tree or shortest distance from src to i is finalized

	//topoTable & nodeTable Initialize

	for(j = 0; j < numSwitch; j++) {
		nodeTable[j].switchID = j + 1;
		nodeTable[j].swIP = 0;
		nodeTable[j].switchPort = 0;
		nodeTable[j].swActive = 1;
		for(k = 0; k < numSwitch; k++) {
			topoTable[j][k].edge = 0;
			topoTable[j][k].faultLink = 0;
			topoTable[j][k].link = 0;
			topoTable[j][k].length = 0;
			topoTable[j][k].bandwidth = 0;
			routingTable[j][k] = -1;

		}
	}
	
	for(j = 0; j < numSwitch; j++) {
		tableNBR[j] = 0;
		swStatus[j] = 0;
		activeNBR[j] = -1;
		liveSWFlag[j]=0;//1 is alive, 0 is dead
	}

	//Adjancency Matrix for Topology 
	while(fgets(topoBuffer, sizeof(topoBuffer), topoFile) != NULL)
	{
		tableInputState=TOPO_SRC;	
		//printf("%s", topoBuffer);
		token=strtok(topoBuffer, " \n");
		//printf("%s= ", token);
	
		if(token!=NULL) {
			unkownValue = atoi(token);
			//printf("%i\n", unkownValue);
		}
	
		srcID = unkownValue - 1;
		tableInputState = TOPO_DEST;
		//printf("%i\n", srcID);
	
		while(token != NULL){
			token=strtok(NULL, " \n");
			//printf("%s= ", token);
	
			if(token!=NULL) {
				unkownValue = atoi(token);
				//printf("unkownValue = %i\n", unkownValue);
		

				switch(tableInputState) {
						case TOPO_SRC:
							srcID = unkownValue - 1;
							tableInputState = TOPO_DEST;

							break;
						case TOPO_DEST:
							destID = unkownValue - 1;
							topoTable[srcID][destID].edge = 1;
							topoTable[destID][srcID].edge = 1;
							topoTable[srcID][destID].link = 1;
							topoTable[destID][srcID].link = 1;
							tableInputState = TOPO_BW;

							break;
						case TOPO_LEN:
							topoTable[srcID][destID].length = unkownValue;
							topoTable[destID][srcID].length = unkownValue;
							tableInputState = TOPO_SRC;

							break;
						case TOPO_BW:
							topoTable[srcID][destID].bandwidth = unkownValue;
							topoTable[destID][srcID].bandwidth = unkownValue;
							tableInputState = TOPO_LEN;
							break;
						default:
							tableInputState = -1;
							perror("tableInputState fails");
							exit;
				}
			}

		}

	}

	//Display the Adjancency Matrix of Topology
	printf("Adjancency Matrix from the Topology File: Edge/BandWidth/Length\n");
	for(j = 0; j < numSwitch; j++)
		printf("%12i", j+1);
	printf("\n");

	for(j = 0; j < numSwitch; j++) {
		printf("%-i", j+1);
		printf("%10i/%i/%i", topoTable[j][0].edge, topoTable[j][0].bandwidth, topoTable[j][0].length);
		for(k = 1; k < numSwitch; k++) {
			printf("%6i/%i/%i", topoTable[j][k].edge, topoTable[j][k].bandwidth, topoTable[j][k].length);
		}
		printf("\n");
	}


	fclose(topoFile);
//Wang 0131 End
	
	/*creating socket*/
	svSock=socket(AF_INET, SOCK_DGRAM, 0);
	if (svSock < 0)
	{
		perror("Error in opening socket");
		exit(1);
	} 
	
	/*binding socket*/
	length = sizeof(server);
	bzero(&server, length);
	server.sin_family=AF_INET;
	server.sin_addr.s_addr=INADDR_ANY;
	server.sin_port=htons(atoi(argv[1]));
	if(bind(svSock, (struct sockaddr *)&server, length)<0)
	{
		perror("Error in binding");
		exit(1);
	}


	fromlen = sizeof(struct sockaddr_in);
	bzero(recvBuffer,512);
//Wang 0131 Start-2	
	bzero(&actvNbrAddr, sizeof(actvNbrAddr));
	actvNbrAddr.sin_family=AF_INET;
	
	bzero(responseBuffer,512);
	bzero(tempBuffer,20);
	
	int fdmax;
	FD_ZERO(&read_fds);
	FD_ZERO(&sv_fds);
	
	FD_SET(svSock, &sv_fds);
	fdmax = svSock;
	tv.tv_sec = KSEC;
	tv.tv_usec = 500000;
		 	
	while(1)
	{
	  	read_fds = sv_fds;
		
		selectValue = select(fdmax+1, &read_fds, NULL, NULL, &tv);
		if(selectValue == -1) {
			perror("select fails!");
			exit(1);
		}
		
		bzero(responseBuffer,512);
		bzero(tempBuffer,20);
		bzero(recvBuffer,512);
		
		if(selectValue > 0) {
			printf("Server: select() is OK.\n");
			for(j = 0; j <= fdmax; j++) {	
				if(FD_ISSET(j, &read_fds)) {

					recvfrom(svSock, recvBuffer, 512,0,(struct sockaddr*)&from, &fromlen);
					//puts(recvBuffer);
					token=strtok(recvBuffer, "/");
					recvHeader=atoi(token);
					
					switch(recvHeader){
					//receiving REGISTER_REQUEST-->send pck REGISTER_RESPONSE
					case REGISTER_REQUEST:
						token=strtok(NULL, "/");
						recvSWID = atoi(token);
						token=strtok(NULL, "/");
						recvSWPort = atoi(token);
						
						nodeTable[recvSWID-1].switchID = recvSWID;
						nodeTable[recvSWID-1].swIP = from.sin_addr.s_addr;
						nodeTable[recvSWID-1].switchPort = from.sin_port;
						nodeTable[recvSWID-1].swActive = 1;
						//memcpy(&nodeTable[recvSWID - 1].addr, &from, sizeof(struct sockaddr_in));
						
						printf("Server: Entering REGISTER_RESPONSE for ID = %i.\n", recvSWID);
						for( k = 0; k < numSwitch; k++) {
							if((recvSWID - 1) != k) {
								if(topoTable[recvSWID - 1][k].edge == 1) {
									tableNBR[numNBR] = k;
									
									if(nodeTable[k].swActive == 1) {
										activeNBR[numNBR] = k;
									}
									printf("Server: tableNBR[%i] = %i.\n",numNBR, tableNBR[numNBR]);
									numNBR++;
								}
							}
						}
					
						//Preparing the packet content of REGISTER_REQUEST
						sprintf(responseBuffer,"%i",REGISTER_RESPONSE);
						sprintf(tempBuffer, "/%i", numNBR);
						strcat(responseBuffer, tempBuffer);
						
						for(k = 0; k < numNBR; k++) {

							//sprintf(tempBuffer, "/%i", nodeTable[tableNBR[k]].switchID);
							sprintf(tempBuffer, "/%i", tableNBR[k] + 1);
							strcat(responseBuffer, tempBuffer);
							
							sprintf(tempBuffer, "/%i", nodeTable[tableNBR[k]].swIP);
							strcat(responseBuffer, tempBuffer);
							sprintf(tempBuffer, "/%i", nodeTable[tableNBR[k]].switchPort);
							strcat(responseBuffer, tempBuffer);
							sprintf(tempBuffer, "/%i", topoTable[recvSWID - 1][tableNBR[k]].edge);
							strcat(responseBuffer, tempBuffer);
							sprintf(tempBuffer, "/%i", topoTable[recvSWID - 1][tableNBR[k]].faultLink);
							strcat(responseBuffer, tempBuffer);
							sprintf(tempBuffer, "/%i", nodeTable[tableNBR[k]].swActive);
							strcat(responseBuffer, tempBuffer);
							
							
						}	
						strcat(responseBuffer, END_SIG);
						
						sendto(svSock,responseBuffer, strlen(responseBuffer),0,(struct sockaddr *)&from, fromlen);
						
						//Preparing the new switch info for its neighbors
						bzero(responseBuffer,512);
						bzero(tempBuffer,20);
						
						for(k = 0; k < numNBR; k++) {
							if(activeNBR[k] > -1) {
								sprintf(responseBuffer,"%i",REGISTER_RESPONSE);
								sprintf(tempBuffer, "/%i", 0);
								strcat(responseBuffer, tempBuffer);
	
								sprintf(tempBuffer, "/%i", recvSWID);
								strcat(responseBuffer, tempBuffer);
								sprintf(tempBuffer, "/%i", nodeTable[recvSWID-1].swIP);
								strcat(responseBuffer, tempBuffer);
								sprintf(tempBuffer, "/%i", nodeTable[recvSWID-1].switchPort);
								strcat(responseBuffer, tempBuffer);
								sprintf(tempBuffer, "/%i", topoTable[activeNBR[k]][recvSWID-1].edge);
								strcat(responseBuffer, tempBuffer);
								sprintf(tempBuffer, "/%i", topoTable[activeNBR[k]][recvSWID-1].faultLink);
								strcat(responseBuffer, tempBuffer);
								sprintf(tempBuffer, "/%i", nodeTable[recvSWID-1].swActive);
								strcat(responseBuffer, tempBuffer);
													
								strcat(responseBuffer, END_SIG);
								
								actvNbrAddr.sin_addr.s_addr=(nodeTable[activeNBR[k]].swIP);
								actvNbrAddr.sin_port=(nodeTable[activeNBR[k]].switchPort);
								
								sendto(svSock,responseBuffer, strlen(responseBuffer),0,(struct sockaddr *)&actvNbrAddr, sizeof(actvNbrAddr));
								
								bzero(&actvNbrAddr, sizeof(actvNbrAddr));
								actvNbrAddr.sin_family=AF_INET;
								bzero(responseBuffer,512);
								bzero(tempBuffer,20);
							}
						}
						
						for(k = 0; k < numSwitch; k++) {
							tableNBR[k] = 0;
							activeNBR[k] = -1;
						}
							
						numNBR = 0 ;
					
						break;
			
						//receiving TOPO_UPDATE-->if needed, compute new routing path, also keep track of 
					case TOPOLOGY_UPDATE:		
						token=strtok(NULL,"/");//read switch ID 
						recvSWID=atoi(token);
						printf("swID:%i\n",recvSWID);
						token=strtok(NULL,"/");//read NBR_ID

						while(token!=NULL){
							if(strcmp(token, "END\n") == 0)
								break;
							
							for(k = 0; k < numSwitch; k++){
								
								if(nodeTable[k].switchID==atoi(token)){
									token=strtok(NULL,"/");//read NBR_ACTIVE

									if(nodeTable[k].swActive!=atoi(token)){
										puts("swActive change, now routing\n");//routing
										topoChanged++;
										nodeTable[k].swActive=atoi(token);//updating nodeTable
										token=strtok(NULL,"/");//read NBR_link
										token=strtok(NULL,"/");//read NBR_link
	
									}
									else{
										token=strtok(NULL,"/");//read NBR_link
									
										if(topoTable[recvSWID-1][k].link!=atoi(token)){
											printf("topoTable.link %i and %i change, now routing\n",nodeTable[k].switchID, recvSWID);//routing
											topoChanged++;
											topoTable[recvSWID-1][k].link=atoi(token);//updating topoTable link
											topoTable[k][recvSWID-1].link=atoi(token);//updating topoTable link
											token=strtok(NULL,"/");//read NBR_link
										}
										else{
											token=strtok(NULL,"/");//read NBR_faultlink
											
											if(topoTable[recvSWID-1][k].faultLink!=atoi(token)){
												printf("topoTable.faultlink %i and %i change, now routing\n", nodeTable[k].switchID, recvSWID);//routing
												topoChanged++;
												topoTable[recvSWID-1][k].faultLink=atoi(token);//updating topoTable faultlink
												topoTable[k][recvSWID-1].faultLink=atoi(token);//updating topoTable faultlink
											}
											
										}
									}
								}
					
							}
							token=strtok(NULL,"/");//read NBR_ID

						}
						if(topoChanged > 0) {
							printf("Path Recalculation.\n");	
							//DO ROUTING HERE
							for(srcID = 0; srcID < numSwitch; srcID++) {
								for(dstID = 0; dstID < numSwitch; dstID++)
								routingTable[srcID][dstID] = routingForNextHop(srcID, dstID, (edgeInfo_t *)topoTable, nodeTable, numSwitch);
							}
							//Sending routing table for every switch
							bzero(responseBuffer,512);
							bzero(tempBuffer,20);
							for(srcID = 0; srcID < numSwitch; srcID++) {
								sprintf(responseBuffer,"%i",ROUTE_UPDATE);
								sprintf(tempBuffer, "/%i", numSwitch);
								strcat(responseBuffer, tempBuffer);
								for(dstID = 0; dstID < numSwitch; dstID++) {
									sprintf(tempBuffer, "/%i", dstID+1);
									strcat(responseBuffer, tempBuffer);
									sprintf(tempBuffer, "/%i", routingTable[srcID][dstID]);
									strcat(responseBuffer, tempBuffer);
								}									
								strcat(responseBuffer, END_SIG);
								
								actvNbrAddr.sin_addr.s_addr=nodeTable[srcID].swIP;
								actvNbrAddr.sin_port=nodeTable[srcID].switchPort;
								
								sendto(svSock,responseBuffer, strlen(responseBuffer),0,(struct sockaddr *)&actvNbrAddr, sizeof(actvNbrAddr));
								
								bzero(&actvNbrAddr, sizeof(actvNbrAddr));
								actvNbrAddr.sin_family=AF_INET;
								bzero(responseBuffer,512);
								bzero(tempBuffer,20);
							}
							
						}
						liveSWFlag[recvSWID-1]=1;//record which switch has sent topo_update in M*k sec
						
						for(k=0;k<numSwitch;k++)
							printf("flag of switch%i is %i\n",k+1,liveSWFlag[k]);
						
						topoChanged = 0;
						break;

					default:
						perror("Unspecified Header!!!");			


					}
				}
			}
		}
		//when reaching M*K sec, check which switch doesn't send TOPO_UPDATE, if needed, then reroute 
		else if (selectValue == 0) {
			//printf("Server: Select timeout.\n");
			if(timeIterat == MTIMES) {
				printf("Server: Now Checking Topology.\n");

				for(j = 1 ; j < numSwitch ; j++) {
					if(liveSWFlag[j] == 0)
						topoChangedPeriod++;
				}
				
				if(topoChangedPeriod > 0) {
					printf("Server: Now Sending New Routing (TBD).\n");//DO routing
					for(srcID = 0; srcID < numSwitch; srcID++) {
						for(dstID = 0; dstID < numSwitch; dstID++)
						routingTable[srcID][dstID] = routingForNextHop(srcID, dstID, (edgeInfo_t *)topoTable, nodeTable, numSwitch);
					}
					//Sending routing table for every switch
					bzero(responseBuffer,512);
					bzero(tempBuffer,20);
					for(srcID = 0; srcID < numSwitch; srcID++) {
						sprintf(responseBuffer,"%i",ROUTE_UPDATE);
						sprintf(tempBuffer, "/%i", numSwitch);
						strcat(responseBuffer, tempBuffer);
						for(dstID = 0; dstID < numSwitch; dstID++) {
							sprintf(tempBuffer, "/%i", dstID+1);
							strcat(responseBuffer, tempBuffer);
							sprintf(tempBuffer, "/%i", routingTable[srcID][dstID]);
							strcat(responseBuffer, tempBuffer);
						}									
						strcat(responseBuffer, END_SIG);
						
						actvNbrAddr.sin_addr.s_addr=nodeTable[srcID].swIP;
						actvNbrAddr.sin_port=nodeTable[srcID].switchPort;
						
						sendto(svSock,responseBuffer, strlen(responseBuffer),0,(struct sockaddr *)&actvNbrAddr, sizeof(actvNbrAddr));
						
						bzero(&actvNbrAddr, sizeof(actvNbrAddr));
						actvNbrAddr.sin_family=AF_INET;
						bzero(responseBuffer,512);
						bzero(tempBuffer,20);
					}	
				} else {
					printf("Server: Nothing Changed in Topology.\n");
				}
				
				for(k=0;k<numSwitch;k++) {
					liveSWFlag[k] = 0;
					printf("flag of switch%i is reseted as %i\n",k+1,liveSWFlag[k]);
				}
				
				topoChangedPeriod = 0;
				timeIterat = 0;
				
			}
			
			
			
			
			timeIterat++;
			//Reset the timeoute value, tv
			tv.tv_sec = KSEC;
		 	tv.tv_usec = 500000;




		}
		
	
	}



	return 1;

}
