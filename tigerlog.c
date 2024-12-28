
/* Program to receive data for logging to a file. */

#include <stdio.h>      // for printf() and fprintf()
#include <sys/socket.h> // for socket(), connect(), sendto(), and recvfrom()
#include <arpa/inet.h>  // for sockaddr_in and inet_addr()
#include <stdlib.h>     // for atoi() and exit()
#include <stdbool.h>    
#include <stdarg.h>    
#include <string.h>     // for memset()
#include <signal.h>
#include <netdb.h>
#include <unistd.h>     // for close()
#include <time.h>       // for time()
#include <sys/time.h>   // for settimeofday()
#include <math.h>       // for abs()
#include <ctype.h>
#include <dirent.h>
#include <errno.h>

#include <ini.h>
#include <strutils.h>

#define MILLION				1000000
#define KILOBYTE			1024
#define DEFAULT_MAXLOGS		32
#define DEFAULT_MAXRECVSIZE	1024
#define APP_NAME			"TigerLog"

typedef struct _logs_ {
	char *name;
	char *path;
	FILE *fd;
	bool inUse;
} TigerLog;

TigerLog *tigerLogs = NULL;

IniFile *ini = NULL;
char *cfgFile = NULL;
char *basePath = NULL;
char *logName = NULL;
short tigerLogPort = 0;
int maxLogSize = 0;
int maxRecv = 0;
int maxMsg = 0;
int maxLogs = 0;

FILE *tigerLog = NULL;
char filePath[1024];
char backupPath1[1024];
char backupPath2[1024];

void archiveTigerLog(void);
void handleSignal(int sig);
void handleArchive(int sig);

void createNewLog(char *line);
void tigerLogIt(const char *fmt, ...);
void logIt(char *msg);
void freeTigerLogEntry(TigerLog *tl);
void deleteLog(char *line);
void archiveLog(char *line);

int allAscii(char *s );

