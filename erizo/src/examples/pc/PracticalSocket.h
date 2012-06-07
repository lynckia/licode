/*
 *   C++ sockets on Unix and Windows
 *   Copyright (C) 2002
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __PRACTICALSOCKET_INCLUDED__
#define __PRACTICALSOCKET_INCLUDED__

#include <string>            // For string
#include <exception>         // For exception class

using namespace std;

/**
 *   Signals a problem with the execution of a socket call.
 */
class SocketException : public exception {
public:
  /**
   *   Construct a SocketException with a explanatory message.
   *   @param message explanatory message
   *   @param incSysMsg true if system message (from strerror(errno))
   *   should be postfixed to the user provided message
   */
  SocketException(const string &message, bool inclSysMsg = false) throw();

  /**
   *   Provided just to guarantee that no exceptions are thrown.
   */
  ~SocketException() throw();

  /**
   *   Get the exception message
   *   @return exception message
   */
  const char *what() const throw();

private:
  string userMessage;  // Exception message
};

/**
 *   Base class representing basic communication endpoint
 */
class Socket {
public:
  /**
   *   Close and deallocate this socket
   */
  ~Socket();

  /**
   *   Get the local address
   *   @return local address of socket
   *   @exception SocketException thrown if fetch fails
   */
  string getLocalAddress() throw(SocketException);

  /**
   *   Get the local port
   *   @return local port of socket
   *   @exception SocketException thrown if fetch fails
   */
  unsigned short getLocalPort() throw(SocketException);

  /**
   *   Set the local port to the specified port and the local address
   *   to any interface
   *   @param localPort local port
   *   @exception SocketException thrown if setting local port fails
   */
  void setLocalPort(unsigned short localPort) throw(SocketException);

  /**
   *   Set the local port to the specified port and the local address
   *   to the specified address.  If you omit the port, a random port 
   *   will be selected.
   *   @param localAddress local address
   *   @param localPort local port
   *   @exception SocketException thrown if setting local port or address fails
   */
  void setLocalAddressAndPort(const string &localAddress, 
    unsigned short localPort = 0) throw(SocketException);

  /**
   *   If WinSock, unload the WinSock DLLs; otherwise do nothing.  We ignore
   *   this in our sample client code but include it in the library for
   *   completeness.  If you are running on Windows and you are concerned
   *   about DLL resource consumption, call this after you are done with all
   *   Socket instances.  If you execute this on Windows while some instance of
   *   Socket exists, you are toast.  For portability of client code, this is 
   *   an empty function on non-Windows platforms so you can always include it.
   *   @param buffer buffer to receive the data
   *   @param bufferLen maximum number of bytes to read into buffer
   *   @return number of bytes read, 0 for EOF, and -1 for error
   *   @exception SocketException thrown WinSock clean up fails
   */
  static void cleanUp() throw(SocketException);

  /**
   *   Resolve the specified service for the specified protocol to the
   *   corresponding port number in host byte order
   *   @param service service to resolve (e.g., "http")
   *   @param protocol protocol of service to resolve.  Default is "tcp".
   */
  static unsigned short resolveService(const string &service,
                                       const string &protocol = "tcp");

private:
  // Prevent the user from trying to use value semantics on this object
  Socket(const Socket &sock);
  void operator=(const Socket &sock);

protected:
  int sockDesc;              // Socket descriptor
  Socket(int type, int protocol) throw(SocketException);
  Socket(int sockDesc);
};

/**
 *   Socket which is able to connect, send, and receive
 */
class CommunicatingSocket : public Socket {
public:
  /**
   *   Establish a socket connection with the given foreign
   *   address and port
   *   @param foreignAddress foreign address (IP address or name)
   *   @param foreignPort foreign port
   *   @exception SocketException thrown if unable to establish connection
   */
  void connect(const string &foreignAddress, unsigned short foreignPort)
    throw(SocketException);

  /**
   *   Write the given buffer to this socket.  Call connect() before
   *   calling send()
   *   @param buffer buffer to be written
   *   @param bufferLen number of bytes from buffer to be written
   *   @exception SocketException thrown if unable to send data
   */
  void send(const void *buffer, int bufferLen) throw(SocketException);

  /**
   *   Read into the given buffer up to bufferLen bytes data from this
   *   socket.  Call connect() before calling recv()
   *   @param buffer buffer to receive the data
   *   @param bufferLen maximum number of bytes to read into buffer
   *   @return number of bytes read, 0 for EOF, and -1 for error
   *   @exception SocketException thrown if unable to receive data
   */
  int recv(void *buffer, int bufferLen) throw(SocketException);

  /**
   *   Get the foreign address.  Call connect() before calling recv()
   *   @return foreign address
   *   @exception SocketException thrown if unable to fetch foreign address
   */
  string getForeignAddress() throw(SocketException);

