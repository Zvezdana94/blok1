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

#define CLIENT_SLEEP_TIME 50
#define DEFAULT_BUFLEN 512

#define BUFFER_SIZE 20
#define MASTER_PORT_CLIENT 27019
#define NODE_PORT_CLIENT 27016

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

bool InitializeWindowsSockets();

int receiveMessage(SOCKET acceptedSocket, char *recvbuf);
int sendMessage(SOCKET connectSocket, char* messageToSend, int messageLength);
//int waitForSocketAccept(SOCKET acceptedSocket, bool isSend, int sleepTime);
//int waitForSocketAcceptOnce(SOCKET acceptedSocket, bool isSend);

int tryToSelect(SOCKET acceptedSocket, bool isSend, int sleepTime);
int tryToSelectOnce(SOCKET acceptedSocket, bool isSend);

int connectWithNode(char *ipAddr);
int getIpAdressesFromMaster();

//#pragma pack(1)
//typedef struct{
//	int messageSize;
//	int priority;
//	int clientId;
//}MessageHeader;

//DWORD WINAPI getFromMaster(LPVOID lpParam) {
//	char ack[4];
//	int iResult, success;
//	char recvbuf[DEFAULT_BUFLEN];
//	//int id = *(int *)lpParam;
//
//	while (1) {
//		//iResult = tryToSelectOnce(communicationSocket, false);
//		iResult = tryToSelect(connectMasterSocket, false, CLIENT_SLEEP_TIME);
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
//			int l = 0;
//			char *help;
//
//			help = strtok(ipAdress, "\n");
//			while (help != NULL) {
//				//printf("%s\n", help);
//				list[l] = help;
//				l++;
//
//				help = strtok(NULL, "\n");
//			}
//		}
//		else if (iResult == 0 || iResult == SOCKET_ERROR) {
//			printf("\nReceive message nije uspeo");
//			break;
//		}
//		Sleep(2);
//	}
//}

DWORD WINAPI sendData(LPVOID lpParam) {
	int a;
	int selectedNode;
	char *nodeIpAddr;
	int node = 1;

	while (1) {
		for (a = 0; a < (sizeof(list) / sizeof(list[0])); a++) {
			if (list[a] != "") {
				printf("\n %d. Node %d \n", node, node);
				node++;
			}
		}

		scanf("%d", &selectedNode);
		nodeIpAddr = list[selectedNode - 1];
		connectWithNode(nodeIpAddr);
	}

	return 0;
}

DWORD WINAPI getNode(LPVOID lpParam) {
	int iResult;
	char recvbuf[DEFAULT_BUFLEN];
	int nodeId = *(int *)lpParam;
	SOCKET connectSocket;
	int operation;

	printf("\nThread getNode with id: %d started\n", nodeId);

	connectSocket = nodeSockets[nodeId];

	while (1) {

		iResult = tryToSelect(connectSocket, false, CLIENT_SLEEP_TIME);

		if (iResult == SOCKET_ERROR) {
			printf("select failed with error: %d\n", WSAGetLastError());
			closesocket(connectSocket);
			//EnterCriticalSection(&clientsSockets_cs);
			nodeSockets.erase(nodeId);
			//LeaveCriticalSection(&clientsSockets_cs);
			WSACleanup();
			printf("\nSocket error return 1");
			return 0;
		}

		iResult = recv(connectSocket, recvbuf, DEFAULT_BUFLEN, 0);
		if (iResult > 0) {
			operation = *(int*)recvbuf;

			if (operation == 1) {
				printf("\nNode with id %d responded to read request.\n", id);
			}
			else if (operation == 2) {
				printf("\nNode with id %d responded to write request.\n", id);
			}
		}
		else if (iResult == 0) {
			printf("Connection with node closed.\n");
			closesocket(connectSocket);
		}
		else {
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(connectSocket);
		}

		//iResult = receiveMessage(connectSocket, recvbuf);
		//if (iResult == 1) {
		//	/*int pr = (*(Message*)recvbuf).priority;
		//	int idClient = (*(Message*)recvbuf).clientId;

		//	do {
		//	success = push(&priorityQueue[pr], recvbuf);
		//	if (!success) {
		//	printf("\n Priority queue %d je PUN, ne moze se pushovati poruka", pr);
		//	Sleep(50);
		//	}
		//	} while (!success);

		//	printf("\nMessage pushed.Id client: %d, priority: %d\n", idClient, pr);*/
		//}
		//else if (iResult == 0 || iResult == SOCKET_ERROR) {
		//	printf("\nReceive message nije uspeo");

		//	/*EnterCriticalSection(&backupQueues_cs);
		//	auto itQ = backupQueues.find(id);
		//	if (itQ != backupQueues.end()) {
		//	cqueue_free(&backupQueues[id]);
		//	backupQueues.erase(id);
		//	}
		//	LeaveCriticalSection(&backupQueues_cs);

		//	EnterCriticalSection(&clientsSockets_cs);
		//	auto itCl = clientsSockets.find(id);
		//	if (itCl != clientsSockets.end()) {
		//	closesocket(communicationSocket);
		//	clientsSockets.erase(id);
		//	}
		//	LeaveCriticalSection(&clientsSockets_cs);*/
		//	return 0;
		//}
	}
	return 0;
}

