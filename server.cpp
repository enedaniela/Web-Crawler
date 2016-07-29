#include <iostream>
#include <exception>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <iterator>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <fstream>
#include <cstdlib>
#include <queue>

#define MAX_CLIENTS	10
#define MIN_CLIENTS 5
#define BUFLEN 4096

using namespace std;

/*clasa ce defineste un client*/
class Worker {
public:
	int worker_skt;
	int port;
	string ip;
    bool isBusy;
  public:
    Worker (int, int, string, bool);
};

Worker::Worker (int socket, int p, string wIP, bool busy) {
	worker_skt = socket;
	port = p;
	ip = wIP;
	isBusy = busy;
}

bool recursive = false;
bool entire = false;
bool logFile = false;
string logFileName;
char* port;
vector<Worker> workers;//vector in care se pastreaza clientii conectati
int workersCount = 0;
queue<string> links;
fd_set read_fds;	//multimea de citire folosita in select()
string pathToWrite;

/*exceptie pentru nerespecarea formatului paramerilor*/
struct MyException : public exception
{
  const char * what () const throw ()
  {
    return "Usage: ./server [-r] [-e] [-o <fisier_log>] -p <port> ";
  }
};

/*functie ce testeaza daca un string este numar
folosita pentru a valida portul*/
bool is_number(string s)
{
    string::const_iterator it = s.begin();

    while (it != s.end() && std::isdigit(*it))
    	++it;
    return !s.empty() && it == s.end();
}

/*functe ce scrie mesajele de status in fisier sau la stdout*/
void writeStatus(string msg){
	if(logFile){
		fstream fs;
		string file = logFileName + ".stdout";
		const char *cstr = file.c_str();
	    fs.open (cstr, fstream::in | fstream::out | fstream::app);
	    fs << msg << "\n";
	}
	else{
		cout<< msg << "\n";
	}
}

