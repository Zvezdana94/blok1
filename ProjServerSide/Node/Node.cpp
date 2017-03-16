#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <time.h>
#include <map>

#define SAFE_DELETE_HANDLE(a)  if(a){CloseHandle(a);} 

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27016"
#define BUFFER_SIZE 20

#define MASTER_PORT_CLIENT 27017
#define NODE_PORT_CLIENT 27018
#define NODE_PORT_SERVER "27018"
#define CLIENT_SLEEP_TIME 50
#define SERVER_SLEEP_TIME 50

std::map<int, SOCKET> nodeSockets;

char *list[10];
int id;

#pragma pack(1)
typedef struct {
	int messageSize;
	int clientId;
}MessageHeader;

SOCKET connectMasterSocket = INVALID_SOCKET;
SOCKET connectNodeSocket = INVALID_SOCKET;

SOCKET listenNodeSocket = INVALID_SOCKET;
SOCKET acceptedNodeSocket = INVALID_SOCKET;

bool InitializeWindowsSockets();

//int getListFromMaster();
int connectWithNode(char *ipAddr);
int receiveMessage(SOCKET acceptedSocket, char *recvbuf);
int sendMessage(SOCKET connectSocket, char* messageToSend, int messageLength);
int tryToSelect(SOCKET acceptedSocket, bool isSend, int sleepTime);
int tryToSelectOnce(SOCKET acceptedSocket, bool isSend);

DWORD WINAPI getFromMaster(LPVOID lpParam) {
	char ack[4];
	int iResult;
	char recvbuf[DEFAULT_BUFLEN];

	while (1) {
		//iResult = tryToSelectOnce(communicationSocket, false);
		iResult = tryToSelect(connectMasterSocket, false, SERVER_SLEEP_TIME);

		if (iResult == SOCKET_ERROR) {
			printf("select failed with error: %d\n", WSAGetLastError());
			closesocket(connectMasterSocket);
			WSACleanup();
			printf("\nSocket error return 1");
			return 1;
		}

		iResult = receiveMessage(connectMasterSocket, recvbuf);
		if (iResult == 1) {
			char *ipAdress = recvbuf;

			char *help;
			printf("Splittin string into tokens:\n");
			help = strtok(ipAdress, "\n");
			while (help != NULL) {
				printf("%s\n", help);
				//list[l] = help;
				//l++;

				//ovde treba povezati node sa ostalim node-ovima
				connectWithNode(help);
				
				help = strtok(NULL, "\n");
			}
		}
		else if (iResult == 0 || iResult == SOCKET_ERROR) {
			printf("\nReceive message nije uspeo");
			break;
		}
		Sleep(2);
	}
}

