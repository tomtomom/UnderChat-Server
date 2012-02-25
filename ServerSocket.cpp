#include "ServerSocket.h"

// Static constants for the ServerSocket class
const string ServerSocket::SERVER_NOT_FULL = "OK";
const string ServerSocket::SERVER_FULL     = "FULL";
const string ServerSocket::SHUTDOWN_SIGNAL = "/shutdown";

// ServerSocket constructor
ServerSocket::ServerSocket(unsigned int thePort, unsigned int theBufferSize, unsigned int theMaxSockets) {
    debug          = true; // Flag to control whether to output debug info
    shutdownServer = false; // Flag to control whether it's time to shut down the server

    port       = thePort;                      // The port number on the server we're connecting to
    bufferSize = theBufferSize;                // The maximum size of a message
    maxSockets = theMaxSockets;                // Maximum number of sockets in our socket set
    maxClients = theMaxSockets - 1;            // Maximum number of clients who can connect to the server

    pClientSocket = new TCPsocket[maxClients]; // Create the array to the client sockets
    pSocketIsFree = new bool[maxClients];      // Create the array to the client socket free status'
    pBuffer = new char[bufferSize];            // Create the transmission buffer character array

    clientCount    = 0;     // Initially we have zero clients...


    // Create the socket set with enough space to store our desired number of connections (i.e. sockets)
    socketSet = SDLNet_AllocSocketSet(maxSockets);
    if (socketSet == NULL) {
        string msg = "Failed to allocate the socket set: ";
        msg += SDLNet_GetError();

        SocketException e(msg);
        throw e;
    } else {
        if (debug) {
            cout << "Allocated socket set size: " << maxSockets << ", of which " << maxClients << " are free." <<  endl;
        }
    }

    // Initialize all the client sockets (i.e. blank them ready for use!)
    for (unsigned int loop = 0; loop < maxClients; loop++) {
        pClientSocket[loop] = NULL;
        pSocketIsFree[loop] = true; // Set all our sockets to be free (i.e. available for use for new client connections)
    }

    // Try to resolve the provided server hostname to an IP address.
    // If successful, this places the connection details in the serverIP object and creates a listening port on the
    // provided port number.
    // Note: Passing the second parameter as "NULL" means "make a listening port". SDLNet_ResolveHost returns one of two
    // values: -1 if resolving failed, and 0 if resolving was successful
    int hostResolved = SDLNet_ResolveHost(&serverIP, NULL, port);

    if (hostResolved == -1) {
        string msg = "Failed to open the server socket: ";
        msg += SDLNet_GetError();
        SocketException e(msg);
        throw e;
    } else { // If we resolved the host successfully, output the details
        if (debug) {
            // Get our IP address in proper dot-quad format by breaking up the 32-bit unsigned
            // host address and splitting it into an array of four 8-bit unsigned numbers...
            Uint8 * dotQuad = (Uint8*)&serverIP.host;

            dotQuadString  = toString( (unsigned short)dotQuad[0] );
            dotQuadString += ".";
            dotQuadString += toString( (unsigned short)dotQuad[1] );
            dotQuadString += ".";
            dotQuadString += toString( (unsigned short)dotQuad[2] );
            dotQuadString += ".";
            dotQuadString += toString( (unsigned short)dotQuad[3] );

            //... and then outputting them cast to integers. Then read the last 16 bits of the serverIP object to get the port number
            cout << "Successfully resolved server host to IP: " << dotQuadString;
            cout << ", will use port " << SDLNet_Read16(&serverIP.port) << endl;
        }
    }

    // Try to open the server socket
    serverSocket = SDLNet_TCP_Open(&serverIP);

    if (!serverSocket) {
        string msg = "Failed to open the server socket: ";
        msg += SDLNet_GetError();

        SocketException e(msg);
        throw e;
    } else {
        if (debug) {
            cout << "Sucessfully created server socket." << endl;
        }
    }

    // Add our server socket (i.e. the listening socket) to the socket set
    SDLNet_TCP_AddSocket(socketSet, serverSocket);

    if (debug) {
        cout << "Awaiting clients..." << endl;
    }

} // End of constructor

