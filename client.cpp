#include <iostream>
#include <exception>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h> 
#include <unistd.h>
#include <iterator>
#include <sstream>
#include <iostream>
#include <dirent.h>
#include <unistd.h>
#include <fstream>

using namespace std;

#define BUFLEN 4096
#define HTTP_PORT 80

bool logFile;
char* port;
char* address;
string logFileName;
int sockfd;

/*exceptie pentru nerespecarea formatului paramerilor*/
class myexception: public exception
{
  virtual const char* what() const throw()
  {
    return "Usage: ./client [-o <fisier_log>] -a <adresa ip server> -p <port>";
  }
} paramException;

//returneaza PID
string getPID(){
    pid_t pid;
    pid = getpid();
    stringstream ss;
    ss << pid;
    string p = ss.str();
    return p;
}

/*functie ce testeaza daca un string este numar
folosita pentru a valida portul*/
bool is_number(string s)
{
    string::const_iterator it = s.begin();

    while (it != s.end() && std::isdigit(*it))
    	++it;
    return !s.empty() && it == s.end();
}

//functie care parseaza dupa ' '
vector<string> parse(string str){
    std::istringstream buf(str);
    std::istream_iterator<std::string> beg(buf), end;

    std::vector<std::string> tokens(beg, end); // done!
    return tokens;
}

/*functie ce parseaza dupa un delimitator*/
vector<string> parseByDelimiter(string str, char c)
{
	std::stringstream test(str);
	std::string segment;
	std::vector<std::string> seglist;

	while(std::getline(test, segment, c))
	{
	   seglist.push_back(segment);
	}
	return seglist;
}

/*functie ce extrage calea din linkul primit de la server*/
string appendStringsDownload(vector<string> command){
	string str = "";
	for(int i = 3; i < command.size(); i++){
		str.append(command.at(i));
		str.append(1u,'/');
	}
	string st = str.substr(0, str.size()-1);
	return st;
}

/*functe ce scrie mesajele de status in fisier sau la stdout*/
void writeStatus(string msg){
	if(logFile){
		fstream fs;
		string file = logFileName + getPID() + ".stdout";
		const char *cstr = file.c_str();
	    fs.open (cstr, fstream::in | fstream::out | fstream::app);
	    fs << msg << "\n";
	    fs.close();
	}
	else{
		cout<< msg << "\n";
	}
}

/*functie ce scrie mesajele de eroare in fisier sau la stdout*/
void writeError(string msg){
	if(logFile){
		fstream fs;
		string file = logFileName + getPID() + ".stderr";
		const char *cstr = file.c_str();
	    fs.open (cstr, fstream::in | fstream::out | fstream::app);
	    fs << msg << "\n";
	    fs.close();
	}
	else{
		cout<< msg << "\n";
	}
}

/*functie ce valideaza parametrii*/
void checkParameters(int argc, char *argv[]){
	bool must_port = false;
	bool must_addr = false;
	struct in_addr addr;

	try{
		for(int i = 1; i < argc; i++){		
			string str(argv[i]);
			if(str == "-o"){			
				string fileName(argv[i+1]);
				logFileName = fileName;
				logFile = true;
			}
			if(str == "-p"){				
				string nextParam(argv[i+1]);
				bool paramOK = is_number(nextParam);
				if(!paramOK)
					throw paramException;
				else{
					port = argv[i+1];
					must_port = true;
				}
			}
			if(str == "-a"){
				string fileName(argv[i+1]);
				if (inet_aton(argv[i+1], &addr) == 0) {
		        	throw paramException;
				}
				else{
					address = argv[i+1];
					must_addr = true;
				}
			}
		}
		/*daca nu s-au dat portul si adresa ca parametru
		se arunca exceptie*/
		if(!must_addr || !must_port){
			throw paramException;
		}
	}	
	catch(...){
		writeError("Usage: ./client [-o <fisier_log>] -a <adresa ip server> -p <port>");
	}
}

//functie care trimite un mesaj catre client
void send_message(string str, int i){
	int n;
	const char * c = str.c_str();
	n = send(i,c,strlen(c), 0);
	if (n < 0) {
        string msg = "ERROR writing to socket";
    	writeError(msg);
    }
}

