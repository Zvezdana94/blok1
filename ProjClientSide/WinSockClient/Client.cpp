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

//#define DEFAULT_PORT 27016
#define CLIENT_SLEEP_TIME 50
#define DEFAULT_BUFLEN 512

#define BUFFER_SIZE 20
#define MASTER_PORT_CLIENT 27019

std::map<int, SOCKET> nodeSockets;

char *list[10];
int id;

#pragma pack(1)
typedef struct {
	int messageSize;
	int clientId;
}MessageHeader;

SOCKET connectMasterSocket = INVALID_SOCKET;


bool InitializeWindowsSockets();

int receiveMessage(SOCKET acceptedSocket, char *recvbuf);
int sendMessage(SOCKET connectSocket, char* messageToSend, int messageLength);
//int waitForSocketAccept(SOCKET acceptedSocket, bool isSend, int sleepTime);
//int waitForSocketAcceptOnce(SOCKET acceptedSocket, bool isSend);

int tryToSelect(SOCKET acceptedSocket, bool isSend, int sleepTime);
int tryToSelectOnce(SOCKET acceptedSocket, bool isSend);

//#pragma pack(1)
//typedef struct{
//	int messageSize;
//	int priority;
//	int clientId;
//}MessageHeader;


//Ovde je problem onaj sto smo mislili da ce biti kod node-ova. Kada se klijent zakaci na master on dobije ip adrese trenutno aktivnih node-ova.
//Posle kad se zakaci jos neki node, taj stari klijent nema uvid u to(treba da skontamo kad da se radi taj "refresh")
DWORD WINAPI getFromMaster(LPVOID lpParam) {
	char ack[4];
	int iResult, success;
	char recvbuf[DEFAULT_BUFLEN];
	//int id = *(int *)lpParam;

	while (1) {
		//iResult = tryToSelectOnce(communicationSocket, false);
		iResult = tryToSelect(connectMasterSocket, false, CLIENT_SLEEP_TIME);

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


			char *list[10];
			int l = 0;
			char *help;
			//printf("Splittin string into tokens:\n");
			help = strtok(ipAdress, "\n");
			while (help != NULL) {
				//printf("%s\n", help);
				list[l] = help;
				l++;

				//treba napraviti da izabere koji node hoce da gadja
				//connectWithNode(help);


				help = strtok(NULL, "\n");
			}
			//printf(ipAdress);
		}
		else if (iResult == 0 || iResult == SOCKET_ERROR) {
			printf("\nReceive message nije uspeo");
			break;
		}
		Sleep(2);
	}
}

int __cdecl main(int argc, char **argv)
{
	HANDLE receiver, sender;
	DWORD receiver_Id, sender_Id;

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

	scanf_s("%d");

	closesocket(connectMasterSocket);
	WSACleanup();

	//SAFE_DELETE_HANDLE(sender);
	//SAFE_DELETE_HANDLE(receiver);

	return 0;
}


