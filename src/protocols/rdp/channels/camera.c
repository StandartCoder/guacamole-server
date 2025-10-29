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

#include "channels/camera.h"
#include "plugins/channels.h"
#include "rdp.h"
#include "settings.h"

#include <freerdp/freerdp.h>
#include <freerdp/client/camera.h>
#include <guacamole/client.h>
#include <guacamole/mem.h>

#include <stdlib.h>
#include <string.h>

guac_rdp_camera* guac_rdp_camera_alloc(guac_client* client) {

    guac_rdp_camera* camera = guac_mem_alloc(sizeof(guac_rdp_camera));
    camera->client = client;
    camera->device_path = NULL;
    camera->active = 0;

    return camera;

}

void guac_rdp_camera_free(guac_rdp_camera* camera) {

    if (camera == NULL)
        return;

    guac_mem_free(camera->device_path);
    guac_mem_free(camera);

}

/**
 * Callback invoked when the camera device channel is connected.
 */
static void guac_rdp_camera_channel_connected(rdpContext* context,
        ChannelConnectedEventArgs* args) {

    guac_client* client = ((rdp_freerdp_context*) context)->client;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;
    guac_rdp_camera* camera = rdp_client->camera;

    /* Ignore if not the camera channel */
    if (strcmp(args->name, "camera") != 0)
        return;

    if (camera == NULL)
        return;

    guac_client_log(client, GUAC_LOG_DEBUG,
        "Camera device channel connected");

    camera->active = 1;

}

/**
 * Callback invoked when the camera device channel is disconnected.
 */
static void guac_rdp_camera_channel_disconnected(rdpContext* context,
        ChannelDisconnectedEventArgs* args) {

    guac_client* client = ((rdp_freerdp_context*) context)->client;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;
    guac_rdp_camera* camera = rdp_client->camera;

    /* Ignore if not the camera channel */
    if (strcmp(args->name, "camera") != 0)
        return;

    if (camera == NULL)
        return;

    guac_client_log(client, GUAC_LOG_DEBUG,
        "Camera device channel disconnected");

    camera->active = 0;

}

void guac_rdp_camera_load_plugin(rdpContext* context) {

    guac_client* client = ((rdp_freerdp_context*) context)->client;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;

    /* Don't load if camera support is disabled or no device specified */
    if (rdp_client->settings->enable_camera == 0 ||
        rdp_client->settings->camera_device == NULL) {
        guac_client_log(client, GUAC_LOG_DEBUG,
            "Camera redirection disabled or no device specified");
        return;
    }

    /* Subscribe to channel connect/disconnect events */
    PubSub_SubscribeChannelConnected(context->pubSub,
        (pChannelConnectedEventHandler) guac_rdp_camera_channel_connected);

    PubSub_SubscribeChannelDisconnected(context->pubSub,
        (pChannelDisconnectedEventHandler) guac_rdp_camera_channel_disconnected);

    /* Build camera device argument in format "CameraName:DevicePath" */
    char camera_arg[256];
    snprintf(camera_arg, sizeof(camera_arg), "GuacamoleCamera:%s",
            rdp_client->settings->camera_device);

    /* Add the camera channel with the device path */
    guac_freerdp_dynamic_channel_collection_add(context->settings,
            "camera", camera_arg);

    guac_client_log(client, GUAC_LOG_INFO,
        "Camera redirection enabled for device: %s",
        rdp_client->settings->camera_device);

}
