#ifndef MESSAGES_H
#define MESSAGES_H

// Message sent from Dealer to Worker and from Generator to Dealer
typedef struct {
    int job;  // ID
    int data;
} MQ_REQUEST_MESSAGE_WORKER;

// Message sent from Worker to Dealer
typedef struct {
    int job;  // Id
    int result;
    int worker;
} MQ_RESPONSE_MESSAGE;

#endif