//int __cdecl main(int argc,char** argv){
//
//	MessageHeader* msgHeader;
//
//	char *content="this is a test";
//	char *messageToSend;
//	int size=(sizeof(MessageHeader)+strlen(content)+1);
//	
//
//	SOCKET connectSocket=INVALID_SOCKET;
//	int iResult;
//
//	messageToSend=(char*)malloc(size);
//	memset((void*)messageToSend,0,size);
//	msgHeader=(MessageHeader*)messageToSend;
//
//	msgHeader->clientId=1;
//	msgHeader->priority=1;
//	msgHeader->messageSize=size;
//	
//
//	memcpy((messageToSend+sizeof(MessageHeader)),content,strlen(content)+1);
//
//	if(argc != 2){
//		printf("usage: %s server-name\n",argv[0]);
//		return 1;
//	}
//
//	if(InitializeWindowsSockets()==false){
//		return 1;
//	}
//
//	connectSocket=socket(AF_INET,
//						SOCK_STREAM,
//						IPPROTO_TCP);
//
//	if(connectSocket==INVALID_SOCKET){
//		printf("socket failed with error %ld \n",WSAGetLastError());
//		WSACleanup();
//		return 1;
//	}
//
//	sockaddr_in serverAddress;
//	serverAddress.sin_family=AF_INET;
//	serverAddress.sin_addr.s_addr=inet_addr(argv[1]);
//	serverAddress.sin_port=htons(DEFAULT_PORT); // \port podesiti proveriti
//	
//	if(connect(connectSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress))==SOCKET_ERROR){
//		printf("Unable to connect to server.\n");
//		closesocket(connectSocket);
//		WSACleanup();
//	}
//
//	unsigned long int nonBlockingMode=1;
//	iResult=ioctlsocket(connectSocket,FIONBIO,&nonBlockingMode);
//
//	if(iResult==SOCKET_ERROR){
//		printf("ioctlsocket failed with error %ld\n",WSAGetLastError());
//		return 1;
//	}
//
//	if (waitForSocketAccept(connectSocket, true, CLIENT_SLEEP_TIME) == SOCKET_ERROR){
//			return SOCKET_ERROR;
//	}
//
//	iResult=sendMessage(connectSocket,messageToSend,msgHeader->messageSize);	
//
//	scanf_s("%d");
//
//	closesocket(connectSocket);
//	WSACleanup();
//
//	return 0;
//}



bool InitializeWindowsSockets() {
	WSADATA wsaData;
	// Initialize windows sockets library for this process
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		printf("WSAStartup failed with error: %d\n", WSAGetLastError());
		return false;
	}
	return true;
}

//int connectWithNode(char *ipAddr) { //izmeniti ovu metodu da radi sta treba na klijentu(verovatno nije isto kao kod node.cpp)
//	char initialMessage[4];
//	int iResult;
//	int nodeId = -1;
//	int const RECV_BUFFER_SIZE = 4;
//	char recvbuf[RECV_BUFFER_SIZE];
//
//	connectNodeSocket = socket(AF_INET,
//		SOCK_STREAM,
//		IPPROTO_TCP);
//
//	if (connectNodeSocket == INVALID_SOCKET)
//	{
//		printf("socket failed with error: %ld\n", WSAGetLastError());
//		WSACleanup();
//		return 1;
//	}
//
//	sockaddr_in serverAddress;
//	serverAddress.sin_family = AF_INET;
//	serverAddress.sin_addr.s_addr = inet_addr(ipAddr);
//	serverAddress.sin_port = htons(NODE_PORT_CLIENT);
//
//	if (connect(connectNodeSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
//	{
//		printf("Unable to connect to server,error %d.\n", WSAGetLastError());
//		closesocket(connectNodeSocket);
//		WSACleanup();
//	}
//
//	memcpy(initialMessage, (char*)&id, sizeof(int));
//
//	//saljemo id
//	iResult = send(connectNodeSocket, initialMessage, sizeof(int), 0);
//	//napraviti sve provere pre i posle slanja
//
//	//treba da primi kao odg id drugog node-a i da ga doda u mapu
//	//nodeSockets.insert(std::make_pair(nodeId, connectNodeSocket));
//
//	if (tryToSelect(connectNodeSocket, false, SERVER_SLEEP_TIME) == SOCKET_ERROR) {
//		return SOCKET_ERROR;
//	}
//
//	iResult = recv(connectNodeSocket, recvbuf, RECV_BUFFER_SIZE, 0);
//	if (iResult > 0) {
//		nodeId = *(int*)recvbuf;
//		printf("\nConnected with node id:%d", nodeId);
//		nodeSockets.insert(std::make_pair(nodeId, connectNodeSocket));
//	}
//
//	return 0;
//}

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