/*functie ce scrie mesajele de eroare in fisier sau la stdout*/
void writeError(string msg){
	if(logFile){
		fstream fs;
		string file = logFileName + ".stderr";
		const char *cstr = file.c_str();
	    fs.open (cstr, fstream::in | fstream::out | fstream::app);
	    fs << msg << "\n";
	}
	else{
		cout<< msg << "\n";
	}
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

/*functie care parseaza dupa ' '*/
vector<string> parse(string str){
    std::istringstream buf(str);
    std::istream_iterator<std::string> beg(buf), end;

    std::vector<std::string> tokens(beg, end); // done!
    return tokens;
}

/*functie ce construieste calea prin ierarhia de directoare*/
string appendStrings(vector<string> command){
    string str = "";
    for(int i = 3; i < command.size() - 1; i++){
        str.append(command.at(i));
        str.append(1u,'/');
    }
    
    return str;
}

/*functie ce valideaza parametrii*/
void checkParameters(int argc, char *argv[]){
	bool must = false;
	try{
		for(int i = 0; i < argc; i++){
			string str(argv[i]);
			if(str == "-r")
				recursive = true;
			if(str == "-e")
				entire = true;
			if(str == "-o"){			
				string fileName(argv[i+1]);
				logFileName = fileName;
				logFile = true;
			}
			if(str == "-p"){				
				string nextParam(argv[i+1]);				
				bool paramOK = is_number(nextParam);
				if(!paramOK)
					throw MyException();
				else{
					must = true;
					port = argv[i+1];
				}
			}
		}
		/*daca nu s-a dat portul ca parametru
		se arunca exceptie*/
		if(!must){
			throw MyException();
		}
	}
	catch(...){
		writeError("Usage: ./server [-r] [-e] [-o <fisier_log>] -p <port> !");
	}
}

/*in functie de modul cu care este apelat servereul,
se construieste un header corespunzator*/
string buildHeader(string link){
	string c;
	string header = "download ";
	if(recursive == false && entire == false){
		c = "0 ";
	}
	if(recursive == true && entire == false){
		c = "1 ";
	}
	if(recursive == false && entire == true){
		c = "2 ";
	}
	if(recursive == true && entire == true){
		c = "3 ";
	}
	header.append(c);
	header.append(link);
	return header;
}

/*functie care afiseaza coada de link-uri ce urmeaza sa fie procesate,
folosita pentru debugging*/
void printQueueList(){
	for (queue<string> dump = links; !dump.empty(); dump.pop())
        std::cout << dump.front() << '\n';

    std::cout << "(" << links.size() << " elements)\n";
}

/*functie are adauga la vectorul de workeri un client nou care s-a conectat*/
void clientConnected(char* add, int portWork, int skt){
	string adip(add);

	char str[300];
	sprintf(str, "Noua conexiune de la %s port %d socket_client %d \n ",
		add, portWork, skt);
	string msg(str);
	writeStatus(msg);
	Worker w (skt, portWork, adip, false);
	workers.push_back(w);
	workersCount++;
}

void displayWorkers(){
	string builder;
	for(int i = 0; i < workers.size(); i++){
		builder.append("Client: port: ");
		stringstream ss;
		ss << workers.at(i).port;
		string str = ss.str();
		builder.append(str);
		builder.append(" adresa IP: ");
		builder.append(workers.at(i).ip);
		builder.append("\n");
	}
	writeStatus(builder);
}

/*functie care trimite un mesaj catre client*/
void send_message(string str, int i){
	int n;
	const char * c = str.c_str();
	n = send(i,c,strlen(c), 0);
	if (n < 0) {
        string msg = "ERROR writing to socket";
    	writeError(msg);
    }
}

/*functie care construieste ierarhia de directoare unde
va fi salvat fisierul*/
void buildPath(string command){
	vector<string> elems = parse(command);
	vector<string> dirs = parseByDelimiter(elems.at(1), '/');
	string path = appendStrings(dirs);
	string com = "mkdir -p ";
	pathToWrite = "";
	pathToWrite = path;
	com.append(path);
	system(com.c_str());
	pathToWrite.append(dirs.back());
}

/*functie ce proceseaza comenzile*/
void doCommands(string command){
	vector<string> tokens = parse(command);
	if(tokens.at(0) == "status")
		displayWorkers();

	if(tokens.at(0) == "exit"){
		string sen = "closing";
		for(int i = 0; i < workers.size(); i++){
	        	send_message(sen, workers.at(i).worker_skt);
	        	close(workers.at(i).worker_skt);
				FD_CLR(workers.at(i).worker_skt, &read_fds);
			}				        
        close(0);
        exit(0);
	}

	if(tokens.at(0) == "download"){
		if(tokens.size() == 2){
    		if(workersCount < MIN_CLIENTS)
    			writeError("You must connect at least 5 workers");
    		else{
    			links.push(buildHeader(tokens.at(1)));
    			buildPath(command);
    		}
		}
		else
		writeError("Usage: download <link>");
	}        	
}

/*functie care asociaza unui woker un link pe care sa il descarce*/
void giveTask(string link){
	for(int i = 0; i < workers.size(); i++)
		if(workers.at(i).isBusy == false){
			send_message(link, workers.at(i).worker_skt);
			break;
		}
}

/*functie care parcurge coada de linkuri si le trimite pentru a fi asociate*/
void giveAllTasks(){
	while(!links.empty()){
		giveTask(links.front());
		links.pop();
	}
}


int main(int argc, char *argv[]){

	char bufferSend[BUFLEN];
	int sockfd, newsockfd, portno;
	int socktosend;
	socklen_t clilen;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, j;
	string str;
	fd_set tmp_fds;	//multime folosita temporar 
	int fdmax;		//valoare maxima file descriptor din multimea read_fds
	std::ofstream outfile;
	bool ok = true;

	checkParameters(argc, argv);

    //golim multimea de descriptori de citire (read_fds) si multimea tmp_fds 
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
     
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
    	char msg[] = "ERROR opening socket";
       writeError(msg);
    }     
    portno = atoi(port);

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;	// foloseste adresa IP a masinii
    serv_addr.sin_port = htons(portno);
     
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0) {
    	char msg[] = "ERROR on binding";
       	writeError(msg);
    }
     
    listen(sockfd, MAX_CLIENTS);
	FD_SET(0,&read_fds);
    //adaugam noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
    FD_SET(sockfd, &read_fds);
     
    fdmax = sockfd;
    // main loop
	while (1) {
		tmp_fds = read_fds;
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) {
			char msg[] = "ERROR in select";
			writeError(msg);
		}
		
		for(int i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				//daca exista activitate de la stdin
				if(i == 0){
		        	fgets(bufferSend, BUFLEN-1, stdin);
		        	string command(bufferSend);
		        	doCommands(command);
		        	if(parse(command).at(0) == "download")
		        		outfile.open(pathToWrite.c_str());
		        	giveAllTasks();
		        	
				}
				else if (i == sockfd) {
					// a venit ceva pe socketul inactiv(cel cu listen) = o noua conexiune
					// actiunea serverului: accept()
					clilen = sizeof(cli_addr);
					if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen)) == -1) {
						char msg[] = "ERROR in accept";
						writeError(msg);
					} 
					else {
						//adaug noul socket intors de accept() la multimea descriptorilor de citire
						FD_SET(newsockfd, &read_fds);
						if (newsockfd > fdmax) { 
							fdmax = newsockfd;
						}
					}
					/*adaug clientul conectat la vectorul de workeri*/
					clientConnected(inet_ntoa(cli_addr.sin_addr), 
						ntohs(cli_addr.sin_port), newsockfd);
					if(workers.size() >= MIN_CLIENTS)
						writeStatus("You can start downloading..\n");
				}
				else {
					/* am primit date pe unul din socketii cu care vorbesc 
					cu clientii*/
					//actiunea serverului: recv()
					memset(buffer, 0, BUFLEN+10);
					if ((n = recv(i, buffer, BUFLEN+10, 0)) <= 0) {
						if (n == 0) {
							//conexiunea s-a inchis
							printf("selectserver: socket %d hung up\n", i);
							/*daca clientul s-a deconectat, il sterg din lista de workeri*/
							for(int j = 0; j < workers.size(); j++)
								if(workers.at(j).worker_skt == i)
									workers.erase(workers.begin() + j);
						} else {
							char msg[] = "ERROR in recv";
							writeError(msg);
						}
						close(i); 
						FD_CLR(i, &read_fds); /*scoatem din multimea de citire
						socketul*/
					}
					else{
						int nbytes;
						char recv_msg[BUFLEN+10];
						memset(recv_msg, 0, BUFLEN+10);
						/*extrag informatiile din headerul mesajelor primite de la client*/
						sscanf(buffer,"MSG %d %s", &nbytes, recv_msg);
						/*scot headerul intors de comanda GET*/
						if(ok == true){
							string htmlstr(buffer + 9);
							int found = htmlstr.find('<');
							outfile.write (buffer + 9 + found, nbytes - found);
							ok = false;					
						}
						outfile.write (buffer + 9, nbytes);
					} 
				}
			}
			
		}
	}
	outfile.close();
}