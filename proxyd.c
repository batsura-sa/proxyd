/* ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
   ³  Author: Sergey Batsura _gray_@mail.ru
   ³ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
   ³  Created 23.04.2001 at 18:24:21
   ³ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
   ³  Project: proxyd version 0.1b
   ³  Comments: simple bridge for tcp applications
   ³  Tested in: SCO Open Server, Linux, Sun Solaris
   ³  Bugs: Possible then server close connection proxy is stay working
*/

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<netdb.h>
#include<string.h>
#include<fcntl.h>
#include<errno.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/socket.h>
#include<sys/resource.h>
#include<sys/time.h>
#include<sys/wait.h>
#include<netinet/in.h>
#include<stdarg.h>


#define MAXCONNECTIONS 10
#define MLEN           1024
#define PACKETSIZE     1024
#define LOGFILE        "proxyd.log"
#define LOGFILEBIN     "proxyd.bin"

int LogLevel;

void
Error( int Level, char * Message, ... ) {
va_list args;
FILE	*	fd;
time_t   t;
char     date[MLEN];

	if( Level > LogLevel )
		return;

	if( (fd = fopen( LOGFILE, "a" )) == NULL )
		return;
		
   time(&t);
   strcpy( date, (char *)ctime(&t) );
   date[strlen(date)-1]=0;
   fprintf( fd, "(%s)(pid=%d)\t" , date, getpid() );
   va_start( args, Message );
   vfprintf( fd, Message, args );
   va_end( args );
   fputc( '\n', fd ); 
   fclose( fd );

   return;
}

void
Write( int Char ) {
va_list args;
FILE	*	fd;
time_t   t;
char     date[MLEN];

	if ( LogLevel<100 )
		return;

	if( (fd = fopen( LOGFILEBIN, "a" )) == NULL )
		return;
		
   fputc( Char, fd ); 
   fclose( fd );

   return;
}

void 
SignalHandler( int SignalType )
{
  int status;

  while (  wait3(&status, WNOHANG, NULL) > 0);
}

void
SignalInit( void ) {
struct sigaction sig;
/*
 sig.sa_handler = SIG_IGN;
 sig.sa_flags = 0;
 sigemptyset(&sig.sa_mask);
 sigaction(SIGPIPE,&sig,NULL);
*/
 sigemptyset(&sig.sa_mask);
 sig.sa_flags = 0;
 sig.sa_handler = SignalHandler;
 sigaction(SIGCHLD, &sig, NULL);

}

int
ListenPort( char *StringPort ) {
struct sockaddr_in Address;
int                Len;
short              Port;
int                Socket;
int                ConnectedSocket;
int                ReuseAddr;
int                Process;
unsigned char      StringAddress[4];

 Port = atoi( StringPort );
  
 Error( 100,"ListenPort: begin Port=%d", Port );
 
 Port = htons( Port );

 memset((char *) &Address, 0, sizeof(Address));
 Address.sin_family = AF_INET;
 Address.sin_port = Port;
 Address.sin_addr.s_addr = htonl(INADDR_ANY);
							
 if ( ( Socket = socket(AF_INET, SOCK_STREAM, 0) ) < 0 ) {
     Error( 1, "ListenPort: socket: %s", strerror(errno) ); 
     exit(0);
  }

 ReuseAddr = 1;
 
 setsockopt(Socket, SOL_SOCKET, SO_REUSEADDR, &ReuseAddr, sizeof(ReuseAddr));

 if ( bind( Socket, (struct sockaddr *) &Address, sizeof(Address)) < 0) {
    close(Socket);
    Error( 1, "ListenPort: bind: %s", strerror(errno) ); 
    exit(0);
  }

 if ( listen( Socket, MAXCONNECTIONS ) < 0 ) {
    close(Socket);
    Error( 1, "ListenPort: listen: %s", strerror(errno) ); 
    exit(0);
  }
  
  ConnectedSocket = -1;
	 
  while( ConnectedSocket < 0 ) {
    
     ConnectedSocket = accept( Socket, NULL, NULL );
     
     if ( ConnectedSocket < 0 ) {
      
        if ( errno == EINTR )
          continue;

        Error( 1, "ListenPort: accept: %s", strerror(errno) ); 

        close(Socket);
        exit(0);
      }

      Process = fork();
      
     if ( Process != 0 ) {
        close( ConnectedSocket );
        ConnectedSocket = -1;
        continue;
      }

     if ( getpeername( ConnectedSocket, (struct sockaddr *) &Address, &Len ) < 0 ) {
        Error( 1, "ListenPort: getpeername: %s", strerror( errno ) );
        exit(0);
      }

     memmove( StringAddress, &Address.sin_addr.s_addr, 4 );
     Error( 2, "ListenPort: Client %d.%d.%d.%d connected", 
                  StringAddress[0],
                  StringAddress[1],
                  StringAddress[2],
                  StringAddress[3]);
     close(Socket);
        
   }
   
 Error( 100, "ListenPort: return ConnectedSocket=%d", ConnectedSocket ); 
   
 return ConnectedSocket;
}


