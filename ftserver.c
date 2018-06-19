/*
** Name: Matthew Toro
** Class: CS 372 Computer Networking
** Assignment: Project 2
** Due Date: 6/3/2018
** Description: A server program that creates control and data connections for simple file transfers.
*/

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>

// function prototypes
void sendMessage(int socketFD, char * clientMessage, int fileSize);
void receiveMessage(int socketFD, char* readBuffer, char* serverMessage);
int startup(int listenSocketFD, struct sockaddr_in* serverAddress, int portNum);
void handleRequest(int connectionFD, int controlPortNum);
void copyDirectoryContents(char * message);
int searchDirectory(char * fileName);
void readFromFile(FILE * streamFD, char * message);
void shutdownSockets(int serverSocket, int connectionSocket, struct sockaddr_in* dataAddress, char* serverMessage);
int validatePortNumber(char * portNum);

// global constants
const int MAX_CHARS = 2048;
const int MAX_FILE_SIZE = 157286400; // about 150 MB

int main(int argc, char* argv[])
{
	if (validatePortNumber(argv[1]) == 1)
	{
		printf("Port number is not a valid integer. Please try again with a valid integer.\n");
		exit(2);
	}
	int controlPortNum = atoi(argv[1]);
	struct sockaddr_in* controlAddress = malloc(sizeof(struct sockaddr_in));
	int controlSocketFD = startup(controlSocketFD, controlAddress, controlPortNum);
	
	// begin listening
	printf("Server open on port:%d\n", controlPortNum);
	listen(controlSocketFD, 10);
   
   
   while(1)
   {
		printf("*****************************\n");
		printf("Awaiting connection from client. Press Ctrl-C to end server.\n");
		
		// accept a connection; if not valid, send a message to stderr; borrowed from my program 4 CS 344 Operating Systems
		struct sockaddr_in clientControlAddress; 
		socklen_t sizeOfControlClientInfo = sizeof(clientControlAddress); 
		int controlConnectionFD = accept(controlSocketFD, (struct sockaddr *)&clientControlAddress, &sizeOfControlClientInfo);
		if (controlConnectionFD < 0)
		{
		   perror("Error: error on accept");
		}
		else
		{	
			handleRequest(controlConnectionFD, controlPortNum);
			printf("Control Connection terminated by client\n");
		}
   }
}

/* 
** Name: sendMessage
** Parameters: socket file descriptor, c-string for message, file size
** Output: nothing unless an error occurs
** Description: sends a message through the passed in socket; 
	if file size is specified (non-zero), then send file size followed by file contents;
	borrowed from my CS 344 assignment 4 program and CS 372 project 1
references:
https://stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c
*/
void sendMessage(int socketFD, char * message, int fileSize)
{
	// this if statement is used for shorter messages
	if (fileSize == 0)
	{
		// add terminating sequence to string
		strcat(message, "@@\0"); 
	   // set up the send loop, we want to ensure we send all the bytes to the server
	   int totalBytes = (int) strlen(message);
	   int currentBytes = 0;
	   int leftoverBytes = totalBytes;
	   while(currentBytes < totalBytes)
	   {
			ssize_t bytesSent;
			bytesSent = send(socketFD, message + currentBytes, leftoverBytes, 0);
			if (bytesSent < 0)
			{
			   free(message);
			   error("ERROR: error writing to socket", 2);
			}
			currentBytes += (int) bytesSent;
			leftoverBytes -= (int) bytesSent;
	   }
	}
	else
	{
		// This else statement is used for longer messages/files
		// convert an int to a string; reference located above
		int length = snprintf(NULL, 0, "%d", fileSize);
		char * temp = malloc(length + 1);
		snprintf(temp, length + 1, "%d", fileSize);

		char * fileSizeMessage = malloc(length + 3);
		memset(fileSizeMessage, '\0', sizeof(fileSizeMessage));
		strcpy(fileSizeMessage, temp);
		strcat(fileSizeMessage, "@@\n");
		
		// send file size message first
		int totalBytes = (int) strlen(fileSizeMessage);
	   int currentBytes = 0;
	   int leftoverBytes = totalBytes;
	   while(currentBytes < totalBytes)
	   {
			ssize_t bytesSent;
			bytesSent = send(socketFD, fileSizeMessage + currentBytes, leftoverBytes, 0);
			if (bytesSent < 0)
			{
			   free(fileSizeMessage);
			   error("ERROR: error writing to socket", 2);
			}
			currentBytes += (int) bytesSent;
			leftoverBytes -= (int) bytesSent;
	   }

	   // send file contents now
		totalBytes = fileSize;
	   currentBytes = 0;
	   leftoverBytes = totalBytes;
	   while(currentBytes < totalBytes)
	   {
			ssize_t bytesSent;
			bytesSent = send(socketFD, message + currentBytes, leftoverBytes, 0);
			if (bytesSent < 0)
			{
			   free(message);
			   error("ERROR: error writing to socket", 2);
			}
			currentBytes += (int) bytesSent;
			leftoverBytes -= (int) bytesSent;
	   }
	   
	   free(fileSizeMessage);
	   free(temp);
	}
}