int main(int argc, char *argv[]) {
	int sock;							// Socket
	struct sockaddr_in broadcastAddr;	// Broadcast Address
	unsigned short tigerLogPort;		// Port
	int recvStringLen;					// Length of received string

	if (signal(SIGINT, handleSignal) == SIG_ERR) {
		printf("Can't catch SIGINT\n");
    }

    if (signal(SIGTERM, handleSignal) == SIG_ERR) {
		printf("Can't catch SIGTERM\n");
    }

    if (signal(SIGUSR1, handleArchive) == SIG_ERR) {
		printf("Can't catch SIGUSR1\n");
    }

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-c") == 0) {
			cfgFile = strdup(argv[i+1]);
		}
	}

	struct servent *sp = getservbyname("tigerlog", "udp");
	if (sp == NULL) {
		printf("Getservbyname failed for tigerlog.\n");
		exit(1);
	}
	tigerLogPort = sp->s_port;

	if (cfgFile == NULL) {
		cfgFile = "/usr/local/etc/tigerlog/tigerlog.ini";
	}

	ini = iniCreate(cfgFile);

	basePath = iniGetValue(ini, "System", "basepath");
	logName = iniGetValue(ini, "System", "logName");
	char *ms = iniGetValue(ini, "System", "maxLogSize");
	
	if (ms != NULL) {
		maxLogSize = atoi(ms);
		maxLogSize *= MILLION;
	} else {
		maxLogSize = 5;
		maxLogSize *= MILLION;
	}

	char *mr = iniGetValue(ini, "System", "maxRecv");
	if (mr != NULL) {
		maxRecv = atoi(mr);
	} else {
		maxRecv = DEFAULT_MAXRECVSIZE;
	}

	char *ml = iniGetValue(ini, "System", "maxLogs");
	if (ml != NULL) {
		maxLogs = atoi(ml);
	} else {
		maxLogs = DEFAULT_MAXLOGS;
	}

	tigerLogs = (TigerLog *)calloc(maxLogs, sizeof(TigerLog));

	char recvString[maxRecv + 1];	// Buffer for received string

	// Create a best-effort datagram socket using UDP
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		tigerLogIt("socket() failed");
		return 1;
	}

	// Construct bind structure
	memset(&broadcastAddr, 0, sizeof(broadcastAddr));   // Zero out structure
	broadcastAddr.sin_family = AF_INET;                 // Internet address family
	if (inet_pton(AF_INET, "127.0.0.1", &broadcastAddr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }
	// broadcastAddr.sin_addr.s_addr = htonl(INADDR_ANY);  // Any incoming interface
	broadcastAddr.sin_port = tigerLogPort;      // Broadcast port


	// Bind to the broadcast port
	if (bind(sock, (struct sockaddr *) &broadcastAddr, sizeof(broadcastAddr)) < 0) {
		tigerLogIt("bind() failed");
		return 1;
	}

	char logMsg[maxRecv];
	memset(filePath, 0, sizeof(filePath));
	memset(backupPath1, 0, sizeof(backupPath1));
	memset(backupPath2, 0, sizeof(backupPath2));
	memset(logMsg, 0, sizeof(logMsg));

	sprintf(filePath, "%s/%s%s", basePath, logName, ".log");
	sprintf(backupPath1, "%s/%s%s", basePath, logName, ".log.1" );
	sprintf(backupPath2, "%s/%s%s", basePath, logName, ".log.2" );

	tigerLog = fopen(filePath, "a");	// append mode
	if (tigerLog == NULL) {
		tigerLogIt("fopen() failed for tigerlog");
		return 1;
	}

	tigerLogIt("*** %s started ***", APP_NAME);

	tigerLogIt("*** Compiled: %s ***", __TIMESTAMP__);

	// Create existing log files.
	DIR *dir;
	struct dirent *entry;

	dir = opendir(basePath);
	if (dir != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (entry->d_name[0] == '.')
				continue;
			if (strcmp(entry->d_name, "tigerlog.log") == 0)
				continue;

			char *s = strdup(entry->d_name);
			if (endsWith(s, ".log") == true) {
				char *p = strrchr(s, '.');
				if (p != NULL)
					*p = '\0';
			}
			// printf("Log: %s\n", s);

			char buf[2048];
			sprintf(buf, "NewLog:%s", s);
			createNewLog(buf);
		}

		closedir(dir);
	}

	fd_set readfds;
	struct timeval timeout;
	struct sockaddr_in client_addr;
    socklen_t client_len;

	for ( ;; ) {
		FD_ZERO(&readfds);
		FD_SET(sock, &readfds);

		timeout.tv_sec = 2;
        timeout.tv_usec = 0;

		int r = select(sock + 1, &readfds, NULL, NULL, &timeout);

		if (r < 0) {
        } else if (r == 0) {
        } else {
			// Receive a single datagram from the server
			memset(recvString, 0, sizeof(recvString));
			if ((recvStringLen = recvfrom(sock, recvString, maxRecv, 0, 
					(struct sockaddr *)&client_addr, &client_len)) < 0) {
				tigerLogIt("recvfrom() failed");
				continue;
			}

			if (recvStringLen < 0) {
				tigerLogIt("Error reading socket.\n");
				continue;
			}

			recvString[recvStringLen] = '\0';

			// printf("recvString: %s\n", recvString);

			if (strncmp(recvString, "N:", 2) == 0) {
				createNewLog(recvString);
			} else if (strncmp(recvString, "D:", 2) == 0) {
				// DelLog:logName
				deleteLog(recvString);
			} else if (strncmp(recvString, "A:", 2) == 0) {
				// ArcLog:logName
				archiveLog(recvString);
			} else if (strncmp(recvString, "L~", 2) == 0) {
				char *p = strrchr(recvString, '\n');
				if (p != NULL)
					*p = '\0';
				logIt(recvString);
			} else {
				tigerLogIt("Error: Unknown packet type %s", recvString);
			}
		}
	}

	if (tigerLog != NULL)
		fclose(tigerLog);

	close(sock);
	exit(0);
}

