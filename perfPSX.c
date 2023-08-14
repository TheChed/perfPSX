#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXBUFF 8192
#define DELIM ";";

pthread_mutex_t mutex;
const char delim = ';';

int nbiter = 0, nbmain = 0, nbboost = 0;

int quit = 0;

void Decode(char *s);

int socketID;
size_t bufmain_used = 0;
char bufmain[MAXBUFF];

int init_connect(void);

void decode_RTE(char *s)
{

    char *token;
/*-------------------------------------------
 * Ignore the first two tokens in the route
 * ----------------------------------------*/

    token = strtok(s, "#");
	token = strtok(NULL, ";");

    while (token != NULL) {
        printf("%s\n", token+10);
        token = strtok(NULL, ";");
    }
}

int sendQPSX(const char *s)
{

    char *dem = (char *)malloc((strlen(s) + 1) * sizeof(char));

    strncpy(dem, s, strlen(s));
    dem[strlen(s)] = 10;

    int nbsend = send(socketID, dem, strlen(dem), 0);

    if (nbsend == 0) {
        printf("Error sending variable %s to PSX\n", s);
        printf("only sent: %d bytes\n", nbsend);
    }
    free(dem);
    return nbsend;
}

int init_connect(void)
{

    struct sockaddr_in PSXMAIN;

    socketID = socket(AF_INET, SOCK_STREAM, 0);

    PSXMAIN.sin_family = AF_INET;
    PSXMAIN.sin_port = htons(10747);
    PSXMAIN.sin_addr.s_addr = inet_addr("127.0.0.1");
    /* Now connect to the server */
    if (connect(socketID, (struct sockaddr *)&PSXMAIN, sizeof(PSXMAIN)) < 0) {
        perror("ERROR connecting to main server");
        exit(1);
    }
    return 0;
}

int umain(const char *Q)
{
    size_t bufmain_remain = sizeof(bufmain) - bufmain_used;
    nbmain++;
    if (bufmain_remain == 0) {
        printf("Main socket line exceeded buffer length! Discarding input");
        bufmain_used = 0;
        // printf(bufmain, 0);
        return 0;
    }

    int nbread =
        recv(socketID, (char *)&bufmain[bufmain_used], bufmain_remain, 0);

    if (nbread == 0) {
        printf("Main socket connection closed.");
        return 0;
    }
    if (nbread < 0 && errno == EAGAIN) {
        printf("No data received.");
        /* no data for now, call back when the socket is readable */
        return 0;
    }
    if (nbread < 0) {
        printf("Main socket Connection error");
        return 0;
    }
    bufmain_used += nbread;

    /* Scan for newlines in the line buffer; we're careful here to deal with
     * embedded \0s an evil server may send, as well as only processing lines
     * that are complete.
     */
    char *line_start = bufmain;
    char *line_end;
    while ((line_end = (char *)memchr((void *)line_start, '\n',
                                      bufmain_used - (line_start - bufmain)))) {
        *line_end = 0;
        if (strstr(line_start, "Qs377")) {
            Decode(line_start);
        }
        line_start = line_end + 1;
    }
    /* Shift buffer down so the unprocessed data is at the start */
    bufmain_used -= (line_start - bufmain);
    memmove(bufmain, line_start, bufmain_used);
    return nbread;
}

void *ptUmain(void *thread_param)
{
    (void)(thread_param);
    while (!quit) {
        umain(NULL);
    }
    printf("Exiting ptUmain\n");
    return NULL;
}
void Decode(char *s)
{
	decode_RTE(s);
}

int main(int argc, char **argv)
{
    pthread_t t1;
    (void)argc;
    init_connect();
    if (pthread_create(&t1, NULL, &ptUmain, NULL) != 0) {

        printf("Error creating thread Umain");
    }

    pthread_join(t1, NULL);

    return 0;
}