DWORD WINAPI sendToNode(LPVOID lpParam) {
	int iResult;
	int success;
	int nodeId = *(int *)lpParam;
	SOCKET communicationSocket;
	int operation;
	char initialMessage[4];

	//EnterCriticalSection(&clientsSockets_cs);
	auto itCl = nodeSockets.find(nodeId);
	if (itCl != nodeSockets.end()) {
		communicationSocket = nodeSockets[nodeId];
		//LeaveCriticalSection(&clientSockets_cs);
	}
	else {
		//LeaveCriticalSection(&clientsSockets_cs);
		return 0;
	}

	while (1) {

		printf("\nChoose what do you want to do:");
		printf("\n1. Read");
		printf("\n2. Write\n");
		scanf("%d", &operation);
		memcpy(initialMessage, (char*)&operation, sizeof(int));

		//if (operation == 1) {
		while (1) {
			iResult = tryToSelectOnce(communicationSocket, true);
			if (iResult == SOCKET_ERROR) {
				printf("\nselect failed with error: %d\n", WSAGetLastError());
				auto itCl = nodeSockets.find(id);
				if (itCl != nodeSockets.end()) {
					closesocket(communicationSocket);
					nodeSockets.erase(id);
					WSACleanup();
					return 0;
				}
			}
			else if (iResult == 0) {
				printf("\nthere is no ready buffers for sending...");
				//Sleep(15);
			}
			else if (iResult == 1) {
				iResult = send(communicationSocket, initialMessage, sizeof(int), 0);
				break;
			}
		}
		//}

		//EnterCriticalSection(&backupQueues_cs);
		//auto it = backupQueues.find(id);
		//if (it != backupQueues.end()) {
		//			char *messageToSend = pop(&backupQueues[id], &success);
		//			LeaveCriticalSection(&backupQueues_cs);
		//			if (success) {
		//				while (1) {
		//					iResult = tryToSelectOnce(communicationSocket, true);
		//
		//					if (iResult == SOCKET_ERROR) {
		//						printf("select failed with error: %d\n", WSAGetLastError());
		//						EnterCriticalSection(&clientsSockets_cs);
		//						auto itCl = clientsSockets.find(id);
		//						if (itCl != clientsSockets.end()) {
		//							closesocket(communicationSocket);
		//							clientsSockets.erase(id);
		//						}
		//						LeaveCriticalSection(&clientsSockets_cs);
		//
		//						EnterCriticalSection(&backupQueues_cs);
		//						auto itQ = backupQueues.find(id);
		//						if (itQ != backupQueues.end()) {
		//							cqueue_free(&backupQueues[id]);
		//							backupQueues.erase(id);
		//						}
		//						LeaveCriticalSection(&backupQueues_cs);
		//
		//						WSACleanup();
		//						return 0;
		//					}
		//					else if (iResult == 0) {
		//						printf("\nthere is no ready buffers for sending...");
		//						//Sleep(15);
		//					}
		//					else if (iResult == 1) {
		//						int size = (*(Message *)messageToSend).messageSize;
		//
		//						iResult = sendMessage(communicationSocket, messageToSend, size);
		//						if (iResult == SOCKET_ERROR || iResult == 0) { //pocistiti, pozatvarati sve
		//							printf("\nSending message to client failed");
		//							free(messageToSend);
		//
		//							EnterCriticalSection(&clientsSockets_cs);
		//							auto itCl = clientsSockets.find(id);
		//							if (itCl != clientsSockets.end()) {
		//								closesocket(communicationSocket);
		//								clientsSockets.erase(id);
		//							}
		//							LeaveCriticalSection(&clientsSockets_cs);
		//
		//							EnterCriticalSection(&backupQueues_cs);
		//							auto itQ = backupQueues.find(id);
		//							if (itQ != backupQueues.end()) {
		//								cqueue_free(&backupQueues[id]);
		//								backupQueues.erase(id);
		//							}
		//							LeaveCriticalSection(&backupQueues_cs);
		//
		//							WSACleanup();
		//							return 0;
		//							//break;
		//						}
		//						//else if (iResult == 0 || iResult == 2) { //druga strana ugasila kako treba aplikaciju
		//						//	otherServerAlive = 0;
		//						//	break;
		//						//}
		//						else if (iResult == 1) { //validno
		//							break;
		//						}
		//					}
		//				}
		//				free(messageToSend);
		//
		//				//if (!otherServerAlive) //resiti ovo,sta raditi ako crkne drugi server
		//				//	break; //ili return 1...?			
		//			}
		//		}
		//		else {
		//			LeaveCriticalSection(&backupQueues_cs);
		//			break;
		//		}
		//
		//		//Sleep(50);
	}
	//
	return 0;
}