DWORD WINAPI listenForNodes(LPVOID lpParam) {
	HANDLE nodeReceiver, nodeSender;
	DWORD nodeReceiver_Id, nodeSender_Id;

	int const RECV_BUFFER_SIZE = 4;
	char recvbuf[RECV_BUFFER_SIZE];
	int nodeId = -1;

	char *messageToSend;
	int size;
	char initialMessage[4];

	// Prepare address information structures
	addrinfo  *resultingAddress = NULL;
	addrinfo  hints;

	int iResult;
	unsigned long int nonBlockingMode = 1;

	//----------------------------LISTEN----------------------------
	//--------------------------------------------------------------

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;       // IPv4 address
	hints.ai_socktype = SOCK_STREAM; // Provide reliable data streaming
	hints.ai_protocol = IPPROTO_TCP; // Use TCP protocol
	hints.ai_flags = AI_PASSIVE;

	iResult = getaddrinfo(NULL, NODE_PORT_SERVER, &hints, &resultingAddress);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	listenNodeSocket = socket(AF_INET,      // IPv4 address famly
		SOCK_STREAM,
		IPPROTO_TCP);

	if (listenNodeSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(resultingAddress);
		WSACleanup();
		return 1;
	}


	iResult = bind(listenNodeSocket, resultingAddress->ai_addr, (int)resultingAddress->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(resultingAddress);
		closesocket(listenNodeSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(resultingAddress);

	iResult = listen(listenNodeSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("internal listen failed with error: %d\n", WSAGetLastError());
		closesocket(listenNodeSocket);
		WSACleanup();
		return 1;
	}
	printf("Node Server initialized\n");

	//---------------------------ACCEPT--------------------------------
	//-----------------------------------------------------------------

	do {
		printf("\ntrying to accept nodes...");

		// accept je blokirajuca operacija, i za pocetak i treba da bude tako
		// ne treba da server radi bilo sta drugo, dok se ne poveze sa drugim serverom
		acceptedNodeSocket = accept(listenNodeSocket, NULL, NULL);

		if (acceptedNodeSocket == INVALID_SOCKET) {
			printf("accept failed with error: %d\n", WSAGetLastError());
			closesocket(listenNodeSocket);
			WSACleanup();
			return 1;
		}

		iResult = ioctlsocket(acceptedNodeSocket, FIONBIO, &nonBlockingMode);
		if (iResult == SOCKET_ERROR) {
			printf("ioctlsocket failed with error: %ld\n", WSAGetLastError());
			closesocket(acceptedNodeSocket);
			WSACleanup();
			return 1;
		}

		/*For connection-oriented sockets, readability(select return 1) can also indicate that a
		request to close the socket has been received from the peer.  dakle ako pozivas select na accepted socketu,
		a sa one strane je ugasen endpoint, select ce ti vracati 1 - kao da ima nesto za citanje*/

		if (tryToSelect(acceptedNodeSocket, false, SERVER_SLEEP_TIME) == SOCKET_ERROR) {
			return SOCKET_ERROR;
		}

		iResult = recv(acceptedNodeSocket, recvbuf, RECV_BUFFER_SIZE, 0);
		if (iResult > 0) {
			nodeId = *(int*)recvbuf;
			printf("\nNode with id %d accepted", nodeId);

			nodeSockets.insert(std::make_pair(nodeId, acceptedNodeSocket));

			memcpy(initialMessage, (char*)&id, sizeof(int));

			//saljemo id
			iResult = send(acceptedNodeSocket, initialMessage, sizeof(int), 0);
			//napraviti sve provere pre i posle slanja
		}
		else if (iResult == 0) {
			printf("Connection with node closed.\n");
			closesocket(acceptedNodeSocket);
		}
		else {
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(acceptedNodeSocket);
		}

		Sleep(500);// neka radi ostatak servera malo...

				   //treba nam logika za izlaz iz petlje ako drugi server otkaze
				   /*if (!otherServerAlive) {
				   break;
				   }*/

				   //ako node pukne, treba da zatvorimo njemu odgovarajuci socket, i da ga izbrisemo iz mapa
	} while (1);

	// ovi shutdowni mogu da padnu, onda radi close socket kao u cleanupu
	iResult = shutdown(acceptedNodeSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(acceptedNodeSocket);
		WSACleanup();
		return 1;
	}

	iResult = shutdown(listenNodeSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(listenNodeSocket);
		WSACleanup();
		return 1;
	}

	closesocket(acceptedNodeSocket);
	closesocket(listenNodeSocket);

	return 0;
}

int __cdecl main(int argc, char **argv)
{
	HANDLE receiver, sender, nodeListener;
	DWORD receiver_Id, sender_Id, nodeListener_Id;

	char initialMessage[4];
	int iResult;
	
	unsigned long int nonBlockingMode = 1;	

	if (argc != 2)
	{
		printf("usage: %s server-name\n", argv[0]);
		return 1;
	}
    
	if (InitializeWindowsSockets() == false)
	{
		return 1;
	}

	connectMasterSocket = socket(AF_INET,
		SOCK_STREAM,
		IPPROTO_TCP);
   
	if (connectMasterSocket == INVALID_SOCKET)
	{
		printf("socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = inet_addr(argv[1]);
	serverAddress.sin_port = htons(MASTER_PORT_CLIENT);

	if (connect(connectMasterSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
	{
		printf("Unable to connect to server,error %d.\n", WSAGetLastError());
		closesocket(connectMasterSocket);
		WSACleanup();
	}

	printf("\nUnesite id: "); //srediti u petlji da postoji validacija da je int
	scanf("%d", &id);
	memcpy(initialMessage, (char*)&id, sizeof(int));

	//saljemo id
	iResult = send(connectMasterSocket, initialMessage, sizeof(int), 0);
	//napraviti sve provere pre i posle slanja

	receiver = CreateThread(NULL, 0, &getFromMaster, &id, 0, &receiver_Id);
	//getListFromMaster();

	nodeListener = CreateThread(NULL, 0, &listenForNodes, &id, 0, &nodeListener_Id);

	//napraviti neku proveru da li je server ziv,ako nije pozatvarati sve kako treba,cleanup napraviti
	scanf_s("%d");

	closesocket(connectMasterSocket);
	WSACleanup();

	SAFE_DELETE_HANDLE(nodeListener);
	SAFE_DELETE_HANDLE(receiver);

	return 0;
}

bool InitializeWindowsSockets()
{
    WSADATA wsaData;
	// Initialize windows sockets library for this process
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
    {
        printf("WSAStartup failed with error: %d\n", WSAGetLastError());
        return false;
    }
	return true;
}

//int getListFromMaster() {
//	char ack[4];
//	int iResult, success;
//	char recvbuf[DEFAULT_BUFLEN];
//	//int id = *(int *)lpParam;
//
//	//while (1) {
//		//iResult = tryToSelectOnce(communicationSocket, false);
//		iResult = tryToSelect(connectMasterSocket, false, SERVER_SLEEP_TIME);
//
//		if (iResult == SOCKET_ERROR) {
//			printf("select failed with error: %d\n", WSAGetLastError());
//			closesocket(connectMasterSocket);
//			WSACleanup();
//			printf("\nSocket error return 1");
//			return 1;
//		}
//
//		iResult = receiveMessage(connectMasterSocket, recvbuf);
//		if (iResult == 1) {
//			char *ipAdress = recvbuf;
//
//
//			
//			int l = 0;
//			char *help;
//			printf("Splittin string into tokens:\n");
//			help = strtok(ipAdress, "\n");
//			while (help != NULL) {
//				printf("%s\n", help);
//				list[l] = help;
//				l++;
//
//				//ovde treba povezati node sa ostalim node-ovima
//
//				help = strtok(NULL, "\n");
//			}
//			//printf(ipAdress);
//		}
//		else if (iResult == 0 || iResult == SOCKET_ERROR) {
//			printf("\nReceive message nije uspeo");
//			return 2;
//			//break;
//		}
//		//Sleep(2);
//	//}
//	return 0;
//}

int connectWithNode(char *ipAddr) {
	char initialMessage[4];
	int iResult;
	int nodeId = -1;
	int const RECV_BUFFER_SIZE = 4;
	char recvbuf[RECV_BUFFER_SIZE];

	connectNodeSocket = socket(AF_INET,
		SOCK_STREAM,
		IPPROTO_TCP);

	if (connectNodeSocket == INVALID_SOCKET)
	{
		printf("socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = inet_addr(ipAddr);
	serverAddress.sin_port = htons(NODE_PORT_CLIENT);

	if (connect(connectNodeSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
	{
		printf("Unable to connect to server,error %d.\n", WSAGetLastError());
		closesocket(connectNodeSocket);
		WSACleanup();
	}

	memcpy(initialMessage, (char*)&id, sizeof(int));

	//saljemo id
	iResult = send(connectNodeSocket, initialMessage, sizeof(int), 0);
	//napraviti sve provere pre i posle slanja

	if (tryToSelect(connectNodeSocket, false, SERVER_SLEEP_TIME) == SOCKET_ERROR) {
		return SOCKET_ERROR;
	}

	iResult = recv(connectNodeSocket, recvbuf, RECV_BUFFER_SIZE, 0);
	if (iResult > 0) {
		nodeId = *(int*)recvbuf;
		printf("\nConnected with node id:%d", nodeId);
		nodeSockets.insert(std::make_pair(nodeId, connectNodeSocket));
	}

	return 0;
}

int receiveMessage(SOCKET acceptedSocket, char *recvbuf)
{
	int iResult;
	int currLength = 0;
	int expectedSize = -1;
	MessageHeader *msgHeader;
	char *content;

	do
	{
		iResult = recv(acceptedSocket, recvbuf, DEFAULT_BUFLEN, 0);

		if (iResult>0)
		{
			currLength += iResult;
			//msgHeader = (MessageHeader *)recvbuf;
			//expectedSize = msgHeader->messageSize;
			printf(recvbuf);
			/*int i = 0;
			content = recvbuf + sizeof(MessageHeader);
			for (i; i<(expectedSize - sizeof(MessageHeader)); i++)
			{
				printf("%c", *(content + i));
			}*/
			printf("\n");
		}
		else if (iResult == 0)
		{
			printf("Connection with client closed gracefully\n");
			closesocket(acceptedSocket);
			return 0;
		}
		else
		{
			closesocket(acceptedSocket);
			return SOCKET_ERROR;
		}

	} while (currLength < expectedSize);

	return 1;
}

int sendMessage(SOCKET connectSocket, char* messageToSend, int messageLength)
{
	int iResult = SOCKET_ERROR;
	int i = 0;

	while (i < messageLength)
	{
		if (tryToSelect(connectSocket, true, CLIENT_SLEEP_TIME) == SOCKET_ERROR)
		{
			return SOCKET_ERROR;
		}
		iResult = send(connectSocket, messageToSend + i, messageLength - i, 0);

		if (iResult == SOCKET_ERROR)
		{
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(connectSocket);
			return SOCKET_ERROR;
		}
		else if (iResult == 0)
		{
			printf("gracefull shutdown");
			closesocket(connectSocket);
			return 0;
		}
		else if (iResult == -1)
		{
			return 2; //srediti
		}
		else
		{
			i += iResult;
			printf("\nMessage send\n");
		}
	}
	return 1;
}

int tryToSelect(SOCKET acceptedSocket, bool isSend, int sleepTime)
{
	while (1)
	{
		FD_SET set;
		timeval timeVal;
		FD_ZERO(&set);
		FD_SET(acceptedSocket, &set);
		int iResult;
		timeVal.tv_sec = 0;
		timeVal.tv_usec = 0;

		if (!isSend)
			iResult = select(0, &set, NULL, NULL, &timeVal);
		else
			iResult = select(0, NULL, &set, NULL, &timeVal);

		if (iResult == SOCKET_ERROR)
		{
			fprintf(stderr, "select failed with error: %ld\n", WSAGetLastError());
			return SOCKET_ERROR;
		}

		if (iResult == 0)
		{
			Sleep(sleepTime);
			continue;
		}
		break;
	}
	return 0;
}

int tryToSelectOnce(SOCKET acceptedSocket, bool isSend)
{
	FD_SET set;
	timeval timeVal;
	FD_ZERO(&set);
	FD_SET(acceptedSocket, &set);
	int iResult;
	timeVal.tv_sec = 0;
	timeVal.tv_usec = 0;

	if (!isSend)
		iResult = select(0, &set, NULL, NULL, &timeVal);
	else
		iResult = select(0, NULL, &set, NULL, &timeVal);
	return iResult;
}