/* 
** Name: receiveMessage
** Parameters: socket file descriptor, c-strings for a buffer and c-string for message
** Output: message
** Description: receives a message from the connection as described in the socket file descriptor;
	borrowed from my CS 344 assignment 4 program and CS 372 project 1
*/
void receiveMessage(int socketFD, char* readBuffer, char* message)
{
	int charsRead;
	
	// I use a terminating sequence of "@@" to know when the message is over if it gets segmented
	while(strstr(message, "@@") == NULL)
	{
		memset(readBuffer, '\0', sizeof(readBuffer));
		charsRead = recv(socketFD, readBuffer, sizeof(readBuffer), 0);
		if (charsRead < 0)
		{
		   fprintf(stderr, "ERROR: error receiving data\n");
		   break;
		}
		strcat(message, readBuffer);
	}
	
	// get rid of terminating sequence '@@'
	int terminalLocation = strstr(message, "@@") - message;
	message[terminalLocation] = '\n';
	message[terminalLocation + 1] = '\0';
}

/* 
** Name: startup
** Parameters: socket file descriptor, server address struct pointer, port number
** Output: integer for server socket file descriptor
** Description: does all the boilerplate code to set up a socket for server listening;
	borrowed from my program 4 assignment in CS 344 Operating Systems;
references:
https://hea-www.harvard.edu/~fine/Tech/addrinuse.html
https://beej.us/guide/bgnet/html/multi/setsockoptman.html
https://stackoverflow.com/questions/24194961/how-do-i-use-setsockoptso-reuseaddr?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
*/
int startup(int listenSocketFD, struct sockaddr_in* serverAddress, int portNum)
{  
   serverAddress->sin_family = AF_INET;
   serverAddress->sin_port = htons(portNum);
   serverAddress->sin_addr.s_addr = INADDR_ANY;

   // create the socket
   listenSocketFD = socket(AF_INET, SOCK_STREAM, 0);
   if (listenSocketFD < 0)
   {
	perror("Error: error opening socket");
	exit(2);
   }
   
   // avoid connection refused and address in use errors
   // code based on references above
	int optval = 1;
	setsockopt(listenSocketFD, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
	setsockopt(listenSocketFD, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
   
   // bind the socket
   if (bind(listenSocketFD, (struct sockaddr *)serverAddress, sizeof(*serverAddress)) < 0)
   {
	perror("Error: error on binding");
	exit(2);
   }
   return listenSocketFD;
}

// 
/* 
** Name: handleRequest
** Parameters: socket file descriptor, control connection port number
** Output: sends data across data or control connection as applicable
** Description: main function of the program of handling a command sent by the client
references:
listed in comments in the function where applicable
*/
void handleRequest(int connectionFD, int controlPortNum)
{
	char clientMessage[MAX_CHARS];
	char readBuffer[100];
	memset(clientMessage, '\0', sizeof(clientMessage));
	receiveMessage(connectionFD, readBuffer, clientMessage);
	
	// this if statement checks if the command is valid, otherwise the else statement gets executed instead
	if (clientMessage[0] == '-' && (clientMessage[1] == 'l' || clientMessage[1] == 'g'))
	{
		// the client will send the command along with additional parameters in the format:
		// command \n data_port \n host_name \n file_name
		// I use strtok to separate each parameter for use
		char * command;
		char * data_port;
		char * host_name;
		char * fileName;
		command = strtok(clientMessage, "\n"); // this is the command
		host_name = strtok(NULL, "\n"); // this is the host name
		data_port = strtok(NULL, "\n"); // this is the data port
		fileName = strtok(NULL, "\n"); // this is the file name if applicable
		int dataPortNum = atoi(data_port);
		printf("Control Connection established with %s on port:%d\n", host_name, controlPortNum);
		
		if (fileName != NULL && searchDirectory(fileName) == 0)
		{
			// this if statement handles the case where '-g' is sent but the file name is found
			printf("File \"%s\" requested on port:%d\n", fileName, dataPortNum);
			printf("File not found. Sending error message to %s:%d\n", host_name, controlPortNum);
			char * serverMessage = malloc(MAX_CHARS * sizeof(char));
			memset(serverMessage, '\0', sizeof(serverMessage));
			strcpy(serverMessage, "ERROR: FILE NOT FOUND");
			sendMessage(connectionFD, serverMessage, 0);
			free(serverMessage);
		}
		else
		{
			// if message is '-l', get directory listing and send it
			if(strcmp(command, "-l") == 0)
			{
				// start server data connection for transfer
				struct sockaddr_in* dataAddress = malloc(sizeof(struct sockaddr_in));
				int dataSocketFD = startup(dataSocketFD, dataAddress, dataPortNum);
				listen(dataSocketFD, 10);
				
				// accept the client data connection
				struct sockaddr_in clientDataAddress; 
				socklen_t sizeOfDataClientInfo = sizeof(clientDataAddress); 
				int dataConnectionFD = accept(dataSocketFD, (struct sockaddr *)&clientDataAddress, &sizeOfDataClientInfo);
				if (dataConnectionFD < 0)
				{
				   perror("Error: error on accept");
				}
				printf("Data Connection established with %s on port:%d\n", host_name, dataPortNum);
				printf("List directory requested on port:%d\n", dataPortNum);
				char * serverMessage = malloc(MAX_CHARS * sizeof(char));
				memset(serverMessage, '\0', sizeof(serverMessage));
				strcpy(serverMessage, "");
				
				// get directory listing
				copyDirectoryContents(serverMessage);
				printf("Sending directory contents to %s:%d\n", host_name, dataPortNum);
				sendMessage(dataConnectionFD, serverMessage, 0);
				shutdownSockets(dataConnectionFD, dataSocketFD, dataAddress, serverMessage);
				printf("Data connection with %s:%d terminated by server\n", host_name, dataPortNum);
			}
			else
			{
				// file transfer: need to send a prelim message so the client does not stall
				printf("File \"%s\" requested on port:%d\n", fileName, dataPortNum);
				char * serverMessage = malloc(MAX_CHARS * sizeof(char));
				memset(serverMessage, '\0', sizeof(serverMessage));
				strcpy(serverMessage, "File found. Beginning Transfer.");
				sendMessage(connectionFD, serverMessage, 0);
				
				// start server data connection for transfer
				struct sockaddr_in* dataAddress = malloc(sizeof(struct sockaddr_in));
				int dataSocketFD = startup(dataSocketFD, dataAddress, dataPortNum);
				listen(dataSocketFD, 10);
				
				// accept the client data connection
				struct sockaddr_in clientDataAddress; 
				socklen_t sizeOfDataClientInfo = sizeof(clientDataAddress); 
				int dataConnectionFD = accept(dataSocketFD, (struct sockaddr *)&clientDataAddress, &sizeOfDataClientInfo);
				if (dataConnectionFD < 0)
				{
				   perror("Error: error on accept");
				}
				printf("Data Connection established with %s on port:%d\n", host_name, dataPortNum);
				
				// open the passed in file and read its contents
				//borrowed from my CS 344 assignment 4 program
				FILE * transferFileFD;
				char* transferTextStr = malloc(MAX_FILE_SIZE * sizeof(char));
				memset(transferTextStr, '\0', sizeof(transferTextStr));
				transferFileFD = fopen(fileName, "r");
				
				// get file size
				//reference: https://stackoverflow.com/questions/238603/how-can-i-get-a-files-size-in-c
				fseek(transferFileFD, 0, SEEK_END); // seek to end of file
				int fileSize = ftell(transferFileFD); // get current file pointer
				fseek(transferFileFD, 0, SEEK_SET); // seek back to beginning of file
				
				readFromFile(transferFileFD, transferTextStr);
				fclose(transferFileFD);

				printf("Sending \"%s\" to %s:%d\n", fileName, host_name, dataPortNum);
				sendMessage(dataConnectionFD, transferTextStr, fileSize);
				shutdownSockets(dataConnectionFD, dataSocketFD, dataAddress, transferTextStr);
				printf("Data connection with %s:%d terminated by server\n", host_name, dataPortNum);
			}	
		}
	}
	else
	{
		// if message is invalid, send error message
		char * serverMessage = malloc(MAX_CHARS * sizeof(char));
		memset(serverMessage, '\0', sizeof(serverMessage));
		strcpy(serverMessage, "ERROR: unsupported command");
		sendMessage(connectionFD, serverMessage, 0);
		free(serverMessage);
	}
}

/* 
** Name: copyDirectoryContents
** Parameters: c-string for message
** Output: directory contents are copied to c-string
** Description: reads the files in current directory and saves each file name to passed in message
references:
https://stackoverflow.com/questions/20265328/readdir-beginning-with-dots-instead-of-files
*/
void copyDirectoryContents(char * message)
{
	DIR* currentDirectory;
	struct dirent* directoryPointer;
	char* fileNameInDir;
	currentDirectory = opendir(".");
	while( (directoryPointer = readdir(currentDirectory)) != NULL)
	{
		if ( strcmp(directoryPointer->d_name, ".") == 0 || strcmp(directoryPointer->d_name, "..") == 0) 
		{
			// skip "." and ".." in directory structure
		}
		else
		{
			fileNameInDir = directoryPointer->d_name;
			strcat(message, fileNameInDir);
			strcat(message, "\n");
		}
	}
	// get rid of last new line character
	message[strcspn(message, "\0") - 1] = '\0';
	closedir(currentDirectory);
}

/* 
** Name: searchDirectory
** Parameters: c-string for fileName
** Output: 1 if file found, 0 otherwise
** Description: searches current directory for passed in file name returning a pseudo-bool value
	borrowed from my program 4 assignment in CS 344 Operating Systems;
references:
https://stackoverflow.com/questions/20265328/readdir-beginning-with-dots-instead-of-files
*/
int searchDirectory(char * fileName)
{
	DIR* currentDirectory;
	struct dirent* directoryPointer;
	char* fileNameInDir;
	currentDirectory = opendir(".");
	while( (directoryPointer = readdir(currentDirectory)) != NULL)
	{
		if ( strcmp(directoryPointer->d_name, fileName) == 0)
			return 1;
	}
	closedir(currentDirectory);
	return 0;
}

/* 
** Name: readFromFile
** Parameters: FILE stream pointer, c-string for message
** Output: message contains file contents
** Description: read file contents from File stream pointer using getline
I think this is replacing end of file marker with a new line but I don't think it's a bug I need to worry about for this assignment
references:
http://man7.org/linux/man-pages/man3/getline.3.html
https://stackoverflow.com/questions/11198305/segmentation-fault-upon-input-with-getline
*/
void readFromFile(FILE * streamFD, char * message)
{
	char * string = NULL;
	int size = 0;
	int eof = 0;
	int bytesRead;
	int currentBytes = 0;

	while (eof == 0)
	{
		bytesRead = (int) getline(&string, &size, streamFD);
		int errsv = errno;
		if (bytesRead >= 0)
		{		
			int i;
			for (i = 0; i < bytesRead; i++)
			{
				message[currentBytes] = string[i];
				currentBytes++;
			}
		}
		else if (bytesRead == -1)
		{
			if (errsv == EINVAL || errsv == ENOMEM)
			{
				printf("error: %d\n", errsv);
				eof = 1;
			}
			else
			{
				eof = 1;
			}
		}
	}
}

/* 
** Name: shutdownSockets
** Parameters: server socket file descriptor, connection socket file descriptor,
 server address struct pointer, malloc'ed c-string
** Output: none
** Description: frees memory and closes data connection sockets on the server side
*/
void shutdownSockets(int serverSocket, int connectionSocket, struct sockaddr_in* dataAddress, char* serverMessage)
{
	shutdown(connectionSocket, SHUT_RD);
	shutdown(serverSocket, SHUT_RD);
	close(connectionSocket);
	close(serverSocket);
	free(dataAddress);
	free(serverMessage);
}

/* 
** Name: validatePortNumber
** Parameters: c-string for command-line argument
** Output: invalid return 1, otherwise return 0
** Description: checks command-line argument for invalid ascii, also checks for invalid port number ranges
references: 
https://www.asciitable.com/
https://stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c
*/
int validatePortNumber(char * portNum)
{
	// convert to string
	// search for invalid ascii
	int length = strlen(portNum);
	//char * temp = malloc(length + 1);
	//snprintf(temp, length + 1, "%d", portNum);
	
	int i;
	for (i = 0; i < length; i++)
	{
		if (portNum[i] >= 48 && portNum[i] <= 57)
		{
			// valid ascii integer
		}
		else
		{
			return 1;
		}
	}
	if (atoi(portNum) < 1024)
	{
		return 1;
	}
	else if (atoi(portNum) > 65535)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}