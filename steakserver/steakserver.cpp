// steakserver.cpp : Defines the entry point for the console application.
//

//#include "stdafx.h"



#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include "json.hpp"
#include "sqlite3.h"

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 1024
#define DEFAULT_PORT "27015"

class databaseResponse {
public:
	databaseResponse() {};

	void addRow(std::vector<std::string> row) {
		entries.push_back(row);
	}
	void printResponse() {
		std::cout << "printing response" << std::endl;
		for (auto it = entries.begin(); it != entries.end(); it++) {
			for (auto iter = it->begin(); iter != it->end(); iter++) {
				std::cout << *iter << ";";
			}
			std::cout << std::endl;
		}
	}

	std::vector<std::vector<std::string>> entries;
};


static int callback(void *pResponse, int argc, char** argv, char **azColName){
	std::cout << "in callback" << std::endl;
	std::vector<std::string> tempRow;
	for (int i = 0; i < argc; i++) {
		tempRow.push_back(std::string(argv[i]));
	}
	static_cast<databaseResponse*>(pResponse)->addRow(tempRow);
	return 0;
}


int sendResponse(SOCKET clientSocket, nlohmann::json outputArray) {
	std::string midstageconversion = outputArray.dump();
	const char * charMatchingUserList = midstageconversion.c_str();
	int iSendResult = send(clientSocket, charMatchingUserList, strlen(charMatchingUserList) + 1, 0);
	if (iSendResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}
	return 0;
}


int main(void)
{

	sqlite3 *db;
	int sqlresult = sqlite3_open("lunchbase.db", &db);
	if (sqlresult == SQLITE_OK) {
		std::cout << "database opened" << std::endl;
	}
	else {
		std::cout << "sqlite error" << sqlite3_errmsg(db) << std::endl;
	}

	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	int iSendResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);
	
	
	
	while (1) {

		iResult = listen(ListenSocket, SOMAXCONN);
		if (iResult == SOCKET_ERROR) {
			printf("listen failed with error: %d\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}

		// Accept a client socket
		ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET) {
			printf("accept failed with error: %d\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}



		// No longer need server socket // pole tõsi
		//closesocket(ListenSocket);

		// Receive until the peer shuts down the connection
		do {
			char recvbuf[DEFAULT_BUFLEN] = { 0 };
			int recvbuflen = DEFAULT_BUFLEN;

			iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
			if (iResult > 0) {
				printf("Bytes received: %d\n", iResult);
				std::string buffer = recvbuf;

				nlohmann::json clientRequest;

				try {
					std::cout << "received data from client" << std::endl;
					std::cout << "---BUFFER_BEGIN---" << std::endl << buffer << std::endl << "---BUFFER_END---" << std::endl << std::endl;
					clientRequest = nlohmann::json::parse(buffer);
					std::cout << clientRequest.dump() << std::endl << std::endl;



					std::string username = clientRequest["username"];
					std::string password = clientRequest["password"];
					std::cout << "client trying to log in... usr:" << username << "; pass:" << password << std::endl;
					databaseResponse userlogin;
					char * errMSG;
					std::string sqlquery = "SELECT * FROM users WHERE password=\'" + password + "\' AND username=\'" + username + "\'";
					sqlite3_exec(db, sqlquery.c_str(), callback, &userlogin, &errMSG);
					if (errMSG != 0) {
						std::cout << "sqlite error:" << errMSG << std::endl;
					}

					std::string userId = userlogin.entries[0][0];

					if (userlogin.entries.size() == 1) {
						std::cout << "login successful!" << std::endl;
						//login andmed õiged

						//1 - login
						//2 - anna inimesed
						//3 - anna kokkulepped
						std::string reqType = clientRequest["type"];
						std::cout << reqType << std::endl;
						std::cout << std::stoi(reqType) << std::endl;
						switch (std::stoi(reqType)) {
							case 1: {
								//saada kliendile "jah"
								sendResponse(ClientSocket, nlohmann::json::array());

								break;
							}

							case 2: {
								//annab andmebaasist kasutaja huvide tabeli
								std::cout << "quering matching users" << std::endl;
								std::string selectUserInterests = "select i.userId, u.username, u.name, u.job, count(i.userId) from interests i, users u where i.interest in(select interest from interests where userId =" + userId + ") and i.userId != " + userId + " and u.id = i.userId group by i.userId order by count(i.userId) desc";
								databaseResponse matchList;
								sqlite3_exec(db, selectUserInterests.c_str(), callback, &matchList, &errMSG);
								if (errMSG != 0) {
									std::cout << "sqlite error:" << errMSG << std::endl;
								}
								std::cout << "matching users" << std::endl;
								matchList.printResponse();
								auto matchingUserList = nlohmann::json::array();
								for (auto iter = matchList.entries.begin(); iter != matchList.entries.end(); iter++) {
									nlohmann::json matchingUser = nlohmann::json::object();
									matchingUser["id"] = iter->at(0);
									matchingUser["name"] = iter->at(2);
									matchingUser["job"] = iter->at(3);
									matchingUserList.push_back(matchingUser);
								}
								if (sendResponse(ClientSocket, matchingUserList)) {
									//probleem saatmisel
									break;
								}

								break;
							}

							default: {

							}

						}
					}
					else {
						//login andmed valed
						//saada kliendile "ei"
						std::cout << "login failed!" << std::endl;
					}
				}
				catch (nlohmann::json::parse_error& error) {
					std::cout << "json error: " << error.what() << ":" << error.id << std::endl;
					std::cout << "ignoring client's request" << std::endl;
				}

			}
			else if (iResult == 0)
				printf("Connection closing...\n");
			else {
				printf("recv failed with error: %d\n", WSAGetLastError());
				closesocket(ClientSocket);
				WSACleanup();
				return 1;
			}

		} while (iResult > 0);
	}

	// shutdown the connection since we're done
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}

	// cleanup
	closesocket(ClientSocket);
	WSACleanup();

	return 0;
}