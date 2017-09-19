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
#define BUFFER_SIZE 20

#define MASTER_PORT_CLIENT 27017
#define NODE_PORT_CLIENT 27018
#define NODE_PORT_SERVER "27018"
#define CLIENT_PORT_SERVER "27016"
#define CLIENT_SLEEP_TIME 50
#define SERVER_SLEEP_TIME 50

std::map<int, SOCKET> nodeSockets;
std::map<int, HANDLE> nodeReceiverHandle;
std::map<int, HANDLE> nodeSenderHandle;

std::map<int, SOCKET> clientSockets;
std::map<int, HANDLE> clientsReceiverHandle;
std::map<int, HANDLE> clientsSenderHandle;

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

SOCKET listenClientSocket = INVALID_SOCKET;
SOCKET acceptedClientSocket = INVALID_SOCKET;

bool InitializeWindowsSockets();

//int getListFromMaster();
int connectWithNode(char *ipAddr);
int receiveMessage(SOCKET acceptedSocket, char *recvbuf);
int sendMessage(SOCKET connectSocket, char* messageToSend, int messageLength);
int tryToSelect(SOCKET acceptedSocket, bool isSend, int sleepTime);
int tryToSelectOnce(SOCKET acceptedSocket, bool isSend);

DWORD WINAPI getNode(LPVOID lpParam) {
	int iResult;
	char recvbuf[DEFAULT_BUFLEN];
	int nodeId = *(int *)lpParam;
	SOCKET connectSocket;

	printf("\nThread getNode with id: %d started\n", nodeId);

	connectSocket = nodeSockets[nodeId];

	while (1) {

		iResult = tryToSelect(connectSocket, false, SERVER_SLEEP_TIME);

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

		iResult = receiveMessage(connectSocket, recvbuf);
		if (iResult == 1) {
			/*int pr = (*(Message*)recvbuf).priority;
			int idClient = (*(Message*)recvbuf).clientId;

			do {
			success = push(&priorityQueue[pr], recvbuf);
			if (!success) {
			printf("\n Priority queue %d je PUN, ne moze se pushovati poruka", pr);
			Sleep(50);
			}
			} while (!success);

			printf("\nMessage pushed.Id client: %d, priority: %d\n", idClient, pr);*/
		}
		else if (iResult == 0 || iResult == SOCKET_ERROR) {
			printf("\nReceive message nije uspeo");

			/*EnterCriticalSection(&backupQueues_cs);
			auto itQ = backupQueues.find(id);
			if (itQ != backupQueues.end()) {
			cqueue_free(&backupQueues[id]);
			backupQueues.erase(id);
			}
			LeaveCriticalSection(&backupQueues_cs);

			EnterCriticalSection(&clientsSockets_cs);
			auto itCl = clientsSockets.find(id);
			if (itCl != clientsSockets.end()) {
			closesocket(communicationSocket);
			clientsSockets.erase(id);
			}
			LeaveCriticalSection(&clientsSockets_cs);*/
			return 0;
		}
	}
	return 0;
}

