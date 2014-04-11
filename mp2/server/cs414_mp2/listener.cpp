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

Client* addClient(Client newClient){
	resource.numClients++;

	Client * newArray = new Client[resource.numClients];

	for(int i = 0; i < resource.numClients - 1; i++){
		newArray[i] = resource.clients[i];
	}

	newArray[resource.numClients - 1] = newClient;

	if(resource.clients != NULL){
		delete[] resource.clients;
	}
	resource.clients = newArray;

	return &resource.clients[resource.numClients - 1];
}

void removeClient(int port){
	Client * newArray = new Client[resource.numClients - 1];

	int j = 0;
	for(int i = 0; i < resource.numClients; i++){
		if(i != j || resource.clients[i].port != port){
			newArray[j] = resource.clients[i];
			j++;
		}else{
			resource.remainingBandwidth += resource.clients[i].bandwidth;
		}
	}

	if(resource.clients != NULL){
		delete[] resource.clients;
	}
	resource.clients = newArray;
}

int findClientBandwidth(int port){
	for(int i = 0; i < resource.numClients; i++){
		if(resource.clients[i].port == port){
			return resource.clients[i].bandwidth;
		}
	}
}

void updateClientBandwidth(int port, int newBandwidth){
	for(int i = 0; i < resource.numClients; i++){
		if(resource.clients[i].port == port){
			resource.clients[i].bandwidth = newBandwidth;
			return;
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

	int currentPort = 6001;
	int videoPort = 7001;
	int audioPort = 8001;\

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
			Client *currentClient = createClient(resource, clientBandwidth, currentPort);
			
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

			//Send accept signal
			int signal = ACCEPT;
			send(ClientSocket, (char *)&signal, sizeof(int), 0);

			//Create a new connection in a seperate port
			char buffer[512];
			iResult = getaddrinfo(NULL, itoa(currentPort, buffer, 10), &hints, &result);
			SOCKET newListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
			iResult = bind(newListenSocket, result->ai_addr, (int)result->ai_addrlen);
			freeaddrinfo(result);

			//Send the new port numbers to the client
			send(ClientSocket, (char *)&currentPort, sizeof(int), 0);
			send(ClientSocket, (char *)&videoPort, sizeof(int), 0);
			send(ClientSocket, (char *)&audioPort, sizeof(int), 0);

			data->ClientSocket = accept(newListenSocket, NULL, NULL);

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

Client* createClient(Resources & resource, int clientBandwidth, int clientPort){
	Client newClient;
	newClient.bandwidth = clientBandwidth;
	newClient.port = clientPort;
	
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
	printf("streaming to: %s\n", data->gstData->clientIp);
	GstServer::initPipeline(data->gstData);
	GstServer::buildPipeline(data->gstData);
	GstServer::setPipelineToRun(data->gstData);
	GstServer::waitForEosOrError(data->gstData);

	bool endStream = false;
	while(endStream == false){
		int buffer[2]; //0 - Port Number of Client, 1 - Signal
		recv(ClientSocket, (char *)buffer, 2 * sizeof(int), 0);

		if(buffer[1] == STOP){
			//Call function to stop the stream

			removeClient(buffer[0]);

			endStream = true;
		}else if(buffer[1] == PAUSE){
			//Call pause function
		}else if(buffer[1] == RESUME){
			//Call resume function
		}else if(buffer[1] == REWIND){
			//Call rewind function
		}else if(buffer[1] == FAST_FORWARD){
			//Call fast forward function
		}else if(buffer[1] == SWITCH_MODE){
			//Call function to switch modes

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

				removeClient(port);
				endStream = false;
			}

		}else if(buffer[1] == NEW_RESOURCES){
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

				removeClient(port);
				endStream = false;
			}
		}
	}

	GstServer::stopAndFreeResources(data->gstData);
	closesocket(ClientSocket);
	_endthread();
}