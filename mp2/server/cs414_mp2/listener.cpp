#undef UNICODE

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <process.h>

#pragma comment(lib, "Ws2_32.lib")

#include "listener.h"

Resources resource;

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
}

void init_listener(int totalBandwidth){
	resource.totalBandwidth = totalBandwidth;
	resource.remainingBandwidth = totalBandwidth;
	resource.clients = NULL;
	resource.numClients = 0;

	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	struct addrinfo * result = NULL;
	struct addrinfo hints;

	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if(iResult != 0){
		printf("WSAStartup failed");
		return;
	}

	memset(&hints, 0x00, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	iResult = getaddrinfo(NULL, "6000", &hints, &result);
	if(iResult != 0){
		printf("getaddrinfo failed");
		return;
	}

	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if(ListenSocket == INVALID_SOCKET){
		printf("socket failed");
		return;
	}

	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if(iResult == SOCKET_ERROR){
		printf("bind failed");
		return;
	}
	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if(iResult == SOCKET_ERROR){
		printf("listen failed");
		return;
	}

	int currentPort = 5001;
	int videoPort = 7001;
	int audioPort = 8001;

	while (true) {
		SOCKADDR_IN clientInfo = {0};
		int addrsize = sizeof(clientInfo);
		char* clientIp = NULL;

		printf("waiting for a connection\n");

		ClientSocket = accept(ListenSocket, (struct sockaddr*)&clientInfo, &addrsize);
		if(ClientSocket == INVALID_SOCKET){
			printf("accept failed");
			return;
		}

		clientIp = inet_ntoa(clientInfo.sin_addr);
		if (strcmp(clientIp, "127.0.0.1") == 0) {
			clientIp = "localhost";
		}
		printf("Connected client on: %s\n", clientIp);

		//Get the desired stream settings from the client
		Request newRequest;
		recv(ClientSocket, (char *)&newRequest.mode, sizeof(int), 0);
		recv(ClientSocket, (char *)&newRequest.resolution, sizeof(int), 0);
		recv(ClientSocket, (char *)&newRequest.rate, sizeof(int), 0);
		printf("mode: %d, resolution: %d, rate: %d\n", newRequest.mode, newRequest.resolution, newRequest.rate);

		int clientBandwidth = calculateResources(resource, newRequest);
		if(clientBandwidth == -1){
			//Not enough bandwidth available, send a reject signal
			int signal = REJECT;
			send(ClientSocket, (char *)&signal, sizeof(int), 0);
			closesocket(ClientSocket);
			printf("client rejected, not enough bandwidth\n");
		}else{
			//Create the client in the bandwidth table
			Client * currentClient = createClient(resource, clientBandwidth, currentPort);
			
			//Initialize data to send to new thread
			ThreadData * data = new ThreadData();
			data->mode = newRequest.mode;
			data->resolution = newRequest.resolution;
			data->rate = newRequest.rate;
			data->port = currentPort;
			data->videoPort = videoPort;
			data->audioPort = audioPort;
			data->gstData = &currentClient->gstData;
			data->gstData->clientIp = strdup(clientIp);
			data->gstData->videoPort = videoPort;
			data->gstData->audioPort = audioPort;
			data->gstData->mode = (Mode)newRequest.mode;
			data->client = currentClient;

			//Send accept signal
			int signal = ACCEPT;
			send(ClientSocket, (char *)&signal, sizeof(int), 0);

			//Create a new connection in a seperate port
			char buffer[512];
			result = NULL;
			memset(&hints, 0x00, sizeof(hints));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;
			hints.ai_flags = AI_PASSIVE;
			
			iResult = getaddrinfo(NULL, itoa(currentPort, buffer, 10), &hints, &result);
			if(iResult != 0){
				printf("getaddrinfo failed");
			}

			SOCKET newListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
			if(newListenSocket == INVALID_SOCKET){
				printf("socket failed");
			}

			iResult = bind(newListenSocket, result->ai_addr, (int)result->ai_addrlen);
			if(iResult == SOCKET_ERROR){
				printf("bind failed");
			}
			freeaddrinfo(result);

			iResult = listen(newListenSocket, SOMAXCONN);
			if(iResult == SOCKET_ERROR){
				printf("listen failed");
				return;
			}

			//Send the new port numbers to the client
			send(ClientSocket, (char *)&currentPort, sizeof(int), 0);
			send(ClientSocket, (char *)&videoPort, sizeof(int), 0);
			send(ClientSocket, (char *)&audioPort, sizeof(int), 0);

			data->ClientSocket = accept(newListenSocket, NULL, NULL);
			closesocket(newListenSocket);

			//Create a thread that will handle everything for the current client
			_beginthread(handleConnection, 0, data);

			currentPort++;
			videoPort++;
			audioPort++;
			closesocket(ClientSocket);
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

Client * createClient(Resources & resource, int clientBandwidth, int clientPort){
	Client * newClient = new Client();
	newClient->bandwidth = clientBandwidth;
	newClient->port = clientPort;
	
	resource.remainingBandwidth -= clientBandwidth;
	return addClient(newClient);
}

void handleConnection(void * ptr){
	printf("connection handler started on a new thread\n");
	ThreadData * data = (ThreadData *)ptr;
	
	int videoRate = data->rate;
	int videoResolution = data->resolution;
	int streamMode = data->mode;
	int port = data->port;
	SOCKET ClientSocket = data->ClientSocket;

	// Start streaming data to client
	printf("connection from: %s\n", data->gstData->clientIp);
	GstServer::initPipeline(data->gstData);
	GstServer::buildPipeline(data->gstData);
	GstServer::configurePipeline(data->gstData);

	bool endStream = false;
	while(endStream == false) {
		int signal;
		recv(ClientSocket, (char*)&signal, sizeof(int), 0);

		if (signal != 0) {
			printf("client signal: %d\n", signal);
		}

		if (signal == PLAY) {
			printf("got PLAY message\n");
			/*GstServer::initPipeline(data->gstData);
			GstServer::buildPipeline(data->gstData);
			GstServer::configurePipeline(data->gstData);*/
			GstServer::setPipelineToRun(data->gstData);
			_beginthread(GstServer::waitForEosOrError, 0, (void *)data->gstData);
		}
		else if (signal == STOP) {
			printf("got STOP message\n");
			GstServer::stopAndFreeResources(data->gstData);
			GstServer::stopPipeline(data->gstData);
			endStream = true;
		}
		else if (signal == PAUSE) {
			printf("got PAUSE message\n");
			GstServer::pausePipeline(data->gstData);
		}
		else if (signal == RESUME) {
			printf("got RESUME message\n");
			GstServer::playPipeline(data->gstData);
		}
		else if (signal == SWITCH_MODE) {
			printf("got SWITCH MODE message\n");
			switchMode(data);
			GstServer::configurePipeline(data->gstData);
		}
		else if (signal == NEW_RESOURCES){
			int newBandwidth;
			recv(ClientSocket, (char *)&newBandwidth, sizeof(int), 0);

			int oldBandwidth = findClientBandwidth(port);
			if(resource.remainingBandwidth - (newBandwidth - oldBandwidth) > 0){
				int signal = ACCEPT;
				send(ClientSocket, (char *)&signal, sizeof(int), 0);

				resource.remainingBandwidth -= newBandwidth - oldBandwidth;

				updateClientBandwidth(port, newBandwidth);
			}else{
				int signal = REJECT;
				send(ClientSocket, (char *)&signal, sizeof(int), 0);

				endStream = false;
			}
		}
	}

	GstServer::stopAndFreeResources(data->gstData);
	closesocket(ClientSocket);
	removeClient(port);
	fprintf(stderr, "Closing connection with client on port %d\n", port);
	_endthread();
}