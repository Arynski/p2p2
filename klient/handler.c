#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "common/network.h"
#include "common/protocol_STUN.h"

size_t build_frame(uint8_t *buf, uint8_t type, const void *payload, uint16_t payload_len) {
    struct msg_header *hdr = (struct msg_header *)buf;
    hdr->type = type;
    hdr->payload_len = payload_len;
    if (payload && payload_len > 0)
        memcpy(buf + sizeof(struct msg_header), payload, payload_len);
    return sizeof(struct msg_header) + payload_len;
}