// ServerSocket destructor
ServerSocket::~ServerSocket() {
    // Close all the open client sockets
    for (unsigned int loop = 0; loop < maxClients; loop++) {
        if (pSocketIsFree[loop] == false) {
            SDLNet_TCP_Close(pClientSocket[loop]);
            pSocketIsFree[loop] = true;
        }
    }

    // Close our server socket
    SDLNet_TCP_Close(serverSocket);

    // Free our socket set
    SDLNet_FreeSocketSet(socketSet);

    // Release any properties on the heap
    delete pClientSocket;
    delete pSocketIsFree;
    delete pBuffer;
}


void ServerSocket::checkForConnections() {
    // Check for activity on the entire socket set. The second parameter is the number of milliseconds to wait for.
    // For the wait-time, 0 means do not wait (high CPU!), -1 means wait for up to 49 days (no, really), and any other
    // number is a number of milliseconds, i.e. 5000 means wait for 5 seconds, 50 will poll (1000 / 50 = 20) times per second.
    // I've used 1ms below, so we're polling 1,000 times per second, which is overkill for a small chat server, but might
    // be a good choice for a FPS server where every ms counts! Also, 1,000 polls per second produces negligable CPU load,
    // if you put it on 0 then it WILL eat all the available CPU time on one of your cores...
    int numActiveSockets = SDLNet_CheckSockets(socketSet, 1);

    if (numActiveSockets != 0) {
        if (debug) {
            cout << "There are currently " << numActiveSockets << " socket(s) with data to be processed." << endl;
        }
    }

    // Check if our server socket has received any data
    // Note: SocketReady can only be called on a socket which is part of a set and that has CheckSockets called on it (the set, that is)
    // SDLNet_SocketRead returns non-zero for activity, and zero is returned for no activity. Which is a bit bass-ackwards IMHO, but there you go.
    int serverSocketActivity = SDLNet_SocketReady(serverSocket);

    // If there is activity on our server socket (i.e. a client has trasmitted data to us) then...
    if (serverSocketActivity != 0) {
        // If we have room for more clients...
        if (clientCount < maxClients) {
            // Find the first free socket in our array of client sockets
            int freeSpot = -99;
            for (unsigned int loop = 0; loop < maxClients; loop++) {
                if (pSocketIsFree[loop] == true) {
                    if (debug) {
                        cout << "Found a free spot at element: " << loop << endl;
                    }

                    pSocketIsFree[loop] = false; // Set the socket to be taken
                    freeSpot = loop;            // Keep the location to add the new connection at that index in the array
                    break;                      // Break out of the loop straight away
                }
            }

            // ...accept the client connection and then...
            pClientSocket[freeSpot] = SDLNet_TCP_Accept(serverSocket);

            // ...add the new client socket to the socket set (i.e. the list of sockets we check for activity)
            SDLNet_TCP_AddSocket(socketSet, pClientSocket[freeSpot]);

            // Increase our client count
            clientCount++;

            // Send a message to the client saying "OK" to indicate the incoming connection has been accepted
            strcpy( pBuffer, SERVER_NOT_FULL.c_str() );
            int msgLength = strlen(pBuffer) + 1;
            SDLNet_TCP_Send(pClientSocket[freeSpot], (void *)pBuffer, msgLength);

            if (debug) {
                cout << "Client connected. There are now " << clientCount << " client(s) connected." << endl;
            }
        } else { // If we don't have room for new clients...
            if (debug) {
                cout << "Max client count reached - rejecting client connection" << endl;
            }

            // Accept the client connection to clear it from the incoming connections list
            TCPsocket tempSock = SDLNet_TCP_Accept(serverSocket);

            // Send a message to the client saying "FULL" to tell the client to go away
            strcpy( pBuffer, SERVER_FULL.c_str() );
            int msgLength = strlen(pBuffer) + 1;
            SDLNet_TCP_Send(tempSock, (void *)pBuffer, msgLength);

            // Shutdown, disconnect, and close the socket to the client
            SDLNet_TCP_Close(tempSock);
        }

    } // End of if server socket is has activity check

} // End of checkActivity function

