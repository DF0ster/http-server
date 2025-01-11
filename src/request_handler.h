#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <openssl/ssl.h>

void handle_request(SSL* ssl);

#endif // REQUEST_HANDLER_H
