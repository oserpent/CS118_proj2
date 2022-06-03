#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>

#include <stdbool.h>

// =====================================

#define RTO 500000       /* timeout in microseconds */
#define HDR_SIZE 12      /* header size*/
#define PKT_SIZE 524     /* total packet size */
#define PAYLOAD_SIZE 512 /* PKT_SIZE - HDR_SIZE */
#define WND_SIZE 10      /* window size*/
#define MAX_SEQN 25601   /* number of sequence numbers [0-25600] */

// Packet Structure: Described in Section 2.1.1 of the spec. DO NOT CHANGE!
struct packet
{
    unsigned short seqnum;
    unsigned short acknum;
    char syn;
    char fin;
    char ack;
    char dupack;
    unsigned int length;
    char payload[PAYLOAD_SIZE];
};

// Printing Functions: Call them on receiving/sending/packet timeout according
// Section 2.6 of the spec. The content is already conformant with the spec,
// no need to change. Only call them at correct times.
void printRecv(struct packet *pkt)
{
    printf("RECV %d %d%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN" : "", pkt->fin ? " FIN" : "", (pkt->ack || pkt->dupack) ? " ACK" : "");
}

void printSend(struct packet *pkt, int resend)
{
    if (resend)
        printf("RESEND %d %d%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN" : "", pkt->fin ? " FIN" : "", pkt->ack ? " ACK" : "");
    else
        printf("SEND %d %d%s%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN" : "", pkt->fin ? " FIN" : "", pkt->ack ? " ACK" : "", pkt->dupack ? " DUP-ACK" : "");
}

void printTimeout(struct packet *pkt)
{
    printf("TIMEOUT %d\n", pkt->seqnum);
}

// Building a packet by filling the header and contents.
// This function is provided to you and you can use it directly
void buildPkt(struct packet *pkt, unsigned short seqnum, unsigned short acknum, char syn, char fin, char ack, char dupack, unsigned int length, const char *payload)
{
    pkt->seqnum = seqnum;
    pkt->acknum = acknum;
    pkt->syn = syn;
    pkt->fin = fin;
    pkt->ack = ack;
    pkt->dupack = dupack;
    pkt->length = length;
    memcpy(pkt->payload, payload, length);
}

// =====================================

double setTimer()
{
    struct timeval e;
    gettimeofday(&e, NULL);
    return (double)e.tv_sec + (double)e.tv_usec / 1000000 + (double)RTO / 1000000;
}

int isTimeout(double end)
{
    struct timeval s;
    gettimeofday(&s, NULL);
    double start = (double)s.tv_sec + (double)s.tv_usec / 1000000;
    return ((end - start) < 0.0);
}

// =====================================

// SERVER

// COMMENT: The two functions below belong outside the main.

// DESCRIPTION:
// Checks whether the recvpkt is actually a pkt we're waiting for. I.e. Is it in window?
// If so, returns the index that recvpkt should be stored in. Else, returns -1.
// ANALYSIS: If -1 is returned, then recvpkt is outside the window and was already received.
int getRcvdPktIdx(int s, int e, struct packet *recvpkt, int *wndSeqs)
{
    int i = s;
    bool flag = true;
    while (i != e || (flag && i == e))
    {
        flag = false;
        if (recvpkt->seqnum == wndSeqs[i])
        {
            return i;
        }
        i = (i + 1) % WND_SIZE;
    }
    return -1;
}

