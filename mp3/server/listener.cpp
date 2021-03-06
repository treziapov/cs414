#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "listener.h"

Resources resource;

char * itoa(int text, char * buffer){
	sprintf(buffer, "%d", text);
	return buffer;
}

void switchMode(ThreadData *data) {
	if (data->mode == ACTIVE) {
		data->mode = PASSIVE;
		data->gstData->mode = (Mode)PASSIVE;
	}
	else {
		data->mode = ACTIVE;
		data->gstData->mode = (Mode)ACTIVE;
	}
}

Client * addClient(Client * newClient){
	resource.numClients++;

	Client ** newArray = new Client *[resource.numClients];

	for(int i = 0; i < resource.numClients - 1; i++){
		newArray[i] = resource.clients[i];
	}

	newArray[resource.numClients - 1] = newClient;

	if(resource.clients != NULL){
		delete[] resource.clients;
	}
	resource.clients = newArray;

	return resource.clients[resource.numClients - 1];
}

void removeClient(int port){
	Client ** newArray = new Client *[resource.numClients - 1];

	int j = 0;
	for(int i = 0; i < resource.numClients; i++){
		if(i != j || resource.clients[i]->port != port){
			newArray[j] = resource.clients[i];
			j++;
		}else{
			resource.remainingBandwidth += resource.clients[i]->bandwidth;
			delete resource.clients[i];
		}
	}

	if(resource.clients != NULL){
		delete[] resource.clients;
	}

	resource.numClients--;
	if(resource.numClients == 0){
		resource.clients = NULL;
	}else{
		resource.clients = newArray;
	}
}

int findClientBandwidth(int port){
	for(int i = 0; i < resource.numClients; i++){
		if(resource.clients[i]->port == port){
			return resource.clients[i]->bandwidth;
		}
	}
	
	return -1;
}

void updateClientBandwidth(int port, int newBandwidth){
	for(int i = 0; i < resource.numClients; i++){
		if(resource.clients[i]->port == port){
			resource.clients[i]->bandwidth = newBandwidth;
			return;
		}
	}
}

Client * findClient(int port){
	for(int i = 0; i < resource.numClients; i++){
		if(resource.clients[i]->port == port){
			return resource.clients[i];
		}
	}
	
	return NULL;
}