void createLogFile(TigerLog *tl) {

	if (tl == NULL)
		return;

	if (tl->path == NULL || tl->name == NULL)
		return;

}

void freeTigerLogEntry(TigerLog *tl) {
	if (tl->name != NULL) {
		free(tl->name);
	}
	if (tl->path != NULL) {
		free(tl->path);
	}
	if (tl->fd != NULL) {
		fclose(tl->fd);
	}
	tl->name = NULL;
	tl->path = NULL;
	tl->fd = NULL;
	tl->inUse = false;
}

void createNewLog(char *line) {
	char *args[16];

	// Message format for NewLog
	// NewLog:logName

	int n = parse(line, ":", args, 16);
	if (n != 2) {
		char msg[1024];
		sprintf(msg, "NewLog syntax failed for '%s'", line);
		tigerLogIt(msg);
		return;
	}

	// remove tailing .log from name if it exists.
	if (endsWith(args[2], ".log") == true) {
		char *p = strrchr(args[1], '.');
		if (p != NULL) {
			*p = '\0';
		}
	}
	
	// Check if it already exists.
	bool flag = false;
	TigerLog *tl = tigerLogs;
	for (int i = 0; i < maxLogs; i++) {
		if (tl->inUse == true && strcmp(args[1], tl->name) == 0) {
			if (tl->inUse == true) {
				flag = true;
			}
			break;
		}

		tl++;
	}

	if (flag == false) {
		// Does not exist, create enry.
		tl = tigerLogs;
		for (int i = 0; i < maxLogs; i++) {
			if (tl->inUse == false) {	// Find first free slot.
				freeTigerLogEntry(tl);

				tl->inUse = true;
				tl->name = strdup(args[1]);
				tl->path = strdup(basePath);
				flag = true;
				break;
			}

			tl++;
		}

		if (flag == true) {
			// create log file

			char path[2048];
			sprintf(path, "%s/%s.log", basePath, args[1]);
			tl->fd = fopen(path, "a");

			tigerLogIt("*** Starting new log %s ***", args[1]);
		}

	}

}

int allAscii(char *s) {
	int ret = 1;
	int len = 0;

	if (s == NULL)
		return ret;
	
	len = strlen(s);

	for (int i = 0; i < len; i++, s++) {
		if (isascii(*s) == 0) {
			ret = 0;
			break;
		}
	}

	return ret;
}

void archiveLog(char *line) {
	char *args[16];

	int n = parse(line, ":", args, 16);

	if (n != 2)
		return;
	
	bool flag = false;
	TigerLog *tl = tigerLogs;
	for (int i = 0; i < maxLogs; i++) {
		if (tl->inUse == true && strcmp(tl->name, args[1]) == 0) {
			flag = true;
		}
		tl++;
	}

	if (flag == false)
		return;

	fprintf(tl->fd, "Archive log, changing to new file.\n");
	fflush(tl->fd);
	fclose(tl->fd);

	char b1[2048];
	char b2[2048];

	sprintf(b1, "%s/%s.log.1", tl->path, tl->name);
	sprintf(b2, "%s/%s.log.2", tl->path, tl->name);

	if( access( b2, F_OK ) != -1 ) {
		remove( b2);
	}
	if( access(b1, F_OK ) != -1 ) {
		rename(b1, b2);
	}

	if( access( b1, F_OK ) != -1 ) {
		remove( b1);
	}

	if( access(filePath, F_OK ) != -1 ) {
		rename(filePath, backupPath1);
	}

	tigerLog = fopen(filePath, "a");
	if (tigerLog == NULL) {
		tigerLogIt("2. fopen() failed");
	}

	tigerLogIt("Log file '%s' archived.", tl->name);
}