// DESCRIPTION: Returns the first index of the pkt that is not yet received. Else, returns -1.
// ANALYSIS: If -1 is returned, then that means all pkts in window have been received. This probably means the pkt at s was the last to be received.
int getFirstNonRcvdIdx(int s, int e, bool *rcvd)
{
    int i = s;
    bool flag = true;
    while (i != e || (flag && i == e))
    {
        flag = false;
        if (!rcvd[i])
        {
            return i;
        }
        i = (i + 1) % WND_SIZE;
    }
    return -1;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        perror("ERROR: incorrect number of arguments\n");
        exit(1);
    }

    unsigned int servPort = atoi(argv[1]);

    // =====================================
    // Socket Setup

    int sockfd;
    struct sockaddr_in servaddr, cliaddr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(servPort);
    memset(servaddr.sin_zero, '\0', sizeof(servaddr.sin_zero));

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("bind() error");
        exit(1);
    }

    int cliaddrlen = sizeof(cliaddr);

    // NOTE: We set the socket as non-blocking so that we can poll it until
    //       timeout instead of getting stuck. This way is not particularly
    //       efficient in real programs but considered acceptable in this
    //       project.
    //       Optionally, you could also consider adding a timeout to the socket
    //       using setsockopt with SO_RCVTIMEO instead.
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    // =====================================

    unsigned short seqNum = (rand() * rand()) % MAX_SEQN;

    for (int i = 1;; i++)
    {
        // =====================================
        // Establish Connection: This procedure is provided to you directly and
        // is already working.

        int n;

        FILE *fp;

        struct packet synpkt, synackpkt, ackpkt;

        while (1)
        {
            n = recvfrom(sockfd, &synpkt, PKT_SIZE, 0, (struct sockaddr *)&cliaddr, (socklen_t *)&cliaddrlen);
            if (n > 0)
            {
                printRecv(&synpkt);
                if (synpkt.syn)
                    break;
            }
        }

        unsigned short cliSeqNum = (synpkt.seqnum + 1) % MAX_SEQN; // next message from client should have this sequence number

        buildPkt(&synackpkt, seqNum, cliSeqNum, 1, 0, 1, 0, 0, NULL);

        while (1)
        {
            printSend(&synackpkt, 0);
            sendto(sockfd, &synackpkt, PKT_SIZE, 0, (struct sockaddr *)&cliaddr, cliaddrlen);

            while (1)
            {
                // ackpacket is supposed response to synack
                n = recvfrom(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr *)&cliaddr, (socklen_t *)&cliaddrlen);
                if (n > 0)
                {
                    printRecv(&ackpkt);
                    // if statement signifies that connection is completed: server acknowledges client seq num and client acknowledges server seq num
                    if (ackpkt.seqnum == cliSeqNum && (ackpkt.ack || ackpkt.dupack) && ackpkt.acknum == (synackpkt.seqnum + 1) % MAX_SEQN)
                    {
                        // length = length of i + 6 (".file" + nullbyte)
                        int length = snprintf(NULL, 0, "%d", i) + 6;
                        char *filename = malloc(length);
                        snprintf(filename, length, "%d.file", i);

                        fp = fopen(filename, "w");
                        free(filename);
                        if (fp == NULL)
                        {
                            perror("ERROR: File could not be created\n");
                            exit(1);
                        }

                        fwrite(ackpkt.payload, 1, ackpkt.length, fp);

                        seqNum = ackpkt.acknum;
                        cliSeqNum = (ackpkt.seqnum + ackpkt.length) % MAX_SEQN;

                        buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 1, 0, 0, NULL);
                        printSend(&ackpkt, 0);
                        sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr *)&cliaddr, cliaddrlen);
                        // breaks if ack to synack successfully received => handshake complete
                        break;
                    }
                    // sending another synack in case of packet loss of original synack packet
                    else if (ackpkt.syn)
                    {
                        // has dupack bit turned on
                        buildPkt(&synackpkt, seqNum, (synpkt.seqnum + 1) % MAX_SEQN, 1, 0, 0, 1, 0, NULL);
                        // breaks after sending duplicate synack due to original synack packet loss
                        break;
                    }
                }
            }
            // this break is for the if condition of the inner while loop

            // ackpkt.syn means that ackpkt is a syn and so ack to synack is not received
            //! ackpkt.syn means that ackpkt is not a syn and so ack to synack is received
            // if ack to synack is received, then break
            // if ack to synack is not received, then continue outer loop
            // outer loop means we send another synack
            if (!ackpkt.syn)
                break;
        }

        // *** TODO: Implement the rest of reliable transfer in the server ***
        // Implement GBN for basic requirement or Selective Repeat to receive bonus

        // Note: the following code is not the complete logic. It only expects
        //       a single data packet, and then tears down the connection
        //       without handling data loss.
        //       Only for demo purpose. DO NOT USE IT in your final submission

        struct packet recvpkt;

        // COMMENT: Starting from here, insert code wherever appropriate in main.

        int full = 0;
        int s = 0;
        int e = 0;
        struct packet pkts[WND_SIZE];
        int wndSeqs[WND_SIZE]; // DESCRIPTION: Stores the pkt seq nums we're expecting to receive.
        bool rcvd[WND_SIZE];

        while (1)
        {

            // DESCRIPTION: INCREASE WINDOW SIZE PHASE

            int i = 0;
            while (full == 0)
            {
                rcvd[e] = false;
                wndSeqs[e] = (cliSeqNum + PAYLOAD_SIZE * i) % MAX_SEQN; // Q: IT'S PAYLOAD_SIZE RIGHT? NOT PKT_SIZE?
                i++;
                e = (e + 1) % WND_SIZE;
                if (s == e)
                {
                    full = 1;
                }
            }

            // DESCRIPTION: RECEIVE PKTS PHASE

            n = recvfrom(sockfd, &recvpkt, PKT_SIZE, 0, (struct sockaddr *)&cliaddr, (socklen_t *)&cliaddrlen);

            if (n > 0)
            {
                printRecv(&recvpkt);

                // COMMENT: Regardless of whether pkt is within/without window or whether an ack for pkt has been sent already, an ack should be sent again in case it was lost.
                buildPkt(&ackpkt, seqNum, recvpkt.seqnum, 0, 0, 1, 0, 0, NULL); // DOUBLE CHECK seqNum
                printSend(&ackpkt, 0);
                sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr *)&cliaddr, cliaddrlen);

                int idx = getRcvdPktIdx(s, e, &recvpkt, &wndSeqs);
                if (idx >= 0)
                {
                    if (!rcvd[idx])
                    {
                        pkts[idx] = recvpkt;
                        rcvd[idx] = true;
                    }
                    if (idx == s)
                    { // COMMENT: If it's not the first pkt that is received, then no need to do much more with regards to window. Else, do the following.
                        bool flag = true;
                        int temp = getFirstNonRcvdIdx(s, e, &rcvd);
                        int i = s;
                        if (temp != -1)
                        { // COMMENT: First nonreceived pkt is within the window.
                            while (i != temp || (flag && i == temp))
                            { // DESCRIPTION: saves consecutively received pkts
                                flag = false;
                                fwrite(&pkts[i].payload, 1, &pkts[i].length, fp);
                                i = (i + 1) % WND_SIZE;
                            }
                            s = temp;
                            cliSeqNum = wndSeqs[s]; // COMMENT: Given that pkt s is within the window and wndSeqs[s] represents the seq num expected at slot s, cliSeqNum should be just that.
                        }
                        else
                        { // COMMENT: When s is the last pkt received and all other pkts have been received already.
                            while (i != e || (flag && i == e))
                            { // DESCRIPTION: saves consecutively received pkts
                                flag = false;
                                fwrite(&pkts[i].payload, 1, &pkts[i].length, fp);
                                i = (i + 1) % WND_SIZE;
                            }
                            s = e;
                            cliSeqNum = (cliSeqNum + WND_SIZE * PAYLOAD_SIZE) % MAX_SEQN; // COMMENT: Since all pkts in window have been received, cliSeqNum can advance by the entirety of the window.
                        }
                    }
                }

                if (recvpkt.fin)
                {
                    cliSeqNum = (cliSeqNum + 1) % MAX_SEQN;

                    buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 1, 0, 0, NULL);
                    printSend(&ackpkt, 0);
                    sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr *)&cliaddr, cliaddrlen);

                    break;
                }
            }
        }

        // struct packet recvpkt;

        // while(1) {
        //     n = recvfrom(sockfd, &recvpkt, PKT_SIZE, 0, (struct sockaddr *) &cliaddr, (socklen_t *) &cliaddrlen);
        //     if (n > 0) {
        //         printRecv(&recvpkt);
        //         if (recvpkt.fin) {
        //             cliSeqNum = (cliSeqNum + 1) % MAX_SEQN;

        //             buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 1, 0, 0, NULL);
        //             printSend(&ackpkt, 0);
        //             sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);

        //             break;
        //         }
        //         //sending acks back to data packets
        //         cliSeqNum = (recvpkt.seqnum + recvpkt.length) % MAX_SEQN;
        //         buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 1, 0, 0, NULL);
        //         printSend(&ackpkt, 0);
        //         sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
        //     }
        // }

        // *** End of your server implementation ***

        fclose(fp);
        // =====================================
        // Connection Teardown: This procedure is provided to you directly and
        // is already working.

        struct packet finpkt, lastackpkt;
        buildPkt(&finpkt, seqNum, 0, 0, 1, 0, 0, 0, NULL);
        buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 0, 1, 0, NULL);

        printSend(&finpkt, 0);
        sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr *)&cliaddr, cliaddrlen);
        double timer = setTimer();

        while (1)
        {
            while (1)
            {
                n = recvfrom(sockfd, &lastackpkt, PKT_SIZE, 0, (struct sockaddr *)&cliaddr, (socklen_t *)&cliaddrlen);
                if (n > 0)
                    break;

                if (isTimeout(timer))
                {
                    printTimeout(&finpkt);
                    printSend(&finpkt, 1);
                    sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr *)&cliaddr, cliaddrlen);
                    timer = setTimer();
                }
            }

            printRecv(&lastackpkt);
            if (lastackpkt.fin)
            {

                printSend(&ackpkt, 0);
                sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr *)&cliaddr, cliaddrlen);

                printSend(&finpkt, 1);
                sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr *)&cliaddr, cliaddrlen);
                timer = setTimer();

                continue;
            }
            if ((lastackpkt.ack || lastackpkt.dupack) && lastackpkt.acknum == (finpkt.seqnum + 1) % MAX_SEQN)
                break;
        }

        seqNum = lastackpkt.acknum;
    }
}