  /**
   *   Get the foreign port.  Call connect() before calling recv()
   *   @return foreign port
   *   @exception SocketException thrown if unable to fetch foreign port
   */
  unsigned short getForeignPort() throw(SocketException);

protected:
  CommunicatingSocket(int type, int protocol) throw(SocketException);
  CommunicatingSocket(int newConnSD);
};

/**
 *   TCP socket for communication with other TCP sockets
 */
class TCPSocket : public CommunicatingSocket {
public:
  /**
   *   Construct a TCP socket with no connection
   *   @exception SocketException thrown if unable to create TCP socket
   */
  TCPSocket() throw(SocketException);

  /**
   *   Construct a TCP socket with a connection to the given foreign address
   *   and port
   *   @param foreignAddress foreign address (IP address or name)
   *   @param foreignPort foreign port
   *   @exception SocketException thrown if unable to create TCP socket
   */
  TCPSocket(const string &foreignAddress, unsigned short foreignPort) 
      throw(SocketException);

private:
  // Access for TCPServerSocket::accept() connection creation
  friend class TCPServerSocket;
  TCPSocket(int newConnSD);
};

/**
 *   TCP socket class for servers
 */
class TCPServerSocket : public Socket {
public:
  /**
   *   Construct a TCP socket for use with a server, accepting connections
   *   on the specified port on any interface
   *   @param localPort local port of server socket, a value of zero will
   *                   give a system-assigned unused port
   *   @param queueLen maximum queue length for outstanding 
   *                   connection requests (default 5)
   *   @exception SocketException thrown if unable to create TCP server socket
   */
  TCPServerSocket(unsigned short localPort, int queueLen = 5) 
      throw(SocketException);

  /**
   *   Construct a TCP socket for use with a server, accepting connections
   *   on the specified port on the interface specified by the given address
   *   @param localAddress local interface (address) of server socket
   *   @param localPort local port of server socket
   *   @param queueLen maximum queue length for outstanding 
   *                   connection requests (default 5)
   *   @exception SocketException thrown if unable to create TCP server socket
   */
  TCPServerSocket(const string &localAddress, unsigned short localPort,
      int queueLen = 5) throw(SocketException);

  /**
   *   Blocks until a new connection is established on this socket or error
   *   @return new connection socket
   *   @exception SocketException thrown if attempt to accept a new connection fails
   */
  TCPSocket *accept() throw(SocketException);

private:
  void setListen(int queueLen) throw(SocketException);
};

/**
  *   UDP socket class
  */
class UDPSocket : public CommunicatingSocket {
public:
  /**
   *   Construct a UDP socket
   *   @exception SocketException thrown if unable to create UDP socket
   */
  UDPSocket() throw(SocketException);

  /**
   *   Construct a UDP socket with the given local port
   *   @param localPort local port
   *   @exception SocketException thrown if unable to create UDP socket
   */
  UDPSocket(unsigned short localPort) throw(SocketException);

  /**
   *   Construct a UDP socket with the given local port and address
   *   @param localAddress local address
   *   @param localPort local port
   *   @exception SocketException thrown if unable to create UDP socket
   */
  UDPSocket(const string &localAddress, unsigned short localPort) 
      throw(SocketException);

  /**
   *   Unset foreign address and port
   *   @return true if disassociation is successful
   *   @exception SocketException thrown if unable to disconnect UDP socket
   */
  void disconnect() throw(SocketException);

  /**
   *   Send the given buffer as a UDP datagram to the
   *   specified address/port
   *   @param buffer buffer to be written
   *   @param bufferLen number of bytes to write
   *   @param foreignAddress address (IP address or name) to send to
   *   @param foreignPort port number to send to
   *   @return true if send is successful
   *   @exception SocketException thrown if unable to send datagram
   */
  void sendTo(const void *buffer, int bufferLen, const string &foreignAddress,
            unsigned short foreignPort) throw(SocketException);

  /**
   *   Read read up to bufferLen bytes data from this socket.  The given buffer
   *   is where the data will be placed
   *   @param buffer buffer to receive data
   *   @param bufferLen maximum number of bytes to receive
   *   @param sourceAddress address of datagram source
   *   @param sourcePort port of data source
   *   @return number of bytes received and -1 for error
   *   @exception SocketException thrown if unable to receive datagram
   */
  int recvFrom(void *buffer, int bufferLen, string &sourceAddress, 
               unsigned short &sourcePort) throw(SocketException);

  /**
   *   Set the multicast TTL
   *   @param multicastTTL multicast TTL
   *   @exception SocketException thrown if unable to set TTL
   */
  void setMulticastTTL(unsigned char multicastTTL) throw(SocketException);

  /**
   *   Join the specified multicast group
   *   @param multicastGroup multicast group address to join
   *   @exception SocketException thrown if unable to join group
   */
  void joinGroup(const string &multicastGroup) throw(SocketException);

  /**
   *   Leave the specified multicast group
   *   @param multicastGroup multicast group address to leave
   *   @exception SocketException thrown if unable to leave group
   */
  void leaveGroup(const string &multicastGroup) throw(SocketException);

private:
  void setBroadcast();
};

#endif