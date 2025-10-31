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
#include "user.h"

#include <freerdp/freerdp.h>
#ifdef HAVE_FREERDP_CAMERA
#include <freerdp/client/camera.h>
#endif
#include <guacamole/client.h>
#include <guacamole/mem.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

guac_rdp_camera* guac_rdp_camera_alloc(guac_client* client) {

    guac_rdp_camera* camera = guac_mem_alloc(sizeof(guac_rdp_camera));
    camera->client = client;
    camera->virtual_device_path = NULL;
    camera->active = 0;
    camera->video_stream = NULL;
    camera->device_fd = -1;

    return camera;

}

void guac_rdp_camera_free(guac_rdp_camera* camera) {

    if (camera == NULL)
        return;

    /* Stop video stream and cleanup */
    guac_rdp_camera_stop_stream(camera);

    guac_mem_free(camera->virtual_device_path);
    guac_mem_free(camera);

}

#ifdef HAVE_FREERDP_CAMERA

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

#endif /* HAVE_FREERDP_CAMERA */

void guac_rdp_camera_load_plugin(rdpContext* context) {

#ifndef HAVE_FREERDP_CAMERA
    /* Camera support is not available in this FreeRDP version */
    guac_client* client = ((rdp_freerdp_context*) context)->client;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;
    
    if (rdp_client->settings->enable_camera) {
        guac_client_log(client, GUAC_LOG_WARNING,
            "Camera redirection requires FreeRDP 3.6.0 or later. "
            "Current FreeRDP version does not support camera redirection.");
    }
    return;
#else
    guac_client* client = ((rdp_freerdp_context*) context)->client;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;

    /* Don't load if camera support is disabled */
    if (rdp_client->settings->enable_camera == 0) {
        guac_client_log(client, GUAC_LOG_DEBUG,
            "Camera redirection disabled");
        return;
    }

    /* Subscribe to channel connect/disconnect events */
    PubSub_SubscribeChannelConnected(context->pubSub,
        (pChannelConnectedEventHandler) guac_rdp_camera_channel_connected);

    PubSub_SubscribeChannelDisconnected(context->pubSub,
        (pChannelDisconnectedEventHandler) guac_rdp_camera_channel_disconnected);

    /* Start the camera stream to create virtual device */
    if (guac_rdp_camera_start_stream(rdp_client->camera) != 0) {
        guac_client_log(client, GUAC_LOG_ERROR,
            "Failed to start camera stream");
        return;
    }

    /* Build camera device argument in format "CameraName:VirtualDevicePath" */
    char camera_arg[256];
    snprintf(camera_arg, sizeof(camera_arg), "GuacamoleCamera:%s",
            rdp_client->camera->virtual_device_path);

    /* Add the camera channel with the virtual device path */
    guac_freerdp_dynamic_channel_collection_add(context->settings,
            "camera", camera_arg);

    guac_client_log(client, GUAC_LOG_INFO,
        "Camera redirection enabled using virtual device: %s",
        rdp_client->camera->virtual_device_path);
#endif

}

int guac_rdp_camera_start_stream(guac_rdp_camera* camera) {

    if (camera == NULL)
        return -1;

    guac_client* client = camera->client;

    /* Create unique virtual device path */
    char template[] = "/tmp/guac_camera_XXXXXX";
    camera->device_fd = mkstemp(template);

    if (camera->device_fd == -1) {
        guac_client_log(client, GUAC_LOG_ERROR,
            "Failed to create virtual camera device");
        return -1;
    }

    /* Store the virtual device path */
    camera->virtual_device_path = guac_mem_alloc(strlen(template) + 1);
    strcpy(camera->virtual_device_path, template);

    guac_client_log(client, GUAC_LOG_DEBUG,
        "Created virtual camera device: %s", camera->virtual_device_path);

    return 0;
}

void guac_rdp_camera_stop_stream(guac_rdp_camera* camera) {

    if (camera == NULL)
        return;

    /* Close and remove virtual device */
    if (camera->device_fd != -1) {
        close(camera->device_fd);
        camera->device_fd = -1;
    }

    if (camera->virtual_device_path != NULL) {
        unlink(camera->virtual_device_path);
        guac_mem_free(camera->virtual_device_path);
        camera->virtual_device_path = NULL;
    }

    camera->video_stream = NULL;
    camera->active = 0;
}

int guac_rdp_camera_handle_video_data(guac_rdp_camera* camera,
        const void* data, int length) {

    if (camera == NULL || camera->device_fd == -1 || data == NULL || length <= 0)
        return -1;

    /* Write video data to virtual device pipe */
    ssize_t bytes_written = write(camera->device_fd, data, length);

    if (bytes_written != length) {
        guac_client_log(camera->client, GUAC_LOG_WARNING,
            "Failed to write video data to virtual device");
        return -1;
    }

    return 0;
}

int guac_rdp_camera_blob_handler(guac_user* user, guac_stream* stream,
        void* data, int length) {

    guac_rdp_client* rdp_client = (guac_rdp_client*) user->client->data;
    guac_rdp_camera* camera = rdp_client->camera;

    /* Ignore if camera is not available or not active */
    if (camera == NULL || !camera->active) {
        guac_client_log(user->client, GUAC_LOG_WARNING,
            "Received camera video data but camera is not active");
        return 0;
    }

    /* Forward video data to virtual camera device */
    if (guac_rdp_camera_handle_video_data(camera, data, length) != 0) {
        guac_client_log(user->client, GUAC_LOG_ERROR,
            "Failed to process camera video data");
        return -1;
    }

    guac_client_log(user->client, GUAC_LOG_TRACE,
        "Processed %d bytes of camera video data", length);

    return 0;
}

int guac_rdp_camera_end_handler(guac_user* user, guac_stream* stream) {

    guac_rdp_client* rdp_client = (guac_rdp_client*) user->client->data;
    guac_rdp_camera* camera = rdp_client->camera;

    guac_client_log(user->client, GUAC_LOG_DEBUG,
        "Camera video stream ended");

    /* Clear the video stream reference */
    if (camera != NULL) {
        camera->video_stream = NULL;
    }

    return 0;
}