int __cdecl main(int argc, char **argv)
{
	HANDLE receiver, sender;
	DWORD receiver_Id, sender_Id;

	char initialMessage[4];
	int iResult;

	int a;
	int selectedNode;
	char *nodeIpAddr;

	int k = 0;
	for (k; k < 10; k++) {
		list[k] = "";
	}

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

	iResult = ioctlsocket(connectMasterSocket, FIONBIO, &nonBlockingMode);
	if (iResult == SOCKET_ERROR) {
		printf("ioctlsocket failed with error %ld\n", WSAGetLastError());
		return 1;
	}

	getIpAdressesFromMaster();

	while (1) {
		//Mora ova petlja da ne bi propao dole u ovaj scanf
	}
	

	//receiver = CreateThread(NULL, 0, &getFromMaster, &id, 0, &receiver_Id);

	scanf_s("%d");

	closesocket(connectMasterSocket);
	WSACleanup();

	SAFE_DELETE_HANDLE(sender);
	SAFE_DELETE_HANDLE(receiver);

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

int getIpAdressesFromMaster() {
	char ack[4];
	int iResult, success;
	char recvbuf[DEFAULT_BUFLEN];

	int a;
	int selectedNode;
	char *nodeIpAddr;
	int node = 1;
	//int id = *(int *)lpParam;

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


		//char *list[10];
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
		//break;
	}
	//Sleep(2);
	//}

	for (a = 0; a < (sizeof(list) / sizeof(list[0])); a++) {
		if (list[a] != "") {
			printf("%d. Node %d \n", node, node);
			node++;
		}

		//}			
	}

	scanf("%d", &selectedNode);
	nodeIpAddr = list[selectedNode - 1];
	connectWithNode(nodeIpAddr);

	return 0;
}

int connectWithNode(char *ipAddr) {
	HANDLE nodeReceiver, nodeSender;
	DWORD nodeReceiver_Id, nodeSender_Id;

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


	if (tryToSelect(connectNodeSocket, false, CLIENT_SLEEP_TIME) == SOCKET_ERROR) {
		return SOCKET_ERROR;
	}

	iResult = recv(connectNodeSocket, recvbuf, RECV_BUFFER_SIZE, 0);
	if (iResult > 0) {
		nodeId = *(int*)recvbuf;
		printf("\nConnected with node id:%d", nodeId);
		nodeSockets.insert(std::make_pair(nodeId, connectNodeSocket));

		nodeReceiver = CreateThread(NULL, 0, &getNode, &nodeId, 0, &nodeReceiver_Id);
		nodeSender = CreateThread(NULL, 0, &sendToNode, &nodeId, 0, &nodeSender_Id);
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