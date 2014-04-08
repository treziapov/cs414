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

void connect(char * ip, int mode, int resolution, int rate){
	SOCKET ConnectSocket = INVALID_SOCKET;
	ServerSocket = INVALID_SOCKET;
	struct addrinfo * result = NULL;
	struct addrinfo hints;

	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	iResult = getaddrinfo(ip, "6000", &hints, &result);

	for(struct addrinfo * ptr = result; ptr != NULL; ptr = ptr->ai_next){
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if(iResult != SOCKET_ERROR){
			break;
		}
	}
	freeaddrinfo(result);

	send(ConnectSocket, (char *)&mode, sizeof(int), 0);
	send(ConnectSocket, (char *)&resolution, sizeof(int), 0);
	send(ConnectSocket, (char *)&rate, sizeof(int), 0);

	int signal;
	recv(ConnectSocket, (char *)&signal, sizeof(int), 0);

	if(signal == ACCEPT){
		int port;
		recv(ConnectSocket, (char *)&port, sizeof(int), 0);

		int videoPort;
		recv(ConnectSocket, (char *)&videoPort, sizeof(int), 0);

		int audioPort;
		recv(ConnectSocket, (char *)&audioPort, sizeof(int), 0);
		
		char buffer[512];
		iResult = getaddrinfo(ip, itoa(port, buffer, 10), &hints, &result);
		
		SOCKET newConnectSocket;
		for(struct addrinfo * ptr = result; ptr != NULL; ptr = ptr->ai_next){
			newConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

			iResult = connect(newConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
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

bool isEnoughBandwidth(int mode, int resolution, int rate, int bandwidth){
	int audioBitRate = 0;
		if(mode == ACTIVE){
			audioBitRate = 8000 * 16;
		}

		int bitsPerPixel = 24;

		int videoPixelNumber = 0;
		if(resolution == R240){
			videoPixelNumber = 320 * 240;
		}else{
			videoPixelNumber = 640 * 480;
		}

		int videoRate = rate;

		int neededBandwidth = audioBitRate + bitsPerPixel * videoPixelNumber * videoRate;

		if(bandwidth - neededBandwidth < 0){
			return false;
		}

		return true;
}

int startStream(char * ip, int mode, int resolution, int rate, int bandwidth){
	if(ServerSocket != INVALID_SOCKET){
		if(isEnoughBandwidth(mode, resolution, rate, bandwidth)){
			connect(ip, mode, resolution, rate);
			
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
	send(ServerSocket, (char *)&signal, sizeof(int), 0);

	closesocket(ServerSocket);
	ServerSocket = INVALID_SOCKET;

	WSACleanup();
}

void pauseStream(){
	int signal = PAUSE;
	send(ServerSocket, (char *)&signal, sizeof(int), 0);
}

void resumeStream(){
	int signal = RESUME;
	send(ServerSocket, (char *)&signal, sizeof(int), 0);
}

void rewindStream(){
	int signal = REWIND;
	send(ServerSocket, (char *)&signal, sizeof(int), 0);
}

void fastforwardStream(){
	int signal = FAST_FORWARD;
	send(ServerSocket, (char *)&signal, sizeof(int), 0);
}

void switchMode(){
	int signal = SWITCH_MODE;
	send(ServerSocket, (char *)&signal, sizeof(int), 0);
}

int changeResources(int mode, int resolution, int rate, int newBandwidth){
	//Check if we have enough bandwidth for the stream
	if(isEnoughBandwidth(mode, resolution, rate, newBandwidth)){
		//Check if the server has enough bandwidth for the stream
		int signal = NEW_RESOURCES;
		send(ServerSocket, (char *)&signal, sizeof(int), 0);
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

/*int main(int argc, char *argv[]){
	connect("127.0.0.1", ACTIVE, R240, 15);

	while(true);
}*/