void archiveTigerLog() {
	fprintf(tigerLog, "Archive log, changing to new file.\n");
	fflush(tigerLog);
	fclose(tigerLog);

	if( access( backupPath2, F_OK ) != -1 ) {
		remove( backupPath2);
	}
	if( access(backupPath1, F_OK ) != -1 ) {
		rename(backupPath1, backupPath2);
	}

	if( access( backupPath1, F_OK ) != -1 ) {
		remove( backupPath1);
	}

	if( access(filePath, F_OK ) != -1 ) {
		rename(filePath, backupPath1);
	}

	tigerLog = fopen(filePath, "a");
	if (tigerLog == NULL) {
		tigerLogIt("2. fopen() failed");
	}
}

void handleArchive(int sig) {

	archiveTigerLog();
}

void deleteLog(char *line) {
	char *args[16];

	int n = parse(line, ":", args, 16);
	if (n != 2) {
		return;
	}

	TigerLog *tl = tigerLogs;
	for (int i = 0; i < maxLogs; i++) {
		if (tl->inUse == true && strcmp(tl->name, args[1]) == 0) {
			tl->inUse = false;
			break;
		}
		tl++;
	}
}

void logIt(char *line) {
	char *args[16];
	char *b = NULL;

	// line format MsgLog~logName~message
	// So the message field can NOT contain the '~' character.

	b = strdup(line);

	int n = parse(b, "~", args, 16);

	if (n != 3) {
		tigerLogIt("Invald message format.");
		return;
	}
	
	bool flag = false;
	TigerLog *tl = tigerLogs;
	for (int i = 0; i < maxLogs; i++) {
		if (tl->inUse == false) {
			tl++;
			continue;
		}

		// printf("%s, %s\n", tl->name, args[1]);
		if (strcmp(tl->name, args[1]) == 0) {
			flag = true;
			break;
		}
		tl++;
	}

	if (flag == false) {
		char buf[1204];
		sprintf(buf, "Error: logName does not exist. %s", args[1]);
		tigerLogIt(buf);
		free(b);
		return;
	}

	struct tm *tm_info;
	struct timespec ts;
	char tbuf[32];
	char sbuf[32];
	char logMsg[4096];

	if (tl->fd == NULL) {
		free(b);
		return;
	}

	clock_gettime(CLOCK_REALTIME, &ts);
	tm_info = localtime(&ts.tv_sec);

	strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm_info);

	sprintf(sbuf, ".%03ld", (ts.tv_nsec / MILLION));

	strcat(tbuf, sbuf);

	sprintf(logMsg, "%s, %s\n", tbuf, args[2]);

	if (allAscii(logMsg)) {
		fwrite(logMsg, 1, strlen(logMsg), tl->fd);
		fflush(tl->fd);
	}	

	size_t size = ftell(tl->fd);
	// fprintf(stderr, "size %ld  %d\n", size, maxLogSize);
	if (size >= maxLogSize) {
		char buf[1024];
		sprintf("ArcLog:%s", args[2]);
		archiveLog(buf);
	}

	free(b);
}

void tigerLogIt(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	struct tm *tm_info;
	struct timespec ts;
	char tbuf[32];
	char sbuf[32];
	char logMsg[4096];
	char msg[maxRecv +1];

	if (tigerLog == NULL)
		return;

	clock_gettime(CLOCK_REALTIME, &ts);
	tm_info = localtime(&ts.tv_sec);

	strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm_info);

	sprintf(sbuf, ".%03ld", (ts.tv_nsec / MILLION));

	strcat(tbuf, sbuf);

	vsprintf(msg, fmt, ap);

	va_end(ap);

	sprintf(logMsg, "%s, %s\n", tbuf, msg);

	if (allAscii(logMsg)) {
		fwrite(logMsg, 1, strlen(logMsg), tigerLog);
		fflush(tigerLog);
	}	

	size_t size = ftell(tigerLog);
	// fprintf(stderr, "size %d  %d\n", size, maxLogSize);
	if (size >= maxLogSize) {
		archiveTigerLog();
	}
}

void handleSignal(int sig) {

    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);

	if(tigerLog != NULL)
		fclose(tigerLog);

	exit(1);
}
