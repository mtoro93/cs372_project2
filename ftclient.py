# Name: Matthew Toro
# Class: CS 372 Computer Networking
# Assignment: Project 2
# Due Date: 6/3/2018
# Description: A client for initiating simple file transfers with a server

import sys
import os
from socket import *

# Name: validatePortNum
# Parameters: port number string
# Output: error message if applicable
# Description: attempts to convert command-line argument to int, raises exception if it cant;
#		also tests for invalid port number ranges
# try and except block referenced from https://docs.python.org/2/tutorial/errors.html
def validatePortNum(portNum):
	try:
		test = int(portNum)
	except:
		print "Port Number is not an integer. Please try again with an integer value."
		sys.exit(2)
		
	if test < 1024:
		print "Port Number cannot be less than 1024"
		sys.exit(2)
	if test > 65535:
		print "Port number cannot be greater than 65535"
		sys.exit(2)

# Name: validateFileName
# Parameters: file name string
# Output: error message if applicable
# Description: checks if last four letters are the .txt extension
#		if not, print error message and exit the program
def validateFileName(fileName):
	if fileName[-4:] != ".txt":
		print "Please only transfer .txt files. Try again with a .txt file."
		sys.exit(2)
	
# Name: sendMessage
# Parameters: socket for connection and string for the message to send 
# Output: message is sent
# Description: Sends the passed in message across the connection socket;
# 		ensures we send all the bytes by looping until all is sent;
# 		reference: https://docs.python.org/2/howto/sockets.html#socket-howto
def sendMessage(connectionSocket, message):
	message = message + '@@'
	currentBytes = 0
	while currentBytes < len(message):
		bytesSent = connectionSocket.send(message[currentBytes:])
		currentBytes += bytesSent;

# Name: receiveMessage
# Parameters: socket for connection and string for the received message
# Output: message in a string
# Description: Receives a message from the connection socket into the passed in parameter;
# 		Loops for the end of message sequence '@@' to signify the message has been received
def receiveMessage(connectionSocket, message):
	while message.find("@@") == -1:
		readBuffer = connectionSocket.recv(1024)
		message+=readBuffer
	message = message.replace("@@", " ")
	return message


# Name: receiveFile
# Parameters: socket for  connection, string for the file contents, and transfer file name
# Output: saves transfered file contents to current directory with passed in file name
# Description: Receives a preliminary message from the connection socket for the file size;
#		Iterates, receiving from the socket the file contents until file size has been met
# 		Checks current directory for duplicate file name, then prompts for override if applicable
#		references:
#		recv value recommended by https://docs.python.org/2/library/socket.html
#		large file transfer borrowed from https://stackoverflow.com/questions/17667903/python-socket-receive-large-amount-of-data?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
#		list file directory borrowed from https://stackoverflow.com/questions/3207219/how-do-i-list-all-files-of-a-directory
def receiveFile(connectionSocket, message, filename):
	readBuffer = connectionSocket.recv(4096) 
	# find the terminating sequence "@@", then extract the file size
	index = readBuffer.find("@@")
	fileSizeStr = readBuffer[0:index]
	message = readBuffer[index+3:] 
	if not message: #check if message is empty or not 
		message = ""
	fileSize = int(fileSizeStr)
	while len(message) < fileSize:
		readBuffer = connectionSocket.recv(4096)
		message+=readBuffer
		if not readBuffer:
			break
	# save message as a file, check for duplicate file error
	files = os.listdir(os.getcwd())
	fileFound = False
	for f in files:
		if f.find(filename) != -1:
			fileFound = True
	if fileFound:
		response = raw_input("File already exists. Would you like to overwrite? (y/n): ")
		if response == "y":
			file = open(filename, "w");
			file.write(message)
			file.close
			print "File transfer complete"
		else:
			print "File transfer aborted"
	else:
		file = open(filename, "w");
		file.write(message)
		file.close
		print "File transfer complete"
	
# Name: initiateContact
# Parameters: socket for connection, string for serverName, int for server port number
# Output: 
# Description: client establishes connection to server_host;
# 			borrowed from lecture 15 CS 372
def initiateContact(clientSocket, serverName, serverPort):
	try: 
		clientSocket.connect((serverName, serverPort))
		print "Initiating contact with " + serverName + ":" + str(serverPort)
	except:
		print "Server name and port number combination not valid, exiting program"
		sys.exit(2)
	
# Name: makeRequest
# Parameters: socket for control connection, string for command, string for server name,
#		int for server port number, and socket for data connection
# Output: prints out error message, directory listing, or initiate file transfer
# Description: validates command parameter to request directory listing, request file transfer, or receive error message
def makeRequest(clientSocket, command, serverName, serverPort, dataSocket):
	hostName = gethostname()
	if command != '-g' and command != '-l':
		sendMessage(clientSocket, command)
		serverMessage = ""
		serverMessage = receiveMessage(clientSocket, serverMessage)
		print serverName + ":" + str(serverPort) + " says " + serverMessage

	if command == '-l':
		data_port = sys.argv[4]
		message = command + "\n" + hostName + "\n" + str(data_port)
		sendMessage(clientSocket, message)
		initiateContact(dataSocket, serverName, int(data_port))
		print "Data connection established with " + serverName + " on port: " + str(data_port)
		print "Receiving directory structure from " + serverName + ":" + str(data_port)
		serverMessage = ""
		serverMessage = receiveMessage(dataSocket, serverMessage)
		print serverMessage
		print "Data connection with " + serverName + " on port: " + str(data_port) + " terminated"
		
	if command == '-g':
		filename = sys.argv[4]
		data_port = sys.argv[5]
		message = command + "\n" + hostName + "\n" + str(data_port) + "\n" + filename
		sendMessage(clientSocket, message)
		serverMessage = ""
		serverMessage = receiveMessage(clientSocket, serverMessage)
		if serverMessage.find("ERROR") != -1:
			print "" + serverName + ":" + str(serverPort) + " says FILE NOT FOUND"
		else:
			initiateContact(dataSocket, serverName, int(data_port))
			print "Data connection established with " + serverName + " on port: " + str(data_port)
			print "Receiving \"" + filename + "\" from " + serverName + ":" + str(data_port)
			serverMessage = ""
			serverMessage = receiveFile(dataSocket, serverMessage, filename)
			print "Data connection with " + serverName + " on port: " + str(data_port) + " terminated"
						
serverName = sys.argv[1]
validatePortNum(sys.argv[2])
serverPort = int(sys.argv[2])
command = sys.argv[3]
if command == '-l':
	validatePortNum(sys.argv[4])	
if command == '-g':
	validateFileName(sys.argv[4])
	validatePortNum(sys.argv[5])

clientSocket = socket(AF_INET, SOCK_STREAM)
dataSocket = socket(AF_INET, SOCK_STREAM)
initiateContact(clientSocket, serverName, serverPort)
print "Control connection established with " + serverName + " on port: " + str(serverPort)
makeRequest(clientSocket, command, serverName, serverPort, dataSocket)
print "Control connection with " + serverName + " on port: " + str(serverPort) + " terminating"
# close control connection and terminate; on shutdown(), 1 signifies done sending, still receiving
clientSocket.shutdown(1)
clientSocket.close()