#define _POSIX_C_SOURCE 1
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define EARTH_RAD 6371008 // earth radius in meters

#define LBSKG 0.45359237
#define MAXBUFF 50001

pthread_mutex_t mutex;
const char delim = ';';

FILE *flog;
int nbreport = 0, nbroutelegs = 0;

int quit = 0;

typedef struct pos {
    char ID[10];
    char ATA[6];
    double fuel;
    int ETA;
    double latitude;
    double longitude;
} pos;

pos currentPos;
pos posreport[500];

struct WP {
    char raw[100];
    char ID[10];
    char via[30];
    float latitude;
    float longitude;
    int ETA;
    int FUEL;
} RTE[500];

int socketID;
size_t bufmain_used = 0;
char bufmain[MAXBUFF];

int init_connect(void);

void decode_leg(const char *leg)
{
    char *legcpy = malloc((strlen(leg) + 1) * sizeof(char));
    char *token, *val, *savptr;

    if (legcpy == NULL) {
        return;
    }

    strncpy(legcpy, leg, strlen(leg));
    savptr = legcpy;

    strcpy(RTE[nbroutelegs].raw, legcpy);   // SAving raw data
    token = strtok_r(legcpy, "'", &savptr); // Waypoint
    if (strlen(token) == 0) {
        free(legcpy);
        return;
    }

    strcpy(RTE[nbroutelegs].ID, token);

    token = strtok_r(NULL, "'", &savptr); // via
    strcpy(RTE[nbroutelegs].via, token);

    token = strtok_r(NULL, "'", &savptr); // Lat and Long
    if ((val = memchr(token, '/', strlen(token)))) {
        RTE[nbroutelegs].longitude = strtof(val + 1, NULL);
        RTE[nbroutelegs].latitude = strtof(token, NULL);
    }

    token = strtok_r(NULL, "'", &savptr); // ETA
    RTE[nbroutelegs].ETA = strtol(token, NULL, 10);

    token = strtok_r(NULL, "'", &savptr); // ETA
    if (token)
        RTE[nbroutelegs].FUEL = strtol(token, NULL, 10);

    nbroutelegs++;
    free(legcpy);
}

void decode_fuel(char *s)
{

    double fuelQTY = 0;

    fuelQTY += strtof(strtok(s + 7, ";"), NULL) * LBSKG / 10.0;
    for (int i = 0; i < 8; ++i) {
        fuelQTY += strtof(strtok(NULL, ";"), NULL) * LBSKG / 10.0;
    }
    currentPos.fuel = fuelQTY / 1000.0;
}
void decode_pos(char *s)
{
    double latitude, longitude;

    strtok(s + 6, ";");
    for (int i = 0; i < 4; ++i) {
        strtok(NULL, ";");
    }
    latitude = strtof(strtok(NULL, ";"), NULL);
    longitude = strtof(strtok(NULL, ";"), NULL);
    currentPos.latitude = latitude;
    currentPos.longitude = longitude;
}

long decode_time(char *s)
{
    char ATA[6];
    int hours, minutes, seconds;

    seconds = strtol(strtok(s + 6, ";"), NULL, 10) / 1000;
    hours = seconds / 3600;
    minutes = (seconds - (3600 * hours)) / 60;
    seconds = (seconds - (3600 * hours) - (minutes * 60));

    snprintf(ATA, 6, "%02d.%02d", hours, minutes);
    strncpy(currentPos.ATA, ATA, 6);

    return strtol(strtok(s + 6, ";"), NULL, 10);
}
void decode_RTE(char *s)
{

    char *token;

    token = strtok(s, "#");
    /*-------------------------------------------
     * Ignore the first two tokens in the route
     * ----------------------------------------*/
    token = strtok(NULL, ";");
    token = strtok(NULL, ";");

    token = strtok(NULL, ";");
    while (token != NULL) {
        decode_leg(token + 10);
        token = strtok(NULL, ";");
    }
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
double dist(double lat1, double lat2, double long1, double long2)
{
    return 2 * EARTH_RAD *
           (sqrt(pow(sin((lat2 - lat1) / 2), 2) +
                 cos(lat1) * cos(lat2) * pow(sin((long2 - long1) / 2), 2)));
}

void log_position(void)
{

    double distance;
    static int printOK = 0;

    distance = dist(currentPos.latitude, RTE[0].latitude, currentPos.longitude, RTE[0].longitude);

    if (distance < 200.0 && printOK) {

        printf("%5s|\t%s\t|\t%.1f\t|\t%dZ\n", RTE[0].ID, currentPos.ATA, currentPos.fuel, currentPos.ETA);
        fprintf(flog,"%5s;%s;%.1f;%dZ\n", RTE[0].ID, currentPos.ATA, currentPos.fuel, currentPos.ETA);
        fflush(flog);
        printOK = 0;
    }
    printOK = (distance >= 200.0);
}

int umain(const char *Q)
{
    static int active = -1;
    char rte[5];

    strncpy(rte, "XXXXX", 5);
    size_t bufmain_remain = sizeof(bufmain) - bufmain_used;
    if (bufmain_remain == 0) {
        printf("Main socket line exceeded buffer length! Discarding input");
        bufmain_used = 0;
        return 0;
    }

    int nbread =
        recv(socketID, (char *)&bufmain[bufmain_used], bufmain_remain, 0);

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

        if (strstr(line_start, "Qs373")) {
            active = line_start[8] - '0';
        }
        if (strstr(line_start, "Qi247")) {
            currentPos.ETA = strtol(strtok(line_start + 6, ";"), NULL, 10);
        }
        if (active == 1) {
            strncpy(rte, "Qs376", 5);
        }
        if (active == 2) {
            strncpy(rte, "Qs377", 5);
        }

        if (strstr(line_start, rte)) {
            nbroutelegs = 0;
            decode_RTE(line_start);
        }
        if (strstr(line_start, "Qs438")) {
            decode_fuel(line_start);
        }
        if (strstr(line_start, "Qs126")) {
            decode_time(line_start);
        }
        if (strstr(line_start, "Qs121")) {
            decode_pos(line_start);
        }

        // if we are close to a waypoint
        // in a route then log the position
        log_position();

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

int main(int argc, char **argv)
{
    pthread_t t1;
    (void)argc;
    init_connect();
    flog = fopen("WPT.log", "w");
    if (flog == NULL) {
        printf("Cannot create log file. Exiting\n");
        return -1;
    }
    printf(" WPT |\tLapsed\t|\t Fuel \t|\t ETA \n");
    fprintf(flog,"WPT;Lapsed;Fuel;ETA\n");
    if (pthread_create(&t1, NULL, &ptUmain, NULL) != 0) {

        printf("Error creating thread Umain");
    }

    pthread_join(t1, NULL);

    return 0;
}