/*functie ce trimite si receptioneaza de pe serverul de hhtp*/
void send_command(int sockhttp, char sendbuf[]) {
	char recvbuf[BUFLEN];
	char string_to_send[BUFLEN+10];
	int nbytes;
	char CRLF[3];
	FILE *fp = fopen("", "ab+");
	CRLF[0] = 13; CRLF[1] = 10; CRLF[2] = 0;
	strcat(sendbuf, CRLF);

	write(sockhttp, sendbuf, strlen(sendbuf));
	while(1){
		memset(recvbuf,0,BUFLEN);
		nbytes = read(sockhttp, recvbuf, BUFLEN - 1);
		if(nbytes == 0)
		  break;
		memset(string_to_send,0,BUFLEN+10);
		/*construiesc headerul mesajului pe care il trimit catre server*/
		sprintf(string_to_send,"MSG %4d %s",nbytes,recvbuf);

		int n = send(sockfd, string_to_send, nbytes+9, 0);
		if (n < 0) {
		    char msg[] = "ERROR writing to socket";
		    writeError(msg);
		}
	  
	}
}

/*functie care trimite comanda GET catre server*/
void downloadPage(string server_ip, string link_to_download){
	int sockhttp;
	int port = HTTP_PORT;
	struct sockaddr_in servaddr;
	char sendbuf[BUFLEN]; 
	char recvbuf[BUFLEN];

	if ( (sockhttp = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
	writeError("Eroare la creare socket.\n");
	exit(-1);
	}  

	/* formarea adresei serverului */
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);

	if (inet_aton(server_ip.c_str(), &servaddr.sin_addr) <= 0 ) {
	writeError("Adresa IP invalida.\n");
	exit(-1);
	}

	/*  conectare la server  */
	if (connect(sockhttp, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
	writeError("Eroare la conectare\n");
	exit(-1);
	}

	sprintf(sendbuf, "GET /%s HTTP/1.0\n\n", link_to_download.c_str());
	send_command(sockhttp, sendbuf);

	close(sockhttp);
}

int main(int argc, char *argv[]){

	int n, no = 0;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int length = 4095;
    char * buffer = new char [length];
    struct hostent *he;
    struct in_addr *addr_list;
    char ip[16];

    fd_set read_fds;    //multimea de citire folosita in select()
    fd_set tmp_fds;    //multime folosita temporar 
    int fdmax;     //valoare maxima file descriptor din multimea read_fds

    //golim multimea de descriptori de citire (read_fds) si multimea tmp_fds 
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    char bufferSend[BUFLEN];
    char bufferRecv[BUFLEN];

 	checkParameters(argc, argv);
    
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        char msg[] = "ERROR opening socket";
        writeError(msg);
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(port));
    inet_aton(address, &serv_addr.sin_addr);
       
    if (connect(sockfd,(struct sockaddr*) &serv_addr,sizeof(serv_addr)) < 0) {
        char msg[] = "ERROR connecting";
        writeError(msg);    
    }
    FD_SET(sockfd, &read_fds);
    FD_SET(0,&read_fds);
    fdmax = sockfd;

    while(1){
        tmp_fds = read_fds; 
        if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) {
            char msg[] = "ERROR in select";
            writeError(msg);
        }

        if(FD_ISSET(0, &tmp_fds)){
      		//citesc de la tastatura
        	memset(bufferSend, 0 , BUFLEN);
        	fgets(bufferSend, BUFLEN-1, stdin);
        }
        if(FD_ISSET(sockfd, &tmp_fds)){
            //primesc de la server
            memset(bufferRecv, 0 , BUFLEN);
            no = recv(sockfd, bufferRecv, sizeof(bufferRecv), 0);
            string command(bufferRecv);
            vector<string> tokens = parse(command);
            if(tokens.at(0) == "closing"){
		        	close(sockfd);
		        	close(0);
					FD_CLR(sockfd, &read_fds);
					FD_CLR(0, &read_fds);
					break;
			}
			if(tokens.at(0) == "download"){
				vector<string> l = parseByDelimiter(tokens.at(2), '/');
				if ((he = gethostbyname(l.at(2).c_str())) == NULL) {  // get the host info
			        writeError("gethostbyname: unrecognizable");			        
			    }
			    else{
				    addr_list = (struct in_addr *)he->h_addr;
    				memcpy(ip, inet_ntoa(*(struct in_addr*)he->h_addr), 16);
			    }

				if(tokens.at(1) == "0"){
					cout<<"Downloading.." << tokens.at(2)<<"\n";
					downloadPage(ip, appendStringsDownload(l));
					cout<<"Download finished.\n";
				}
			}
        }
    }
}