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

#include "config.h"
#include "rdp.h"
#include "copilot-rdp.h"

#include <guacamole/client.h>
#include <guacamole/copilot.h>
#include <guacamole/string.h>

void guac_rdp_copilot_init(guac_client* client, guac_rdp_client* rdp_client) {

    if (rdp_client->settings->enable_copilot == 0)
        return;

    /* Allocate copilot */
    guac_copilot* copilot = guac_copilot_alloc(client);

    if (copilot == NULL) {
        guac_client_log(client, GUAC_LOG_WARNING,
                "Failed to allocate Copilot");
        return;
    }

    rdp_client->copilot = copilot;

    /* Set OpenAI API key if provided */
    if (rdp_client->settings->copilot_openai_key != NULL &&
            strlen(rdp_client->settings->copilot_openai_key) > 0) {
        copilot->ai_api_key = guac_strdup(rdp_client->settings->copilot_openai_key);
        guac_client_log(client, GUAC_LOG_INFO,
                "Copilot OpenAI integration enabled");
    } else {
        guac_client_log(client, GUAC_LOG_INFO,
                "Copilot running in local-only mode (no OpenAI key provided)");
    }

    /* Set RDP-specific context */
    guac_copilot_update_context(copilot, "rdp", NULL, "Windows");

    /* Initialize workflows and quick actions */
    extern void guac_copilot_init_workflows(guac_copilot* copilot);
    extern void guac_copilot_init_quick_actions(guac_copilot* copilot);

    guac_copilot_init_workflows(copilot);
    guac_copilot_init_quick_actions(copilot);

    guac_client_log(client, GUAC_LOG_INFO,
            "Copilot initialized for RDP session with %d workflows",
            copilot->workflow_count);

}

void guac_rdp_copilot_track_keystroke(guac_rdp_client* rdp_client,
        int keysym, int pressed) {

    if (rdp_client->copilot == NULL || !pressed)
        return;

    /* Track Ctrl+Alt+H for Copilot activation (handled by client) */
    /* We just track general activity here */

}

void guac_rdp_copilot_track_app(guac_rdp_client* rdp_client,
        const char* app_name) {

    if (rdp_client->copilot == NULL || app_name == NULL)
        return;

    guac_copilot_context* ctx = rdp_client->copilot->context;

    /* Track active applications for context */
    if (ctx->active_apps == NULL) {
        ctx->active_apps = guac_mem_alloc(10 * sizeof(char*));
        ctx->app_count = 0;
    }

    /* Check if app already tracked */
    for (int i = 0; i < ctx->app_count; i++) {
        if (strcmp(ctx->active_apps[i], app_name) == 0)
            return;
    }

    /* Add new app if space available */
    if (ctx->app_count < 10) {
        ctx->active_apps[ctx->app_count++] = guac_strdup(app_name);
    }

}
