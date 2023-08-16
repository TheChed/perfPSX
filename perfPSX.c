#define _POSIX_C_SOURCE 1
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "libxml/tree.h"
#include "libxml/xmlstring.h"
#include <stdio.h>
#include <libxml/parser.h>

#define EARTH_RAD 6371008 // earth radius in meters

#define LBSKG 0.45359237
#define MAXBUFF 50001
#define MINDISTANCE 1000

pthread_mutex_t mutex;
const char delim = ';';

FILE *flog;
int nbreport = 0, nbroutelegs = 0, nbxmllegs = 0;

int quit = 0;

typedef struct pos {
    char ID[10];
    char raw[500];
    int ATA;
    int ATAxml;
    double fuel, fuelxml;
    int ETA;
    double latitude;
    double latitudexml;
    double longitude;
    double longitudexml;
} pos;

pos currentPos;

pos RTE[500];

int socketID;
size_t bufmain_used = 0;
char bufmain[MAXBUFF];

void parsefix(xmlDocPtr doc, xmlNodePtr cur)
{
    xmlChar *key;
    cur = cur->xmlChildrenNode;

    while (cur != NULL) {
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"ident"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            printf("IDENT: %s\t", key);
            strcpy(RTE[nbxmllegs].ID, (char *)key);
            xmlFree(key);
            nbxmllegs++;
        }
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"time_total"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            printf("TIME: %s\t", key);
            RTE[nbxmllegs].ATAxml = strtol((char *)key, NULL, 10);
            xmlFree(key);
        }
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"fuel_plan_onboard"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            printf("FUEL: %s\n", key);
            RTE[nbxmllegs].fuelxml = strtol((char *)key, NULL, 10);
            xmlFree(key);
        }
        cur = cur->next;
    }
}

void parsenavlog(xmlDocPtr doc, xmlNodePtr cur)
{
    cur = cur->xmlChildrenNode;

    while (cur != NULL) {
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"fix"))) {
            parsefix(doc, cur);
        }
        cur = cur->next;
    }
}

int parseXML(const char *filename)
{
    xmlDocPtr doc;
    xmlNodePtr cur;

    doc = xmlParseFile(filename);

    if (doc == NULL) {
        fprintf(stderr, "Could not open xml file\n");
        return -1;
    }

    cur = xmlDocGetRootElement(doc);

    if (cur == NULL) {
        fprintf(stderr, "Empty Document\n");
        xmlFreeDoc(doc);
        return -1;
    }

    if (xmlStrcmp(cur->name, (const xmlChar *)"OFP")) {
        fprintf(stderr, "Document of wrong type, root node !=navlog\n");
        xmlFreeDoc(doc);
        return -1;
    }

    /* ----------------------------------
     * find the navlog node in the xml file
     * ---------------------------------*/
    cur = cur->xmlChildrenNode;

    while (cur != NULL) {
        if (!xmlStrcmp(cur->name, (const xmlChar *)"navlog")) {
            printf("cur->name: %s\n", cur->name);
            parsenavlog(doc, cur);
        }
        cur = cur->next;
    }

    xmlFreeDoc(doc);
    xmlCleanupParser();
    return 0;
}
int init_connect(void);

void insertleg(const char *raw, char *ID, int ETA, int fuel, double latitude, double longitude)
{
    if (!nbxmllegs) {
        printf("No XML route provided\n");
        return;
    }

    printf("Processing ID: %s\n",ID);
    for (int i = 0; i < nbxmllegs; ++i) {
        if (!strcmp(RTE[i].ID, ID)) {
            RTE[i].ATA = ETA;
            strncpy(RTE[i].raw, raw, 500);
            RTE[i].fuel = fuel;
            RTE[i].longitude = longitude;
            RTE[i].latitude = latitude;
        }
    }

    return;
}
void decode_leg(const char *leg)
{
    char *token, *val, *savptr;

    char ID[50];
    double latitude, longitude;
    int ETA, fuel;

    char *legcpy = malloc(1 + strlen(leg) * sizeof(char));

    strcpy(legcpy, leg);
    savptr = legcpy;

    token = strtok_r(legcpy, "'", &savptr); // Waypoint
    if (strlen(token) == 0) {
        return;
    }

    strncpy(ID, token,50);

    token = strtok_r(NULL, "'", &savptr); // via
    if(token==NULL) return; 

    token = strtok_r(NULL, "'", &savptr); // Lat and Long
    if(token==NULL) return; 
    if ((val = memchr(token, '/', strlen(token)))) {
        longitude = strtof(val + 1, NULL);
        latitude = strtof(token, NULL);
    } else {
        longitude = 0;
        latitude = 0;
    }

    if(token==NULL) return; 
    token = strtok_r(NULL, "'", &savptr); // ETA
    ETA = strtol(token, NULL, 10);
    if(token==NULL) return; 

    token = strtok_r(NULL, "'", &savptr); // Fuel
    if(token==NULL) return; 
    fuel = strtol(token, NULL, 10);

    insertleg(leg, ID, ETA, fuel, latitude, longitude);
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

void formattime(int sec, char *ATA)
{

    int hours, minutes;

    hours = sec / 3600;
    minutes = (sec - (3600 * hours)) / 60;
    snprintf(ATA, 6, "%02d.%02d", hours, minutes);
    return;
}

void decode_time(char *s)
{
    int seconds;

    seconds = strtol(strtok(s + 6, ";"), NULL, 10) / 1000;
    currentPos.ATA = seconds;
    return;
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
    char ATA[6];

    distance = dist(currentPos.latitude, RTE[0].latitude, currentPos.longitude, RTE[0].longitude);

    if (distance < MINDISTANCE && printOK) {

        formattime(currentPos.ATA, ATA);
        printf("%5s|\t%s\t|\t%.1f\t|\t%04dZ\n", RTE[0].ID, ATA, currentPos.fuel, currentPos.ETA);
         fprintf(flog, "%5s,%s,%.1f,%04dZ\n", RTE[0].ID, ATA, currentPos.fuel, currentPos.ETA);
        fflush(flog);
        printOK = 0;
    }
    printOK = (distance >= MINDISTANCE);
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
    parseXML(argv[1]);
    printf("Created %d legs\n", nbxmllegs);

    printf(" WPT |\tLapsed\t|\t Fuel \t|\t ETA \n");
    fprintf(flog, "WPT,Lapsed,Fuel,ETA\n");
    if (pthread_create(&t1, NULL, &ptUmain, NULL) != 0) {

        printf("Error creating thread Umain");
    }

    pthread_join(t1, NULL);

    return 0;
}
