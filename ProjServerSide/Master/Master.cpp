#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <time.h>
#include <map>

#define SAFE_DELETE_HANDLE(a)  if(a){CloseHandle(a);} 

#define SERVER_SLEEP_TIME 50
#define DEFAULT_BUFLEN 512 //ovo cemo menjati
#define DEFAULT_PORT 27016
#define BUFFER_SIZE 20

#define CLIENT_PORT_SERVER "27019" //za komunikaciju sa klijentom
#define NODE_PORT_SERVER "27017" //za komunikaciju sa node-om

std::map<int, SOCKET> nodeSockets;
std::map<int, HANDLE> nodeReceiverHandle;
std::map<int, HANDLE> nodeSenderHandle; //Da li mi ovo treba?

int num = 0;
char * ipAdressList[10]; //lupila sam broj

SOCKET listenNodeSocket = INVALID_SOCKET;
SOCKET acceptedNodeSocket = INVALID_SOCKET;
SOCKET listenClientSocket = INVALID_SOCKET;
SOCKET acceptedClientSocket = INVALID_SOCKET;

// Initializes WinSock2 library
// Returns true if succeeded, false otherwise.
bool InitializeWindowsSockets();

int receiveMessage(SOCKET acceptedSocket, char *recvbuf);
int sendMessage(SOCKET connectSocket, char* messageToSend, int messageLength);
int tryToSelect(SOCKET acceptedSocket, bool isSend, int sleepTime);
int tryToSelectOnce(SOCKET acceptedSocket, bool isSend);
int addIpAdressToList(SOCKET result);

void cleanup();

#pragma pack(1)
typedef struct {
	int messageSize;
	int clientId;
}MessageHeader;

DWORD WINAPI listenForNodes(LPVOID lpParam) {
	HANDLE nodeReceiver, nodeSender;
	DWORD nodeReceiver_Id, nodeSender_Id;

	int const RECV_BUFFER_SIZE = 4;
	char recvbuf[RECV_BUFFER_SIZE];
	int nodeId = -1;

	char *messageToSend;
	int size;

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

			
			nodeSockets.insert(std::make_pair(nodeId, acceptedNodeSocket));

			int numberOfAdresses = 0;
			int j = 0;
			bool bla = false;
			int messageSize = 10*15; //ovo mora na neki lepsi nacin da se napise,a ne ovako
			messageToSend = (char*)calloc(messageSize, sizeof(char));
			int sizeBefore = 0;
			for (j; j < 10; j++) {
				
				//messageToSend = (char*)calloc(messageSize, sizeof(char));
				int currSize = strlen(ipAdressList[j]) + 1;
				//int messageSize = strlen(ipAdressList[j]) + 1;
				//if (messageSize > 1) {
				if (currSize > 1) {
					memcpy(messageToSend + sizeBefore, ipAdressList[j], strlen(ipAdressList[j]));
					char *help = "\n";
					memcpy(messageToSend + sizeBefore+strlen(ipAdressList[j]), help, strlen(help));
					sizeBefore += strlen(ipAdressList[j]);
					sizeBefore += 1;
					numberOfAdresses++;
					bla = true;

				}
			}

					/*messageToSend = (char*)calloc(messageSize, sizeof(char));
					memcpy(messageToSend, ipAdressList[j], strlen(ipAdressList[j]));*/
			if (bla) {
				while (1) {
					iResult = tryToSelectOnce(acceptedNodeSocket, true);

					if (iResult == SOCKET_ERROR) {
						printf("select failed with error: %d\n", WSAGetLastError());
						closesocket(acceptedNodeSocket);
						WSACleanup();
						return 1;
					}
					else if (iResult == 0) {
						printf("\nthere is no ready buffers for sending...");
						Sleep(10);
					}
					else if (iResult == 1) {
						iResult = sendMessage(acceptedNodeSocket, messageToSend, messageSize);
						if (iResult == SOCKET_ERROR || iResult == 0 || iResult == 2) { //pocistiti, pozatvarati sve
							sizeBefore = 0;
							break;
						}
						else if (iResult == 1) { //validno
							//printf(" %d ", j + 1);
							break;
						}
					}
				}
				free(messageToSend);
				Sleep(30);
			}
				//}
			//}


			addIpAdressToList(acceptedNodeSocket);

			//nodeReceiver = CreateThread(NULL, 0, &getFromClient, &nodeId, 0, &nodeReceiver_Id);
			//nodeSender = CreateThread(NULL, 0, &sendToClient, &nodeId, 0, &nodeSender_Id);

			//nodeReceiverHandle.insert(std::make_pair(nodeId, nodeReceiver));
			//nodeSenderHandle.insert(std::make_pair(nodeId, nodeSender));
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

		printf("\nClient accepted");

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
			nodeId = *(int*)recvbuf;

			//clientSockets.insert(std::make_pair(nodeId, acceptedClientSocket));

			//clientReceiver = CreateThread(NULL, 0, &getFromClient, &nodeId, 0, &clientReceiver_Id);
			//clientSender = CreateThread(NULL, 0, &sendToClient, &nodeId, 0, &clientSender_Id);

			//clientReceiverHandle.insert(std::make_pair(nodeId, clientReceiver));
			//clientSenderHandle.insert(std::make_pair(nodeId, clientSender));
		}
		else if (iResult == 0) {
			printf("Connection with client closed.\n");
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

	closesocket(acceptedNodeSocket);
	closesocket(listenNodeSocket);

	return 0;
}