//int receiveMessage(SOCKET acceptedSocket, char *recvbuf){
//	int iResult;
//	int currLength = 0;
//	int expectedSize=-1;
//	MessageHeader *msgHeader;
//	char *content;
//	
//	do{		
//		// Receive data until the client shuts down the connection
//		iResult = recv(acceptedSocket, recvbuf, DEFAULT_BUFLEN, 0);
//			
//		if (iResult > 0)
//		{
//			currLength+=iResult;
//					
//			msgHeader=(MessageHeader *)recvbuf;
//			expectedSize=msgHeader->messageSize;
//			int i=0;
//			content=recvbuf+sizeof(MessageHeader);
//			for(i;i<(expectedSize-sizeof(MessageHeader));i++){
//				printf("%c",*(content+i));
//			}
//			printf("\n");
//		 }
//		else if (iResult == 0)
//		{
//			// connection was closed gracefully
//			printf("Connection with client closed.\n");
//			closesocket(acceptedSocket);
//			return 0;
//		}
//		else
//		{
//			return 2; //kasnije cemo se baviti ovim, razlikovanje situacija kad drugi server otkaze, ili ne salje poruku...
//
//			//if(currLength>=expectedSize && currLength>0){
//			//	return 2;
//			//}
//			//// there was an error during recv
//			//printf("recv failed with error: %d\n", WSAGetLastError());
//			//closesocket(acceptedSocket);
//			//return SOCKET_ERROR;
//		}
//	}while(currLength < expectedSize);
//	
//	return 1;
//}

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

//int sendMessage(SOCKET connectSocket, char* messageToSend, int messageLength){
//	int iResult = SOCKET_ERROR;
//	int i = 0;
//
//	while (i < messageLength)
//	{
//		if (waitForSocketAccept(connectSocket, true, CLIENT_SLEEP_TIME) == SOCKET_ERROR)
//		{
//			return SOCKET_ERROR;
//		}
//		iResult = send(connectSocket, messageToSend + i, messageLength - i, 0);
//		i += iResult;
//
//		if (iResult == SOCKET_ERROR)
//		{
//			printf("send failed with error: %d\n", WSAGetLastError());
//			closesocket(connectSocket);
//			return SOCKET_ERROR;
//		}
//		else if (iResult == 0)
//		{
//			printf("gracefull shutdown");
//			closesocket(connectSocket);
//			return 0;
//		}
//	}
//	printf("Bytes Sent: %ld\n", iResult);
//	return 1;
//}

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


//int waitForSocketAccept(SOCKET acceptedSocket, bool isSend, int sleepTime){
//	while (1)
//	{
//		FD_SET set;
//		timeval timeVal;
//		FD_ZERO(&set);
//		FD_SET(acceptedSocket, &set);
//		int iResult;
//		timeVal.tv_sec = 0;
//		timeVal.tv_usec = 0;
//
//		if (!isSend)
//			iResult = select(0, &set, NULL, NULL, &timeVal);
//		else
//			iResult = select(0, NULL, &set, NULL, &timeVal);
//
//		if (iResult == SOCKET_ERROR)
//		{
//			fprintf(stderr, "select failed with error: %ld\n", WSAGetLastError());
//			return SOCKET_ERROR;
//		}
//
//		if (iResult == 0)
//		{
//			Sleep(sleepTime);
//			continue;
//		}
//		break;
//	}
//	return 0;
//}
//
//int waitForSocketAcceptOnce(SOCKET acceptedSocket, bool isSend){
//	FD_SET set;
//	timeval timeVal;
//	FD_ZERO(&set);
//	FD_SET(acceptedSocket, &set);
//	int iResult;
//	timeVal.tv_sec = 0;
//	timeVal.tv_usec = 50000;
//
//	if (!isSend)
//		iResult = select(0, &set, NULL, NULL, &timeVal);
//	else
//		iResult = select(0, NULL, &set, NULL, &timeVal);
//
//	if (iResult == SOCKET_ERROR)
//	{
//		fprintf(stderr, "select failed with error: %ld\n", WSAGetLastError());
//		return SOCKET_ERROR;
//	}
//
//	if (iResult == 0)
//	{
//		Sleep(CLIENT_SLEEP_TIME);
//	}
//	else
//	{
//		return 1;
//	}
//	return 0;
//}