#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "connecter.h"

void sendServerSignal(int signal, int ServerSocket) {
	send(ServerSocket, (char*)&signal, sizeof(int), 0); 
}

void connect(Settings * settingsData){
	int ConnectSocket = 0;
	settingsData->ServerSocket = 0;
	struct addrinfo * result = NULL;
	struct addrinfo hints;

	memset(&hints, 0x00, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	printf ("Trying to connect to %s:%d.\n", settingsData->ip, 6000);
	getaddrinfo(settingsData->ip, "6000", &hints, &result);

	for (struct addrinfo * ptr = result; ptr != NULL; ptr = ptr->ai_next) {
		ConnectSocket = socket(AF_INET, SOCK_STREAM, 0);

		if(connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen) != -1) {
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
		send(ConnectSocket, (char *)&settingsData->videoPort, sizeof(int), 0);
		send(ConnectSocket, (char *)&settingsData->audioPort, sizeof(int), 0);
		//settmessagePort = settingsData->messagePort;
		
		char buffer[512];
		sprintf(buffer, "%d", settingsData->messagePort);
		getaddrinfo(settingsData->ip, buffer, &hints, &result);
		
		int newConnectSocket;
		for(struct addrinfo * ptr = result; ptr != NULL; ptr = ptr->ai_next){
			newConnectSocket = socket(AF_INET, SOCK_STREAM, 0);

			if(connect(newConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen) != -1){
				break;
			}
		}
		freeaddrinfo(result);
		
		close(ConnectSocket);

		settingsData->ServerSocket = newConnectSocket;
	}else{
		settingsData->ServerSocket = -1;
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
	if(settingsData->ServerSocket != -1){
		if(isEnoughBandwidth(settingsData)){
			connect(settingsData);
			
			if(settingsData->ServerSocket == -1){
				settingsData->ServerSocket = 0;
				return CONNECTION_ERROR;
			}else{
			}
		}else{
			return RESOURCES_ERROR;
		}
	}
	sendServerSignal(PLAY, settingsData->ServerSocket);
	return 0;
}

void stopStream(Settings * settingsData){
	sendServerSignal(STOP, settingsData->ServerSocket);
	close(settingsData->ServerSocket);
	//ServerSocket = INVALID_SOCKET;
}

void pauseStream(Settings * settingsData){
	sendServerSignal(PAUSE, settingsData->ServerSocket);
}

void resumeStream(Settings * settingsData){
	sendServerSignal(RESUME, settingsData->ServerSocket);
}

void rewindStream(Settings * settingsData){
	sendServerSignal(REWIND, settingsData->ServerSocket);
}

void fastforwardStream(Settings * settingsData){
	sendServerSignal(FAST_FORWARD, settingsData->ServerSocket);
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
		send(settingsData->ServerSocket, (char *)&signal, sizeof(int), 0);

		int newBandwidth = calculateBandwidth(settingsData);
		send(settingsData->ServerSocket, (char *)&newBandwidth, sizeof(int), 0);

		recv(settingsData->ServerSocket, (char *)&signal, sizeof(int), 0);

		if(signal == ACCEPT){
			//The server has enough bandwidth, continue streaming
			return 0;
		}else{
			//The server does not have enough bandwidth, stop streaming
			stopStream(settingsData);

			return CONNECTION_ERROR;
		}
	}else{
		//You do not have enough bandwidth, stop streaming
		stopStream(settingsData);

		return RESOURCES_ERROR;
	}
}

int changeResources(Settings * settingsData){
	//Check if we have enough bandwidth for the stream
	if(isEnoughBandwidth(settingsData)){
		//Check if the server has enough bandwidth for the stream
		int signal = NEW_RESOURCES;
		send(settingsData->ServerSocket, (char *)&signal, sizeof(int), 0);

		int newBandwidth = calculateBandwidth(settingsData);
		send(settingsData->ServerSocket, (char *)&newBandwidth, sizeof(int), 0);

		recv(settingsData->ServerSocket, (char *)&signal, sizeof(int), 0);

		if(signal == ACCEPT){
			//The server has enough bandwidth, continue streaming
			return 0;
		}else{
			//The server does not have enough bandwidth, stop streaming
			stopStream(settingsData);

			return CONNECTION_ERROR;
		}
	}else{
		//You do not have enough bandwidth, stop streaming
		stopStream(settingsData);

		return RESOURCES_ERROR;
	}
}
