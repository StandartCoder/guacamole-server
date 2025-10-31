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

#ifndef GUAC_RDP_CHANNELS_CAMERA_H
#define GUAC_RDP_CHANNELS_CAMERA_H

#include <freerdp/freerdp.h>
#include <guacamole/client.h>
#include <guacamole/stream.h>

/**
 * Webcam/camera redirection support for RDP. Receives video stream data
 * from browser clients and redirects it to the RDP server, making the
 * browser's webcam available to applications running on the remote desktop.
 */
typedef struct guac_rdp_camera {

    /**
     * The client that owns this camera redirection.
     */
    guac_client* client;

    /**
     * Virtual device path for the camera pipe (e.g., /tmp/guac_camera_XXXX).
     */
    char* virtual_device_path;

    /**
     * Whether camera redirection is currently active.
     */
    int active;

    /**
     * The video stream from the browser client.
     */
    guac_stream* video_stream;

    /**
     * File descriptor for the virtual camera device pipe.
     */
    int device_fd;

} guac_rdp_camera;

/**
 * Allocates a new camera redirection module.
 *
 * @param client
 *     The guac_client associated with the RDP session.
 *
 * @return
 *     A new camera redirection module.
 */
guac_rdp_camera* guac_rdp_camera_alloc(guac_client* client);

/**
 * Frees the given camera redirection module.
 *
 * @param camera
 *     The camera module to free.
 */
void guac_rdp_camera_free(guac_rdp_camera* camera);

/**
 * Adds FreeRDP's camera redirection plugin to the list of dynamic virtual
 * channel plugins to be loaded. The plugin will only be loaded once the
 * "drdynvc" plugin is loaded.
 *
 * @param context
 *     The rdpContext associated with the active RDP session.
 */
void guac_rdp_camera_load_plugin(rdpContext* context);

/**
 * Handles incoming video stream data from the browser client. This function
 * writes the received video data to the virtual camera device pipe.
 *
 * @param camera
 *     The camera module receiving the video data.
 *
 * @param data
 *     The video data received from the browser.
 *
 * @param length
 *     The length of the received video data.
 *
 * @return
 *     Zero if successful, non-zero if an error occurs.
 */
int guac_rdp_camera_handle_video_data(guac_rdp_camera* camera,
        const void* data, int length);

/**
 * Starts the camera video stream. Creates a virtual device and begins
 * accepting video data from the browser client.
 *
 * @param camera
 *     The camera module to start.
 *
 * @return
 *     Zero if successful, non-zero if an error occurs.
 */
int guac_rdp_camera_start_stream(guac_rdp_camera* camera);

/**
 * Stops the camera video stream and cleans up the virtual device.
 *
 * @param camera
 *     The camera module to stop.
 */
void guac_rdp_camera_stop_stream(guac_rdp_camera* camera);

/**
 * Handler for blob instructions received over camera video streams.
 * This receives video data from the browser and writes it to the virtual device.
 *
 * @param user
 *     The user receiving the blob.
 *
 * @param stream
 *     The video stream.
 *
 * @param data
 *     The video data received.
 *
 * @param length
 *     The length of the video data.
 *
 * @return
 *     Zero if successful, non-zero if an error occurs.
 */
int guac_rdp_camera_blob_handler(guac_user* user, guac_stream* stream,
        void* data, int length);

/**
 * Handler for end instructions received over camera video streams.
 * Cleans up when the video stream ends.
 *
 * @param user
 *     The user ending the stream.
 *
 * @param stream
 *     The video stream that has ended.
 *
 * @return
 *     Zero if successful, non-zero if an error occurs.
 */
int guac_rdp_camera_end_handler(guac_user* user, guac_stream* stream);

#endif
