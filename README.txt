Matthew Toro
CS 372 Computer Networking
Project 2
Due Date: 6/3/2018

RUNNING FTSERVER.C:
I have included a makefile to compile the program.
To compile the server, run 'make'. 
To run the program, type "ftserver <port_number>" where port number is a valid port number to run the server on.
Example: "ftserver 33030"

RUNNING FTCLIENT.PY:
I wrote a tcsh shell script named ftclient to run the client.
It will determine the command line parameters and run the python interpreter.
To execute the script, make sure it is an executable first.
Run "chmod +x ftclient" in case it is not an executable.
To run the client on the command line type:
"ftclient <server_name> <server_port> <command> <file_name> <data_port>"
where server_name is the address where ftserver is running,
server_port is the port you passed in to ftserver,
command is either '-l' or '-g',
file_name is the name of a file with '.txt' extension if the command is '-g',
and data_port is a valid port number to start a data connection.

Examples:
List Directory: "ftclient flip2 33030 -l 33031"
Get File: "ftclient flip2 33030 -g file.txt 33031"

For my testing, I ran my server on flip2.engr.oregonstate.edu using port 33030 and my client on flip3.engr.oregonstate.edu using data port 33031.

Overview:
FTSERVER is programmed to stall and listen for a client connection. 
Once FTCLIENT has validated its command line parameters, it will initiate contact with the server.
Once the connection is established, the client will send the command along with data port, host name, and filename if applicable.
The server will interpret the command and respond appropriately.
If the command is not valid, it will send an error message to the client.
If it receives '-l', it will save its directory contents, open a data connection, and send it on that connection.
If it receives '-g', it will search for that file. 
If the file does not exist, it will send an error message to the client.
If the file exists, it will read its contents, open a data connection, and send it on that connection.
After a valid command, the server will close the data connection and loop again to receive another client connection.
When the client receives a directory listing, it will print the listing and close the control connection.
When the client receives file content, it will save that file, prompting in case the file already exists.
If desired, the user can overwrite the already existing file. If not, the file transfer is aborted.
The client then closes the control connection and terminates.
