/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "rdpcam-stream.h"

#include <guacamole/client.h>
#include <guacamole/mem.h>
#include <guacamole/protocol.h>
#include <guacamole/socket.h>
#include <guacamole/stream.h>
#include <guacamole/user.h>

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

guac_rdpcam_stream* guac_rdpcam_create(guac_client* client) {

    guac_rdpcam_stream* stream = guac_mem_zalloc(sizeof(guac_rdpcam_stream));

    if (!stream) {
        guac_client_log(client, GUAC_LOG_ERROR, "Failed to allocate RDPCAM stream");
        return NULL;
    }

    if (pthread_mutex_init(&stream->lock, NULL) != 0) {
        guac_client_log(client, GUAC_LOG_ERROR, "Failed to initialize RDPCAM stream mutex");
        guac_mem_free(stream);
        return NULL;
    }

    if (pthread_cond_init(&stream->frame_available, NULL) != 0) {
        guac_client_log(client, GUAC_LOG_ERROR, "Failed to initialize RDPCAM stream condition variable");
        pthread_mutex_destroy(&stream->lock);
        guac_mem_free(stream);
        return NULL;
    }

    stream->client = client;
    stream->queue_head = NULL;
    stream->queue_tail = NULL;
    stream->queue_size = 0;
    stream->stopping = false;
    stream->streaming = false;
    stream->credits = 0;
    stream->stream_index = 0;
    stream->has_active_sender = false;
    stream->active_sender_channel = NULL;

    guac_client_log(client, GUAC_LOG_DEBUG, "RDPCAM stream created");

    return stream;

}

void guac_rdpcam_destroy(guac_rdpcam_stream* stream) {

    if (!stream)
        return;
    pthread_mutex_lock(&stream->lock);

    stream->stopping = true;

    /* Drain any queued frames before releasing the stream. */
    guac_rdpcam_frame* frame = stream->queue_head;
    while (frame) {
        guac_rdpcam_frame* next = frame->next;
        guac_mem_free(frame->data);
        guac_mem_free(frame);
        frame = next;
    }

    stream->queue_head = NULL;
    stream->queue_tail = NULL;
    stream->queue_size = 0;

    pthread_cond_broadcast(&stream->frame_available);

    pthread_mutex_unlock(&stream->lock);

    pthread_cond_destroy(&stream->frame_available);
    pthread_mutex_destroy(&stream->lock);

    guac_mem_free(stream);

}


void guac_rdpcam_signal_stop(guac_rdpcam_stream* stream) {

    if (!stream)
        return;

    pthread_mutex_lock(&stream->lock);
    if (!stream->stopping)
        stream->stopping = true;

    pthread_cond_broadcast(&stream->frame_available);
    pthread_mutex_unlock(&stream->lock);
}