DWORD WINAPI getFromClient(LPVOID lpParam) {
	char ack[4];
	int iResult, success;
	char recvbuf[DEFAULT_BUFLEN];
	int id = *(int *)lpParam;
	int i = 0;
	SOCKET communicationSocket;
	int operation;
	char initialMessage[4];

	printf("\nThread getFromClient with id: %d started\n", id);

	communicationSocket = clientSockets[id];
	memcpy(ack, (char*)&id, sizeof(int));

	if (send(communicationSocket, ack, sizeof(int), 0) == SOCKET_ERROR) { //klijent je otkazao
		iResult = closesocket(communicationSocket);
		if (iResult == SOCKET_ERROR) {
			printf("\nClosing socket associated with client %d failed. Error: %d", id, WSAGetLastError());
		}
		//EnterCriticalSection(&clientsSockets_cs);
		clientSockets.erase(id);
		//LeaveCriticalSection(&clientsSockets_cs);
		return 0;
	}

	printf("\nACK was sent to client %d", id);

	while (1) {
		iResult = tryToSelect(communicationSocket, false, SERVER_SLEEP_TIME);

		if (iResult == SOCKET_ERROR) {
			printf("select failed with error: %d\n", WSAGetLastError());
			closesocket(communicationSocket);
			//EnterCriticalSection(&clientsSockets_cs);
			clientSockets.erase(id);
			//LeaveCriticalSection(&clientsSockets_cs);
			WSACleanup();
			printf("\nSocket error return 1");
			return 0;
		}

		iResult = recv(communicationSocket, recvbuf, DEFAULT_BUFLEN, 0);
		if (iResult > 0) {
			operation = *(int*)recvbuf;

			if (operation == 1) {
				printf("\nClient with id %d wants to read.", id);
			}
			else if (operation == 2) {
				printf("\nClient with id %d wants to write.", id);
			}

			memcpy(initialMessage, (char*)&operation, sizeof(int));
			iResult = send(communicationSocket, initialMessage, sizeof(int), 0);
			//napraviti sve provere pre i posle slanja
		}
		else if (iResult == 0) {
			printf("Connection with client closed.\n");
			closesocket(communicationSocket);
		}
		else {
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(communicationSocket);
		}

		//iResult = receiveMessage(communicationSocket, recvbuf);
		//if (iResult == 1) {



		//	/*int pr = (*(Message*)recvbuf).priority;
		//	int idClient = (*(Message*)recvbuf).clientId;

		//	do {
		//		success = push(&priorityQueue[pr], recvbuf);
		//		if (!success) {
		//			printf("\n Priority queue %d je PUN, ne moze se pushovati poruka", pr);
		//			Sleep(50);
		//		}
		//	} while (!success);

		//	printf("\nMessage pushed.Id client: %d, priority: %d\n", idClient, pr);*/
		//}
		//else if (iResult == 0 || iResult == SOCKET_ERROR) {
		//	printf("\nReceive message nije uspeo");

		//	/*EnterCriticalSection(&backupQueues_cs);
		//	auto itQ = backupQueues.find(id);
		//	if (itQ != backupQueues.end()) {
		//		cqueue_free(&backupQueues[id]);
		//		backupQueues.erase(id);
		//	}
		//	LeaveCriticalSection(&backupQueues_cs);

		//	EnterCriticalSection(&clientsSockets_cs);
		//	auto itCl = clientsSockets.find(id);
		//	if (itCl != clientsSockets.end()) {
		//		closesocket(communicationSocket);
		//		clientsSockets.erase(id);
		//	}
		//	LeaveCriticalSection(&clientsSockets_cs);*/
		//	return 0;
		//}
	}

	//treba obraditi: Ako klijent otkaze zatvoriti taj socket i izbrisati iz mape
	return 0;
}