// Function to do something appropriate with the detected socket activity (i.e. we received a message from a client)
// You should put whatever you want to happen when a message is sent from a client inside this function!
// In this example case, I'm going to send the message to all other connected clients except the one who originated the message!
void ServerSocket::dealWithActivity(unsigned int clientNumber) {
    // Get the contents of the buffer as a string
    string bufferContents = pBuffer;

    // Output the message the server received to the screen
    if (debug) {
        cout << "Received: >>>> " << bufferContents << " from client number: " << clientNumber << endl;
    }

    // Send message to all other connected clients
    for (unsigned int loop = 0; loop < maxClients; loop++) {
        // Send a message to the client saying "OK" to indicate the incoming connection has been accepted
        //strcpy( buffer, SERVER_NOT_FULL.c_str() );
        unsigned int msgLength = strlen(pBuffer) + 1;

        // If the message length is more than 1 (i.e. client pressed enter without entering any other text), then
        // send the message to all connected clients except the client who originated the message in the first place
        if ( (msgLength > 1) && (loop != clientNumber) && (pSocketIsFree[loop] == false) ) {
            if (debug) {
                cout << "Retransmitting: " << bufferContents << " (" << msgLength << " bytes) to client " << loop << endl;
            }

            SDLNet_TCP_Send(pClientSocket[loop], (void *)pBuffer, msgLength);
        }
    }

    // If the client told us to shut down the server, then set the flag to get us out of the main loop and shut down
    if ( bufferContents.compare(SHUTDOWN_SIGNAL) == 0 ) {
        shutdownServer = true;

        if (debug) {
            cout << "Disconnecting all clients and shutting down the server..." << endl;
        }
    }

} // End of dealWithActivity function

// Function to check all connected client sockets for activity
// If we find a client with activity we return its number, or if there are
// no clients with activity or we've processed all activity we return 0
int ServerSocket::checkForActivity() {
    // Loop to check all possible client sockets for activity
    for (unsigned int clientNumber = 0; clientNumber < maxClients; clientNumber++) {
        // If the socket is has activity then SDLNet_SocketReady() returns non-zero
        int clientSocketActivity = SDLNet_SocketReady(pClientSocket[clientNumber]);

        // The line below produces a LOT of debug, so only uncomment if the code's seriously misbehaving!
        //cout << "Client number " << clientNumber << " has activity status: " << clientSocketActivity << endl;

        // If there is any activity on the client socket...
        if (clientSocketActivity != 0) {
            // Check if the client socket has transmitted any data by reading from the socket and placing it in the buffer character array
            int receivedByteCount = SDLNet_TCP_Recv(pClientSocket[clientNumber], pBuffer, bufferSize);

            // If there's activity, but we didn't read anything from the client socket, then the client has disconnected...
            if (receivedByteCount <= 0) {
                //...so output a suitable message and then...
                if (debug) {
                    cout << "Client " << clientNumber << " disconnected." << endl;
                }

                //... remove the socket from the socket set, then close and reset the socket ready for re-use and finally...
                SDLNet_TCP_DelSocket(socketSet, pClientSocket[clientNumber]);
                SDLNet_TCP_Close(pClientSocket[clientNumber]);
                pClientSocket[clientNumber] = NULL;

                // ...free up their slot so it can be reused...
                pSocketIsFree[clientNumber] = true;

                // ...and decrement the count of connected clients.
                clientCount--;

                if (debug) {
                    cout << "Server is now connected to: " << clientCount << " client(s)." << endl;
                }
            } else { // If we read some data from the client socket...
                // ... return the active client number to be processed by the dealWithActivity function
                return clientNumber;
            }

        } // End of if client socket is active check

    } // End of server socket check sockets loop

    // If we got here then there are no more clients with activity to process!
    return -1;

} // End of checkForActivity function

// Function to return the shutdown status of the ServerSocket object
bool ServerSocket::getShutdownStatus() {
    return shutdownServer;
}