bool guac_rdpcam_push(guac_rdpcam_stream* stream, const void* data, size_t len) {

    guac_client* client = stream ? stream->client : NULL;

    if (!stream || !data || len == 0) {
        if (client)
            guac_client_log(client, GUAC_LOG_WARNING, "RDPCAM push called with invalid parameters: stream=%p, data=%p, len=%zu", stream, data, len);
        return false;
    }

    pthread_mutex_lock(&stream->lock);

    /* Reject new frames once destruction has begun. */
    if (stream->stopping) {
        guac_client_log(stream->client, GUAC_LOG_DEBUG, "RDPCAM stream is stopping, rejecting frame");
        pthread_mutex_unlock(&stream->lock);
        return false;
    }

    /* Prevent unbounded growth when the consumer is back-pressured. */
    if (stream->queue_size >= GUAC_RDPCAM_MAX_FRAMES) {
        pthread_mutex_unlock(&stream->lock);
        return false;
    }

    if (len < sizeof(guac_rdpcam_frame_header)) {
        guac_client_log(stream->client, GUAC_LOG_WARNING, "RDPCAM frame too small: %zu bytes (expected at least %zu)", 
                       len, sizeof(guac_rdpcam_frame_header));
        pthread_mutex_unlock(&stream->lock);
        return false;
    }

    const guac_rdpcam_frame_header* header = (const guac_rdpcam_frame_header*) data;
    
    if (header->version != 1) {
        guac_client_log(stream->client, GUAC_LOG_WARNING, "RDPCAM frame has invalid version: %d", header->version);
        pthread_mutex_unlock(&stream->lock);
        return false;
    }

    if (header->payload_len > GUAC_RDPCAM_MAX_FRAME_SIZE) {
        guac_client_log(stream->client, GUAC_LOG_WARNING, "RDPCAM frame payload too large: %u bytes", header->payload_len);
        pthread_mutex_unlock(&stream->lock);
        return false;
    }

    size_t expected_total_len = sizeof(guac_rdpcam_frame_header) + header->payload_len;
    if (len != expected_total_len) {
        guac_client_log(stream->client, GUAC_LOG_WARNING, "RDPCAM frame length mismatch: got %zu bytes, expected %zu (header: %zu + payload: %u)", 
                        len, expected_total_len, sizeof(guac_rdpcam_frame_header), header->payload_len);
        pthread_mutex_unlock(&stream->lock);
        return false;
    }

    guac_rdpcam_frame* frame = guac_mem_zalloc(sizeof(guac_rdpcam_frame));
    if (!frame) {
        guac_client_log(stream->client, GUAC_LOG_ERROR, "Failed to allocate RDPCAM frame");
        pthread_mutex_unlock(&stream->lock);
        return false;
    }

    frame->data = guac_mem_alloc(header->payload_len);
    if (!frame->data) {
        guac_client_log(stream->client, GUAC_LOG_ERROR, "Failed to allocate RDPCAM frame data");
        guac_mem_free(frame);
        pthread_mutex_unlock(&stream->lock);
        return false;
    }

    const uint8_t* payload_start = (const uint8_t*) data + sizeof(guac_rdpcam_frame_header);
    memcpy(frame->data, payload_start, header->payload_len);
    frame->length = header->payload_len;
    frame->pts_ms = header->pts_ms;
    frame->keyframe = (header->flags & 0x01) != 0;
    frame->next = NULL;
    
    if (stream->queue_tail) {
        stream->queue_tail->next = frame;
        stream->queue_tail = frame;
    } else {
        stream->queue_head = frame;
        stream->queue_tail = frame;
    }
    stream->queue_size++;

    guac_client_log(stream->client, GUAC_LOG_TRACE, "RDPCAM frame queued: %zu bytes, keyframe=%s, pts=%u ms, queue_size=%d/%d",
                    frame->length, frame->keyframe ? "yes" : "no", frame->pts_ms, stream->queue_size, GUAC_RDPCAM_MAX_FRAMES);

    int utilization = (stream->queue_size * 100) / GUAC_RDPCAM_MAX_FRAMES;
    if (utilization >= 80) {
        guac_client_log(stream->client, GUAC_LOG_DEBUG, "RDPCAM queue utilization: %d%% (%d/%d frames)", 
                        utilization, stream->queue_size, GUAC_RDPCAM_MAX_FRAMES);
    }

    pthread_cond_signal(&stream->frame_available);

    pthread_mutex_unlock(&stream->lock);

    return true;

}

bool guac_rdpcam_pop(guac_rdpcam_stream* stream, uint8_t** out_buf, size_t* out_len,
                     bool* out_keyframe, uint32_t* out_pts_ms) {

    if (!stream || !out_buf || !out_len || !out_keyframe || !out_pts_ms)
        return false;

    pthread_mutex_lock(&stream->lock);

    /* Sleep until a frame arrives or destruction is requested. */
    while (stream->queue_size == 0 && !stream->stopping) {
        pthread_cond_wait(&stream->frame_available, &stream->lock);
    }

    if (stream->stopping || stream->queue_size == 0) {
        pthread_mutex_unlock(&stream->lock);
        return false;
    }

    guac_rdpcam_frame* frame = stream->queue_head;
    stream->queue_head = frame->next;
    if (!stream->queue_head) {
        stream->queue_tail = NULL;
    }
    stream->queue_size--;

    *out_buf = frame->data;
    *out_len = frame->length;
    *out_keyframe = frame->keyframe;
    *out_pts_ms = frame->pts_ms;

    guac_client_log(stream->client, GUAC_LOG_TRACE, "RDPCAM frame popped: %zu bytes, keyframe=%s, pts=%u ms, queue_size=%d/%d",
                    frame->length, frame->keyframe ? "yes" : "no", frame->pts_ms, stream->queue_size, GUAC_RDPCAM_MAX_FRAMES);

    if (stream->queue_size == 0) {
        guac_client_log(stream->client, GUAC_LOG_DEBUG, "RDPCAM queue is now empty");
    } else if (stream->queue_size <= 3) {
        guac_client_log(stream->client, GUAC_LOG_DEBUG, "RDPCAM queue is low: %d/%d frames remaining", stream->queue_size, GUAC_RDPCAM_MAX_FRAMES);
    }

    guac_mem_free(frame);

    pthread_mutex_unlock(&stream->lock);

    return true;

}

int guac_rdpcam_get_queue_size(guac_rdpcam_stream* stream) {

    if (!stream)
        return 0;

    pthread_mutex_lock(&stream->lock);
    int size = stream->queue_size;
    pthread_mutex_unlock(&stream->lock);

    return size;

}