struct in_addr *
HostToAddr( char *Addr )
{
  struct hostent *Host;
  static struct in_addr Address;

  Address.s_addr = inet_addr(Addr);
  
  if (Address.s_addr != -1) 
    return &Address;
  
  Host = gethostbyname(Addr);
  
  if (Host != NULL) {
    return (struct in_addr *) *Host->h_addr_list;
  }
  
  return NULL;
}

int
ConnectTo( char *RemoteHost, char *RemotePort ) {
short              Port;
struct in_addr    *Addr;
struct sockaddr_in Address;
int                Socket;

 Error( 100, "ConnectTo: begin RemoteHost=%s RemotePort=%s", RemoteHost, RemotePort );
 
 Port = atoi( RemotePort );
 Port = htons( Port );

 if ( ( Addr = HostToAddr( RemoteHost ) ) == NULL ) {
   Error( 1, "ConnectTo: invalid network address" );
   exit(0);
  }

 memset((char *) &Address, 0, sizeof(Address));
 Address.sin_family = AF_INET;
 Address.sin_port   = Port;
 Address.sin_addr.s_addr = Addr->s_addr;

 Socket = socket(AF_INET, SOCK_STREAM, 0);

 Error( 100, "ConnectTo: Connecting to %s on port %d",inet_ntoa(*Addr),htons(Port));
 
 if ( connect( Socket , (struct sockaddr *) &Address, sizeof(Address)) < 0 ) {
      Error( 1,"ConnectTo: connect: %s", strerror(errno) );
      exit(0);
    }

 Error( 100, "ConnectTo: return Socket=%d", Socket );
 
 return Socket;

}


void
SendByte( int FromSock, int ToSock ) {
char Buffer[PACKETSIZE];
int  Flag;
int  Len;
int  i;
char Char;

//  Flag  = fcntl( FromSock, F_GETFL, 0 );
//  fcntl( FromSock, F_SETFL, Flag|O_NONBLOCK );

 if ( ( Len = read ( FromSock, Buffer, sizeof( Buffer ) ) ) > 0 ) {
      if ( write(   ToSock, Buffer, Len ) < 0 ) {
        close( FromSock );
        close( ToSock );      
        Error( 1, "write: ToSock=%d %s", ToSock, strerror(errno) );
        exit(0);
        }
 } else {
        close( FromSock );
        close( ToSock );      
        Error( 1, "read: FromSock=%d %s %d", ToSock, strerror(errno), Len );
        exit(0);
      }
										
  for( i=0;i<Len;i++)  {
  	Char=0;
   memmove( &Char, Buffer+i, 1 );
  	Error( 100, "Readfrom %d: %u %c", FromSock, Char, Char );
  	Write( Char );
  	}
  	
/* 
 if ( Len < 0 ) 
   if ( errno != EAGAIN ) {
     close( FromSock );
     close( ToSock );
     Error( 1, "read: FromSock=%d %s", FromSock, strerror( errno ) );
     exit(0);
    }
*/

//  fcntl(FromSock,F_SETFL,Flag);
   
}

int
main( int ac, char **av ) {
int     ClientSocket,
        ServerSocket,
        MaxSock;
fd_set  ReadSet,
        DefaultSet;

 if ( ac < 4 ) {
  fprintf( stderr,"Usage:\n\t%s localport remotehost remoteport [loglevel]\n", av[0] );
  exit(0);
 }

 LogLevel = atol( av[4] );
 
 Error( 100, "Process %s started LogLevel=%d", av[0], LogLevel );

 if ( fork() )
 	   exit(0);

 setsid();
 
 Error( 100, "Fork complete" );

 SignalInit();

 Error( 100, "SignalInit complete" );
 
 ClientSocket = ListenPort( av[1] );
 
 Error( 100, "ListenPort complete" );

 ServerSocket = ConnectTo( av[2], av[3] );

 Error( 100, "ConnectTo complete" );
 
 if ( ClientSocket > ServerSocket )
    MaxSock = ClientSocket + 1;
   else
    MaxSock = ServerSocket + 1;

 FD_ZERO(&DefaultSet);
 FD_SET( ClientSocket, &DefaultSet);
 FD_SET( ServerSocket, &DefaultSet);

 while(1) {
 
     ReadSet = DefaultSet;
	  
     switch( select( MaxSock, &ReadSet, NULL, NULL, NULL ) ) {
     
      case -1:
         	  Error( 1, "select: %s", strerror( errno ) );
              exit(0);
              break;
      case  0:
              break;
      default:

             if(FD_ISSET(ClientSocket, &ReadSet)) {
                SendByte( ClientSocket, ServerSocket );
                continue;
                }
                
             if(FD_ISSET(ServerSocket, &ReadSet)) {
                SendByte( ServerSocket, ClientSocket );
                continue;
                }
               break;

      }
  }
 
} 