void init_listener(int totalBandwidth){
	int basePort = 6000;

	resource.totalBandwidth = totalBandwidth;
	resource.remainingBandwidth = totalBandwidth;
	resource.clients = NULL;
	resource.numClients = 0;
	
	int sock_listener = socket(AF_INET, SOCK_STREAM, 0);
	int sock_client;
	
	struct addrinfo hints, * result;
	result = NULL;
	memset(&hints, 0x00, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	char buffer[512];
	
	int err = getaddrinfo(NULL, itoa(basePort, buffer), &hints, &result);
	if(err != 0){
		printf("getaddrinfo failed\n");
		return;
	}
	
	err = bind(sock_listener, result->ai_addr, result->ai_addrlen);
	if(err == -1){
		printf("bind failed\n");
		return;
	}
	freeaddrinfo(result);
	
	err = listen(sock_listener, 10);
	if(err == -1){
		printf("listen failed\n");
		return;
	}
	
	int currentPort = basePort + 100;
	//int videoPort = basePort + 200;
	//int audioPort = basePort + 300;
	
	while(true){
		
		struct sockaddr_storage client_info;
		//struct sockaddr_in client_info;
		//memset(&client_info,0,sizeof(client_info));
		socklen_t addr_size;
		addr_size = sizeof(client_info);
		printf("Waiting for a connection\n");
		
		sock_client = accept(sock_listener, (struct sockaddr *)&client_info, &addr_size);
		
		if(sock_client == -1){
			printf("accept failed\n");
			return;
		}
		printf("test1\n");
		struct sockaddr_in *sin = (struct sockaddr_in *)&client_info;
		unsigned char * ip = (unsigned char *)&sin->sin_addr.s_addr;
		char * clientIp = (char *)malloc(512);
		sprintf(clientIp, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
		printf("cientIP: %s\n", clientIp);
		
		printf("connected client on: %s\n", clientIp);
		
		Request newRequest;
		recv(sock_client, (char *)&newRequest.mode, sizeof(int), 0);
		recv(sock_client, (char *)&newRequest.resolution, sizeof(int), 0);
		recv(sock_client, (char *)&newRequest.rate, sizeof(int), 0);
		
		int clientBandwidth = calculateResources(resource, newRequest);
		if(clientBandwidth == -1){
			int signal = REJECT;
			send(sock_client, (char *)&signal, sizeof(int), 0);
			close(sock_client);
			printf("client rejected, not enough bandwidth\n");
		}else{
			Client * currentClient = createClient(resource, clientBandwidth, currentPort);
			
			ThreadData * data = new ThreadData();
			data->mode = newRequest.mode;
			data->resolution = newRequest.resolution;
			data->rate = newRequest.rate;
			data->port = currentPort;
			
			data->gstData = &currentClient->gstData;
			data->gstData->clientIp = strdup(clientIp);
			data->gstData->mode = (Mode)newRequest.mode;
			data->gstData->videoFrameRate = newRequest.rate;
			data->gstData->resolution = (Resolution)newRequest.resolution;
			data->client = currentClient;
			
			int signal = ACCEPT;
			send(sock_client, (char *)&signal, sizeof(int), 0);
			
			result = NULL;
			memset(&hints, 0x00, sizeof(hints));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags = AI_PASSIVE;
			
			err = getaddrinfo(NULL, itoa(currentPort, buffer), &hints, &result);
			if(err != 0){
				printf("getaddrinfo failed\n");
				return;
			}
			
			int new_client_socket = socket(AF_INET, SOCK_STREAM, 0);
			
			err = bind(new_client_socket, result->ai_addr, (int)result->ai_addrlen);
			if(err == -1){
				printf("bind failed\n");
				return;
			}
			freeaddrinfo(result);
			
			err = listen(new_client_socket, 10);
			if(err == -1){
				printf("listen failed\n");
			}
			
			send(sock_client, (char *)&currentPort, sizeof(int), 0);
			//send(sock_client, (char *)&videoPort, sizeof(int), 0);
			//send(sock_client, (char *)&audioPort, sizeof(int), 0);
			recv(sock_client, (char *)&data->videoPort, sizeof(int), 0);
			recv(sock_client, (char *)&data->audioPort, sizeof(int), 0);
			
			data->gstData->videoPort = data->videoPort;
			data->gstData->audioPort = data->audioPort;
			
			data->ClientSocket = accept(new_client_socket, NULL, NULL);
			close(new_client_socket);
			
			pthread_t thread;
			pthread_create(&thread, NULL, handleConnection, (void *)data);
			
			currentPort++;
			//videoPort++;
			//audioPort++;
			close(sock_client);
			printf("test2\n");
		}
		
	}
}

int calculateResources(Resources rsc, Request req){
	int audioBitRate = 0;
	if(req.mode == ACTIVE){
		audioBitRate = 8000 * 16;
	}

	int bitsPerPixel = 24;

	int videoPixelNumber = 0;
	if(req.resolution == R240){
		videoPixelNumber = 320 * 240;
	}else{
		videoPixelNumber = 640 * 480;
	}

	int videoRate = req.rate;

	int clientBandwidth = audioBitRate + bitsPerPixel * videoPixelNumber * videoRate;

	if(rsc.remainingBandwidth - clientBandwidth < 0){
		return -1;
	}

	return clientBandwidth;
}

Client * createClient(Resources & resources, int clientBandwidth, int clientPort){
	Client * newClient = new Client();
	newClient->bandwidth = clientBandwidth;
	newClient->port = clientPort;
	
	resources.remainingBandwidth -= clientBandwidth;
	return addClient(newClient);
}

void * handleConnection(void * ptr){
	ThreadData * data = (ThreadData *)ptr;
	
	//int videoRate = data->rate;
	//int videoResolution = data->resolution;
	//int streamMode = data->mode;
	int port = data->port;
	data->gstData->resolution = (Resolution)data->resolution;
	data->gstData->videoFrameRate = data->rate;
	int sock_client = data->ClientSocket;
	
	GstServer::initPipeline(data->gstData);
	GstServer::buildPipeline(data->gstData);
	GstServer::configurePipeline(data->gstData);
	
	bool endStream = false;
	while(endStream == false){
		int signal;
		recv(sock_client, (char *)&signal, sizeof(int), 0);
		
		if(signal == PLAY){
			GstServer::setPipelineToRun(data->gstData);
			pthread_t thread;
			pthread_create(&thread, NULL, GstServer::waitForEosOrError, (void *)data->gstData);
		}else if(signal == STOP){
			endStream = true;
		}else if(signal == PAUSE){
			GstServer::pausePipeline(data->gstData);
		}else if(signal == RESUME){
			GstServer::playPipeline(data->gstData);
		}else if(signal == SWITCH_MODE){
			int newBandwidth;
			recv(sock_client, (char *)&newBandwidth, sizeof(int), 0);
			
			int oldBandwidth = findClientBandwidth(port);
			if(resource.remainingBandwidth - (newBandwidth - oldBandwidth) > 0){
				signal = ACCEPT;
				send(sock_client, (char *)&signal, sizeof(int), 0);
				
				resource.remainingBandwidth -= newBandwidth - oldBandwidth;
				updateClientBandwidth(port, newBandwidth);
				
				switchMode(data);
				GstServer::stopAndFreeResources(data->gstData);
				GstServer::initPipeline(data->gstData);
				GstServer::buildPipeline(data->gstData);
				GstServer::configurePipeline(data->gstData);
				GstServer::playPipeline(data->gstData);
			}else{
				signal = REJECT;
				send(sock_client, (char *)&signal, sizeof(int), 0);
				endStream = true;
			}
		}else if(signal == REWIND){
			GstServer::rewindPipeline(data->gstData);
		}else if(signal == FAST_FORWARD){
			GstServer::fastForwardPipeline(data->gstData);
		}else if(signal == NEW_RESOURCES){
			int newBandwidth;
			recv(sock_client, (char *)&newBandwidth, sizeof(int), 0);
			
			int oldBandwidth = findClientBandwidth(port);
			if(resource.remainingBandwidth - (newBandwidth - oldBandwidth) > 0){
				signal = ACCEPT;
				send(sock_client, (char *)&signal, sizeof(int), 0);
				
				resource.remainingBandwidth -= newBandwidth - oldBandwidth;
				
				updateClientBandwidth(port, newBandwidth);
			}else{
				signal = REJECT;
				send(sock_client, (char *)&signal, sizeof(int), 0);
				
				endStream = false;
			}
		}
	}
	
	GstServer::stopAndFreeResources(data->gstData);
	close(sock_client);
	removeClient(port);
	pthread_exit(NULL);
}
