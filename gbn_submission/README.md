# CS118 Project 2 Go-Back-N

Authors: James Youn, Orrin Zhong</br>
Emails: jyoun0921@g.ucla.edu, uclaorrin@ucla.edu</br>
UIDs: 505399332, 605392655</br>


## High-Level Design

The client and server both adhere to the Go Back N Protocol. The client sends a SYN pkt to the server and once an ACK from the server is received, the client then sends the ACK-DATA pkt along with however many pkts are left in the window. From here, the server responds with an ACK message whenever it receives a payload from the client and only shifts the window to the right when the first packet in the window is received. A timer is set for the start of the window and resets every time the window shifts. Therefore, ACKs to packets that are not at the start of the window are ignored and do not reset the timer. When the server receives data packets that are not the next expected packet from the client, they are dropped and an ACK for the next expected packet is sent back. When the client reaches the end of the file to send, it sends a FIN packet, which the server ACKs and sends a FIN back, which the client receives and ACKs back, then closes. The server now waits for another client file.

No additional libraries were used aside from boolean and the ones provided in the skeleton code.


## Problems & Solutions

There was a problem when we accidentally excluded the header size when sending the file. So the last 12 bytes of the payload were always cut off for some reason. We also had a problem with sending large files and found that it always looped when the sequence number of the data packet sent by the client is larger than the maximum sequence number. This is because the code to accept ACKs did not use modulus, so it did not recognize that the ACK for a packet close to the maximum sequence number was an ACK for itself.