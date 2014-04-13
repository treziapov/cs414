#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")

#include "connecter.h"

SOCKET ServerSocket;
WSADATA wsaData;
int messagePort;

void connect(Settings * settingsData){
	SOCKET ConnectSocket = INVALID_SOCKET;
	ServerSocket = INVALID_SOCKET;
	struct addrinfo * result = NULL;
	struct addrinfo hints;

	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	iResult = getaddrinfo(settingsData->ip, "6000", &hints, &result);

	for(struct addrinfo * ptr = result; ptr != NULL; ptr = ptr->ai_next){
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if(iResult != SOCKET_ERROR){
			break;
		}
	}
	freeaddrinfo(result);

	send(ConnectSocket, (char *)&settingsData->mode, sizeof(int), 0);
	send(ConnectSocket, (char *)&settingsData->resolution, sizeof(int), 0);
	send(ConnectSocket, (char *)&settingsData->rate, sizeof(int), 0);

	int signal;
	recv(ConnectSocket, (char *)&signal, sizeof(int), 0);

	if(signal == ACCEPT){
		recv(ConnectSocket, (char *)&settingsData->messagePort, sizeof(int), 0);
		recv(ConnectSocket, (char *)&settingsData->videoPort, sizeof(int), 0);
		recv(ConnectSocket, (char *)&settingsData->audioPort, sizeof(int), 0);
		messagePort = settingsData->messagePort;
		
		char buffer[512];
		iResult = getaddrinfo(settingsData->ip, itoa(settingsData->messagePort, buffer, 10), &hints, &result);
		
		SOCKET newConnectSocket;
		for(struct addrinfo * ptr = result; ptr != NULL; ptr = ptr->ai_next){
			newConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

			iResult = connect(newConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
			signal = 1;
			send(newConnectSocket, (char *)&signal, sizeof(int), 0);
			if(iResult != SOCKET_ERROR){
				break;
			}
		}
		freeaddrinfo(result);
		
		closesocket(ConnectSocket);

		ServerSocket = newConnectSocket;
	}else{
		ServerSocket = SOCKET_ERROR;
	}
}

bool isEnoughBandwidth(Settings * settingsData){
	int audioBitRate = 0;
		if(settingsData->mode == ACTIVE){
			audioBitRate = 8000 * 16;
		}

		int bitsPerPixel = 24;

		int videoPixelNumber = 0;
		if(settingsData->resolution == R240){
			videoPixelNumber = 320 * 240;
		}else{
			videoPixelNumber = 640 * 480;
		}

		int videoRate = settingsData->rate;

		int neededBandwidth = audioBitRate + bitsPerPixel * videoPixelNumber * videoRate;

		if(settingsData->bandwidth - neededBandwidth < 0){
			return false;
		}

		return true;
}

int startStream(Settings * settingsData){
	if(ServerSocket != INVALID_SOCKET){
		if(isEnoughBandwidth(settingsData)){
			connect(settingsData);
			
			if(ServerSocket == SOCKET_ERROR){
				ServerSocket = INVALID_SOCKET;
				return CONNECTION_ERROR;
			}
		}else{
			return RESOURCES_ERROR;
		}
	}

	return 0;
}

void stopStream(){
	int signal = STOP;
	send(ServerSocket, (char *)&messagePort, sizeof(int), 0);
	send(ServerSocket, (char *)&signal, sizeof(int), 0);

	closesocket(ServerSocket);
	ServerSocket = INVALID_SOCKET;

	WSACleanup();
}

void pauseStream(){
	fprintf(stderr, "pause\n");
	int signal = PAUSE;
	send(ServerSocket, (char *)&messagePort, sizeof(int), 0);
	send(ServerSocket, (char *)&signal, sizeof(int), 0);
}

void resumeStream(){
	fprintf(stderr, "resume\n");
	int signal = RESUME;
	send(ServerSocket, (char *)&messagePort, sizeof(int), 0);
	send(ServerSocket, (char *)&signal, sizeof(int), 0);
}

void rewindStream(){
	fprintf(stderr, "rewind\n");
	int signal = REWIND;
	send(ServerSocket, (char *)&messagePort, sizeof(int), 0);
	send(ServerSocket, (char *)&signal, sizeof(int), 0);
}

void fastforwardStream(){
	fprintf(stderr, "fast forward\n");
	int signal = FAST_FORWARD;
	send(ServerSocket, (char *)&messagePort, sizeof(int), 0);
	send(ServerSocket, (char *)&signal, sizeof(int), 0);
}

int calculateBandwidth(Settings * settingsData){
	int audioBitRate = 0;
	if(settingsData->mode == ACTIVE){
		audioBitRate = 8000 * 16;
	}

	int bitsPerPixel = 24;

	int videoPixelNumber = 0;
	if(settingsData->resolution == R240){
		videoPixelNumber = 320 * 240;
	}else{
		videoPixelNumber = 640 * 480;
	}

	int videoRate = settingsData->rate;

	int clientBandwidth = audioBitRate + bitsPerPixel * videoPixelNumber * videoRate;

	return clientBandwidth;
}

int switchMode(Settings * settingsData){
	//Check if we have enough bandwidth for the stream
	if(isEnoughBandwidth(settingsData)){
		//Check if the server has enough bandwidth for the stream
		int signal = SWITCH_MODE;
		send(ServerSocket, (char *)&messagePort, sizeof(int), 0);
		send(ServerSocket, (char *)&signal, sizeof(int), 0);

		int newBandwidth = calculateBandwidth(settingsData);
		send(ServerSocket, (char *)&newBandwidth, sizeof(int), 0);

		recv(ServerSocket, (char *)&signal, sizeof(int), 0);

		if(signal == ACCEPT){
			//The server has enough bandwidth, continue streaming
			return 0;
		}else{
			//The server does not have enough bandwidth, stop streaming
			stopStream();

			return CONNECTION_ERROR;
		}
	}else{
		//You do not have enough bandwidth, stop streaming
		stopStream();

		return RESOURCES_ERROR;
	}
}

int changeResources(Settings * settingsData){
	//Check if we have enough bandwidth for the stream
	if(isEnoughBandwidth(settingsData)){
		//Check if the server has enough bandwidth for the stream
		int signal = NEW_RESOURCES;
		send(ServerSocket, (char *)&messagePort, sizeof(int), 0);
		send(ServerSocket, (char *)&signal, sizeof(int), 0);

		int newBandwidth = calculateBandwidth(settingsData);
		send(ServerSocket, (char *)&newBandwidth, sizeof(int), 0);

		recv(ServerSocket, (char *)&signal, sizeof(int), 0);

		if(signal == ACCEPT){
			//The server has enough bandwidth, continue streaming
			return 0;
		}else{
			//The server does not have enough bandwidth, stop streaming
			stopStream();

			return CONNECTION_ERROR;
		}
	}else{
		//You do not have enough bandwidth, stop streaming
		stopStream();

		return RESOURCES_ERROR;
	}
}