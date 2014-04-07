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

void init_listener(int totalBandwidth){
	resource.totalBandwidth = totalBandwidth;
	resource.remainingBandwidth = totalBandwidth;

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

	ZeroMemory(&hints, sizeof(hints));
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
	int audioPort = 8001;
	while(true){
		printf("waiting for a connection\n");
		ClientSocket = accept(ListenSocket, NULL, NULL);
		if(ClientSocket == INVALID_SOCKET){
			printf("accept failed");
			return;
		}
		printf("Connected client on port %d\n", currentPort);

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
		}else{
			//Create the client in the bandwidth table
			createClient(resource, clientBandwidth, currentPort);
			
			//Initialize data to send to new thread
			ThreadData * data = new ThreadData();
			data->mode = newRequest.mode;
			data->resolution = newRequest.resolution;
			data->rate = newRequest.rate;
			data->port = currentPort;

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

void createClient(Resources & resource, int clientBandwidth, int clientPort){
	Client newClient;
	newClient.bandwidth = clientBandwidth;
	newClient.port = clientPort;

	resource.clients.push_back(newClient);
	resource.remainingBandwidth -= clientBandwidth;
}

void handleConnection(void * ptr){
	ThreadData * data = (ThreadData *)ptr;
	
	int videoRate = data->rate;
	int videoResolution = data->resolution;
	int streamMode = data->mode;
	int port = data->port;
	SOCKET ClientSocket = data->ClientSocket;

	//Call function to start streaming data here

	bool endStream = false;
	while(endStream == false){
		int buffer[2]; //0 - Port Number of Client, 1 - Signal
		recv(ClientSocket, (char *)buffer, 2 * sizeof(int), 0);

		if(buffer[1] == STOP){
			//Call function to stop the stream


			for(int i = 0; i < (int)resource.clients.size(); i++){
				if(resource.clients[i].port == buffer[0]){
					resource.remainingBandwidth += resource.clients[i].bandwidth;
					resource.clients.erase(resource.clients.begin() + i);
				}
			}

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
		}else if(buffer[1] == NEW_RESOURCES){
			
		}
	}

	closesocket(ClientSocket);
	_endthread();
}