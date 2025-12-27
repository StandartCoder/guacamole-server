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

#ifndef GUAC_RDP_CHANNELS_RDPCAM_STREAM_H
#define GUAC_RDP_CHANNELS_RDPCAM_STREAM_H

#include <guacamole/client.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * The maximum number of video frames to buffer in the RDPCAM stream.
 */
#define GUAC_RDPCAM_MAX_FRAMES 15

/**
 * The maximum size of a single video frame in bytes.
 */
#define GUAC_RDPCAM_MAX_FRAME_SIZE (1024 * 1024) // 1MB

/**
 * RDPCAM frame header structure (little-endian).
 */
typedef struct guac_rdpcam_frame_header {

    /**
     * Version number (must be 1).
     */
    uint8_t version;

    /**
     * Flags (bit 0: keyframe).
     */
    uint8_t flags;

    /**
     * Reserved field (must be 0).
     */
    uint16_t reserved;

    /**
     * Presentation timestamp in milliseconds.
     */
    uint32_t pts_ms;

    /**
     * Length of the following H.264 payload in bytes.
     */
    uint32_t payload_len;

} __attribute__((packed)) guac_rdpcam_frame_header;

/**
 * A single video frame in the RDPCAM queue.
 */
typedef struct guac_rdpcam_frame {

    /**
     * Presentation timestamp in milliseconds.
     */
    uint32_t pts_ms;

    /**
     * Whether this is a keyframe.
     */
    bool keyframe;

    /**
     * The frame data (H.264 Annex-B format).
     */
    uint8_t* data;

    /**
     * The length of the frame data in bytes.
     */
    size_t length;

    /**
     * Pointer to the next frame in the queue.
     */
    struct guac_rdpcam_frame* next;

} guac_rdpcam_frame;

/**
 * RDPCAM stream for buffering and managing video frames from the client.
 */
typedef struct guac_rdpcam_stream {

    /**
     * Lock for thread-safe access to the stream.
     */
    pthread_mutex_t lock;

    /**
     * Condition variable for signaling frame availability.
     */
    pthread_cond_t frame_available;

    /**
     * The guac_client instance handling the relevant RDP connection.
     */
    guac_client* client;

    /**
     * Head of the frame queue.
     */
    guac_rdpcam_frame* queue_head;

    /**
     * Tail of the frame queue.
     */
    guac_rdpcam_frame* queue_tail;

    /**
     * Current number of frames in the queue.
     */
    int queue_size;


    /**
     * Whether the stream is being destroyed.
     */
    bool stopping;

    /**
     * Whether streaming has been started (shared across all device channels).
     */
    bool streaming;

    /**
     * Number of available credits for sending frames (shared across all channels).
     * Protected by the stream lock.
     */
    uint32_t credits;

    /**
     * Stream index for the active stream (shared across all channels).
     * This is the stream index from StartStreamsRequest (typically 0).
     * Protected by the stream lock.
     */
    uint8_t stream_index;

    /**
     * Whether a device channel has claimed the sender role. Only one
     * channel should actively dequeue and transmit frames at a time.
     */
    bool has_active_sender;

    /**
     * Opaque pointer to the channel currently authorized to transmit samples.
     * If NULL, no sender is active. Protected by the stream lock.
     */
    void* active_sender_channel;

} guac_rdpcam_stream;

/**
 * Creates a new RDPCAM stream for the given client.
 *
 * @param client
 *     The guac_client instance handling the relevant RDP connection.
 *
 * @return
 *     A newly-allocated RDPCAM stream, or NULL if allocation fails.
 */
guac_rdpcam_stream* guac_rdpcam_create(guac_client* client);

/**
 * Destroys the given RDPCAM stream, freeing all associated resources.
 *
 * @param stream
 *     The RDPCAM stream to destroy.
 */
void guac_rdpcam_destroy(guac_rdpcam_stream* stream);

/**
 * Signals any threads waiting on the stream that shutdown is in progress, waking
 * them so they can terminate gracefully. The stream itself is not freed.
 *
 * @param stream
 *     The RDPCAM stream to signal.
 */
void guac_rdpcam_signal_stop(guac_rdpcam_stream* stream);

/**
 * Queues a fully-assembled RDPCAM frame within the stream. The frame data is
 * copied into an internal buffer, and the call fails if the stream is stopping,
 * the queue is full, or validation of the header/payload fails.
 *
 * @param stream
 *     The stream receiving the frame.
 *
 * @param data
 *     Pointer to the frame header followed by the H.264 payload.
 *
 * @param len
 *     Total number of bytes available at @p data.
 *
 * @return
 *     Non-zero if the frame was queued successfully, zero otherwise.
 */
bool guac_rdpcam_push(guac_rdpcam_stream* stream, const void* data, size_t len);

/**
 * Retrieves the next queued frame from the stream. Ownership of the returned
 * payload buffer is transferred to the caller.
 *
 * @param stream
 *     The stream to dequeue from.
 *
 * @param out_buf
 *     Receives a pointer to the payload buffer (must be freed by the caller).
 *
 * @param out_len
 *     Receives the payload length in bytes.
 *
 * @param out_keyframe
 *     Receives whether the frame represents a keyframe.
 *
 * @param out_pts_ms
 *     Receives the presentation timestamp, in milliseconds.
 *
 * @return
 *     Non-zero if a frame was returned, zero if the stream is stopping or empty.
 */
bool guac_rdpcam_pop(guac_rdpcam_stream* stream, uint8_t** out_buf, size_t* out_len,
                     bool* out_keyframe, uint32_t* out_pts_ms);

int guac_rdpcam_get_queue_size(guac_rdpcam_stream* stream);


#endif