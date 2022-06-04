# CS118 Project 2 Selective Repeat

Authors: James Youn, Orrin Zhong
Emails: jyoun0921@g.ucla.edu, uclaorrin@ucla.edu
UIDs: 505399332, 605392655


## High-Level Design

The client and server both adhere to the Selective Repeat Protocol. The client sends a SYN pkt to the server and once an ACK from the server is received, the client then sends the ACK-DATA pkt along with however many pkts are left in the window. From here, the server responds with an ACK message whenever it receives a payload from the client and updates its rcvd window accordingly. Whenever the client receives an ACK from the server, it updates the window if the pkt is the first element of window, and client proceeds to send more pkts given that the window size just shrunk. Both keep track of the ACKed and RCVd pkts.

No additional libraries were used aside from boolean and the ones provided in the skeleton code.


## Problems & Solutions

The most time-consuming problem was keeping track of the expected sequence numbers on the server side. Since the server constantly maintains a Rcvd Wnd of a given size, it must check whether the pkt just received from the client is one that it's expecting. The circular buffer exacerbated this problem because the index of the buffer needs to correspond to the index of the array that contains the expected pkt seq nums. Eventually, this issue was handled by utilizing cliSeqNum and adding PAYLOAD_SIZE to this previous value everytime the window was enlarged. cliSeqNum was then updated as well so as to be consistent in the very next loop iteration.
