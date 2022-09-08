#define getTime__
#define aggiungiLog__
#define accediSocket__
#define connetti__
#define leggiElemento__

extern FILE *logFile;
extern char *getTime(char *data);
extern void aggiungiLog(const char *fmt, ...);
extern void accediSocket(int *server, struct sockaddr_un *serverAddress, int *serverLen, char* nomeServer);
extern void connetti(int server, struct sockaddr *serverAddressPtr, int serverLen);
extern void leggiElemento(int fd, char *str);