int __cdecl main(int argc, char **argv) 
{	
	HANDLE clientsListener, nodeListener;
	DWORD clientsListener_Id, nodeListener_Id;

	int i=0;
	int result;
	int iResult;	

	unsigned long int nonBlockingMode = 1;

	MessageHeader *msgHeader;
	char *content;

	int k = 0;
	for (k; k < 10; k++) {
		ipAdressList[k] = "";
	}

    // Validate the parameters
    if (argc != 2)
    {
        printf("usage: %s server-name\n", argv[0]);
        return 1;
    }

    if(InitializeWindowsSockets() == false)
    {
		// we won't log anything since it will be logged
		// by InitializeWindowsSockets() function
		return 1;
    }

	nodeListener = CreateThread(NULL, 0, &listenForNodes, (LPVOID)0, 0, &nodeListener_Id);
	clientsListener = CreateThread(NULL, 0, &listenForClients, (LPVOID)0, 0, &clientsListener_Id);

	/*napraviti ovde neku petlju koja proverava da li treba da zatvori konekcije
	u zavisnosti od statusnih vrednosti koje postavljaju niti, a ticu se socketa...*/
	/*while (1) {
		if (!otherServerAlive) {
			cleanup();
			break;
		}
		Sleep(500);
	}*/

	//cleanup();
	scanf_s("%d");

	SAFE_DELETE_HANDLE(nodeListener);

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

int receiveMessage(SOCKET acceptedSocket, char *recvbuf) {
	int iResult;
	int currLength = 0;
	int expectedSize = -1;
	MessageHeader *msgHeader;
	char *content;

	do {
		iResult = recv(acceptedSocket, recvbuf, DEFAULT_BUFLEN, 0); //mozda cemo morati da uradimo kao i kod sendMessage da bi slao veliku poruku iz delova

		if (iResult > 0) {
			currLength += iResult;
			msgHeader = (MessageHeader *)recvbuf;
			expectedSize = msgHeader->messageSize;
			/*int i = 0;
			content = recvbuf + sizeof(MessageHeader);
			for (i; i < (expectedSize - sizeof(MessageHeader)); i++) {
			printf("%c", *(content + i));
			}
			printf("\n");*/
		}
		else if (iResult == 0) {
			// connection was closed gracefully
			printf("Connection with client closed.\n");
			closesocket(acceptedSocket);
			return 0;
		}
		else {
			//kasnije cemo se baviti ovim, razlikovanje situacija kad drugi server otkaze, ili ne salje poruku...
			closesocket(acceptedSocket);
			printf("\nClient has failed.");
			return SOCKET_ERROR;
		}
	} while (currLength < expectedSize);

	return 1;
}
int sendMessage(SOCKET connectSocket, char* messageToSend, int messageLength) {
	int iResult = SOCKET_ERROR;
	int i = 0;

	while (i < messageLength) {
		if (tryToSelect(connectSocket, true, SERVER_SLEEP_TIME) == SOCKET_ERROR) { //ovo proveriti
			return SOCKET_ERROR;
		}

		iResult = send(connectSocket, messageToSend + i, messageLength - i, 0);

		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(connectSocket);
			return SOCKET_ERROR;
		}
		else if (iResult == 0) {
			printf("gracefull shutdown");
			closesocket(connectSocket);
			return 0;
		}
		if (iResult == -1) { //ovo obraditi, to znaci da nije poslato nista, jer je verovatno druga strana otkazala, iako je select vratio 1

			return 2; //neki kao indikator da je otkaz druge strane...
		}
		else {
			i += iResult;
		}
	}

	return 1;
}
int tryToSelect(SOCKET acceptedSocket, bool isSend, int sleepTime) {
	while (1) {
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

		if (iResult == SOCKET_ERROR) {
			fprintf(stderr, "select failed with error: %ld\n", WSAGetLastError());
			return SOCKET_ERROR;
		}

		if (iResult == 0) {
			Sleep(sleepTime);
			continue;
		}
		break;
	}
	return 0;
}
int tryToSelectOnce(SOCKET acceptedSocket, bool isSend) {
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

int addIpAdressToList(SOCKET result) {
	struct sockaddr_in addr;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	int res = getpeername(result, (struct sockaddr *)&addr, &addr_size);
	char *clientip = new char[20];
	strcpy(clientip, inet_ntoa(addr.sin_addr));
	ipAdressList[num] = clientip;
	num++;

	return 0;
}

void cleanup() {
	//skontati sta ovde treba
	/*int iResult;

	cqueue_free(&queue);
	closesocket(connectSocket);
	closesocket(acceptedSocket);
	closesocket(listenSocket);*/
	WSACleanup();
}