DWORD WINAPI sendToClient(LPVOID lpParam) {
	int iResult;
	int success;
	int id = *(int *)lpParam;
	SOCKET communicationSocket;

	//EnterCriticalSection(&clientsSockets_cs);
	auto itCl = clientSockets.find(id);
	if (itCl != clientSockets.end()) {
		communicationSocket = clientSockets[id];
		//LeaveCriticalSection(&clientSockets_cs);
	}
	else {
		//LeaveCriticalSection(&clientsSockets_cs);
		return 0;
	}

	while (1) {
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

DWORD WINAPI getFromNode(LPVOID lpParam) {
	char ack[4];
	int iResult, success;
	char recvbuf[DEFAULT_BUFLEN];
	int nodeId = *(int *)lpParam;
	int i = 0;
	SOCKET communicationSocket;

	printf("\nThread getFromNode with id: %d started\n", nodeId);

	communicationSocket = nodeSockets[nodeId];
	memcpy(ack, (char*)&id, sizeof(int));

	if (send(communicationSocket, ack, sizeof(int), 0) == SOCKET_ERROR) { //klijent je otkazao
		iResult = closesocket(communicationSocket);
		if (iResult == SOCKET_ERROR) {
			printf("\nClosing socket associated with node %d failed. Error: %d", nodeId, WSAGetLastError());
		}
		//EnterCriticalSection(&clientsSockets_cs);
		nodeSockets.erase(nodeId);
		//LeaveCriticalSection(&clientsSockets_cs);
		return 0;
	}

	printf("\nACK was sent to node %d", nodeId);

	while (1) {
		iResult = tryToSelect(communicationSocket, false, SERVER_SLEEP_TIME);

		if (iResult == SOCKET_ERROR) {
			printf("select failed with error: %d\n", WSAGetLastError());
			closesocket(communicationSocket);
			//EnterCriticalSection(&clientsSockets_cs);
			nodeSockets.erase(nodeId);
			//LeaveCriticalSection(&clientsSockets_cs);
			WSACleanup();
			printf("\nSocket error return 1");
			return 0;
		}

		iResult = receiveMessage(communicationSocket, recvbuf);
		if (iResult == 1) {
			/*int pr = (*(Message*)recvbuf).priority;
			int idClient = (*(Message*)recvbuf).clientId;

			do {
			success = push(&priorityQueue[pr], recvbuf);
			if (!success) {
			printf("\n Priority queue %d je PUN, ne moze se pushovati poruka", pr);
			Sleep(50);
			}
			} while (!success);

			printf("\nMessage pushed.Id client: %d, priority: %d\n", idClient, pr);*/
		}
		else if (iResult == 0 || iResult == SOCKET_ERROR) {
			printf("\nReceive message nije uspeo");

			/*EnterCriticalSection(&backupQueues_cs);
			auto itQ = backupQueues.find(id);
			if (itQ != backupQueues.end()) {
			cqueue_free(&backupQueues[id]);
			backupQueues.erase(id);
			}
			LeaveCriticalSection(&backupQueues_cs);

			EnterCriticalSection(&clientsSockets_cs);
			auto itCl = clientsSockets.find(id);
			if (itCl != clientsSockets.end()) {
			closesocket(communicationSocket);
			clientsSockets.erase(id);
			}
			LeaveCriticalSection(&clientsSockets_cs);*/
			return 0;
		}
		//Sleep(3);
		//}
	}

	//treba obraditi: Ako klijent otkaze zatvoriti taj socket i izbrisati iz mape
	return 0;
}

DWORD WINAPI sendToNode(LPVOID lpParam) {
	int iResult;
	int success;
	int nodeId = *(int *)lpParam;
	SOCKET communicationSocket;

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

DWORD WINAPI getFromMaster(LPVOID lpParam) {
	char ack[4];
	int iResult, success;
	char recvbuf[DEFAULT_BUFLEN];
	//int id = *(int *)lpParam;

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


			char *list[10];
			int l = 0;
			char *help;
			//printf("Splittin string into tokens:\n");
			help = strtok(ipAdress, "\n");
			while (help != NULL) {
				//printf("%s\n", help);
				list[l] = help;
				l++;

				//ovde treba povezati node sa ostalim node-ovima
				connectWithNode(help);

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

		printf("\nNode accepted");

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

			printf("\nNode with id %d accepted", nodeId); //cisto zbog ispisa

			nodeSockets.insert(std::make_pair(nodeId, acceptedNodeSocket));

			memcpy(initialMessage, (char*)&id, sizeof(int));

			//saljemo id
			//iResult = send(acceptedNodeSocket, initialMessage, sizeof(int), 0);
			//napraviti sve provere pre i posle slanja

			nodeReceiver = CreateThread(NULL, 0, &getFromNode, &nodeId, 0, &nodeReceiver_Id);
			nodeSender = CreateThread(NULL, 0, &sendToNode, &nodeId, 0, &nodeSender_Id);

			nodeReceiverHandle.insert(std::make_pair(nodeId, nodeReceiver));
			nodeSenderHandle.insert(std::make_pair(nodeId, nodeSender));
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

DWORD WINAPI listenForClients(LPVOID lpParam) {
	HANDLE clientReceiver, clientSender;
	DWORD clientReceiver_Id, clientSender_Id;

	int const RECV_BUFFER_SIZE = 4;
	char recvbuf[RECV_BUFFER_SIZE];
	int nodeId = -1;
	int clientId = -1;

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

	iResult = getaddrinfo(NULL, CLIENT_PORT_SERVER, &hints, &resultingAddress);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	listenClientSocket = socket(AF_INET,      // IPv4 address famly
		SOCK_STREAM,
		IPPROTO_TCP);

	if (listenClientSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(resultingAddress);
		WSACleanup();
		return 1;
	}


	iResult = bind(listenClientSocket, resultingAddress->ai_addr, (int)resultingAddress->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(resultingAddress);
		closesocket(listenClientSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(resultingAddress);

	iResult = listen(listenClientSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("internal listen failed with error: %d\n", WSAGetLastError());
		closesocket(listenClientSocket);
		WSACleanup();
		return 1;
	}
	printf("Client Server initialized\n");

	//---------------------------ACCEPT--------------------------------
	//-----------------------------------------------------------------

	do {
		printf("\ntrying to accept clients...");

		// accept je blokirajuca operacija, i za pocetak i treba da bude tako
		// ne treba da server radi bilo sta drugo, dok se ne poveze sa drugim serverom
		acceptedClientSocket = accept(listenClientSocket, NULL, NULL);

		if (acceptedClientSocket == INVALID_SOCKET) {
			printf("accept failed with error: %d\n", WSAGetLastError());
			closesocket(listenClientSocket);
			WSACleanup();
			return 1;
		}

		iResult = ioctlsocket(acceptedClientSocket, FIONBIO, &nonBlockingMode);
		if (iResult == SOCKET_ERROR) {
			printf("ioctlsocket failed with error: %ld\n", WSAGetLastError());
			closesocket(acceptedClientSocket);
			WSACleanup();
			return 1;
		}

		/*For connection-oriented sockets, readability(select return 1) can also indicate that a
		request to close the socket has been received from the peer.  dakle ako pozivas select na accepted socketu,
		a sa one strane je ugasen endpoint, select ce ti vracati 1 - kao da ima nesto za citanje*/

		if (tryToSelect(acceptedClientSocket, false, SERVER_SLEEP_TIME) == SOCKET_ERROR) {
			return SOCKET_ERROR;
		}

		iResult = recv(acceptedClientSocket, recvbuf, RECV_BUFFER_SIZE, 0);
		if (iResult > 0) {
			clientId = *(int*)recvbuf;
			printf("\nClient with id %d accepted", clientId);

			clientSockets.insert(std::make_pair(clientId, acceptedClientSocket));

			memcpy(initialMessage, (char*)&id, sizeof(int));

			//???????
			//saljemo id
			//iResult = send(acceptedClientSocket, initialMessage, sizeof(int), 0);
			//napraviti sve provere pre i posle slanja

			clientReceiver = CreateThread(NULL, 0, &getFromClient, &clientId, 0, &clientReceiver_Id);
			clientSender = CreateThread(NULL, 0, &sendToClient, &clientId, 0, &clientSender_Id);

			clientsReceiverHandle.insert(std::make_pair(clientId, clientReceiver));
			clientsSenderHandle.insert(std::make_pair(clientId, clientSender));


		}
		else if (iResult == 0) {
			printf("Connection with node closed.\n");
			closesocket(acceptedClientSocket);
		}
		else {
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(acceptedClientSocket);
		}

		Sleep(500);// neka radi ostatak servera malo...

				   //treba nam logika za izlaz iz petlje ako drugi server otkaze
				   /*if (!otherServerAlive) {
				   break;
				   }*/

				   //ako node pukne, treba da zatvorimo njemu odgovarajuci socket, i da ga izbrisemo iz mapa
	} while (1);

	// ovi shutdowni mogu da padnu, onda radi close socket kao u cleanupu
	iResult = shutdown(acceptedClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(acceptedClientSocket);
		WSACleanup();
		return 1;
	}

	iResult = shutdown(listenClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(listenClientSocket);
		WSACleanup();
		return 1;
	}

	closesocket(acceptedClientSocket);
	closesocket(listenClientSocket);

	return 0;
}

int __cdecl main(int argc, char **argv)
{
	HANDLE receiver, sender, nodeListener, clientListener;
	DWORD receiver_Id, sender_Id, nodeListener_Id, clientListener_Id;

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

	clientListener = CreateThread(NULL, 0, &listenForClients, &id, 0, &clientListener_Id);

	//napraviti neku proveru da li je server ziv,ako nije pozatvarati sve kako treba,cleanup napraviti
	scanf_s("%d");

	closesocket(connectMasterSocket);
	WSACleanup();

	SAFE_DELETE_HANDLE(sender);
	SAFE_DELETE_HANDLE(receiver);
	SAFE_DELETE_HANDLE(nodeListener);
	SAFE_DELETE_HANDLE(clientListener);

	return 0;
}

bool InitializeWindowsSockets()
{
	WSADATA wsaData;
	// Initialize windows sockets library for this process
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
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
	
	if (tryToSelect(connectNodeSocket, false, SERVER_SLEEP_TIME) == SOCKET_ERROR) {
		return SOCKET_ERROR;
	}

	//treba da primi kao odg id drugog node-a i da ga doda u mapu
	iResult = recv(connectNodeSocket, recvbuf, RECV_BUFFER_SIZE, 0);
	if (iResult > 0) {
		nodeId = *(int*)recvbuf;
		printf("\nConnected with node id:%d", nodeId);
		nodeSockets.insert(std::make_pair(nodeId, connectNodeSocket));

		//mislim da ovo ne radi na istom kompu zbog ip adrese i portova, ali da ce raditi na razlicitim kompovima
		/*nodeReceiver = CreateThread(NULL, 0, &getNode, &nodeId, 0, &nodeReceiver_Id);
		nodeSender = CreateThread(NULL, 0, &sendToNode, &nodeId, 0, &nodeSender_Id);

		nodeReceiverHandle.insert(std::make_pair(nodeId, nodeReceiver));
		nodeSenderHandle.insert(std::make_pair(nodeId, nodeSender));*/
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
