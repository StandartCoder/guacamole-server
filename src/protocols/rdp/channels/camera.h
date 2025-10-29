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

/**
 * Webcam/camera redirection support for RDP. Allows the client's webcam
 * to be redirected to the RDP server, making it available to applications
 * running on the remote desktop.
 */
typedef struct guac_rdp_camera {

    /**
     * The client that owns this camera redirection.
     */
    guac_client* client;

    /**
     * Path to the video device (e.g., /dev/video0).
     */
    char* device_path;

    /**
     * Whether camera redirection is currently active.
     */
    int active;

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

#endif
