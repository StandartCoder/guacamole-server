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
#include "ssh.h"
#include "copilot-ssh.h"

#include <guacamole/client.h>
#include <guacamole/copilot.h>
#include <guacamole/mem.h>
#include <guacamole/string.h>

#include <stdlib.h>
#include <string.h>

void guac_ssh_copilot_init(guac_client* client, guac_ssh_client* ssh_client) {

    if (ssh_client->settings->enable_copilot == 0)
        return;

    /* Allocate copilot */
    guac_copilot* copilot = guac_copilot_alloc(client);

    if (copilot == NULL) {
        guac_client_log(client, GUAC_LOG_WARNING,
                "Failed to allocate Copilot");
        return;
    }

    ssh_client->copilot = copilot;

    /* Set OpenAI API key if provided */
    if (ssh_client->settings->copilot_openai_key != NULL &&
            strlen(ssh_client->settings->copilot_openai_key) > 0) {
        copilot->ai_api_key = guac_strdup(ssh_client->settings->copilot_openai_key);
        guac_client_log(client, GUAC_LOG_INFO,
                "Copilot OpenAI integration enabled");
    } else {
        guac_client_log(client, GUAC_LOG_INFO,
                "Copilot running in local-only mode (no OpenAI key provided)");
    }

    /* Set SSH-specific context */
    guac_copilot_update_context(copilot, "ssh", "~", "Linux");

    /* Initialize workflows and quick actions */
    extern void guac_copilot_init_workflows(guac_copilot* copilot);
    extern void guac_copilot_init_quick_actions(guac_copilot* copilot);

    guac_copilot_init_workflows(copilot);
    guac_copilot_init_quick_actions(copilot);

    guac_client_log(client, GUAC_LOG_INFO,
            "Copilot initialized for SSH session with %d workflows and %d quick actions",
            copilot->workflow_count, copilot->quick_action_count);

}

void guac_ssh_copilot_track_command(guac_ssh_client* ssh_client,
        const char* command) {

    if (ssh_client->copilot == NULL || command == NULL || strlen(command) == 0)
        return;

    /* Skip if command is just whitespace */
    const char* c = command;
    while (*c && (*c == ' ' || *c == '\t' || *c == '\n' || *c == '\r'))
        c++;
    if (*c == '\0')
        return;

    /* Add to copilot history */
    guac_copilot_add_command(ssh_client->copilot, command);

    /* Update context based on command */
    if (strncmp(command, "cd ", 3) == 0) {
        /* Track directory changes */
        const char* dir = command + 3;
        while (*dir == ' ') dir++;

        if (strlen(dir) > 0) {
            /* Simple tracking - real implementation would resolve paths */
            guac_copilot_update_context(ssh_client->copilot, NULL, dir, NULL);
        }

    } else if (strncmp(command, "sudo ", 5) == 0 || strcmp(command, "su") == 0) {
        /* Track privilege escalation */
        ssh_client->copilot->context->is_privileged = 1;

    } else if (strcmp(command, "exit") == 0 && ssh_client->copilot->context->is_privileged) {
        /* Exiting privileged session */
        ssh_client->copilot->context->is_privileged = 0;
    }

}

void guac_ssh_copilot_track_output(guac_ssh_client* ssh_client,
        const char* output, int length) {

    if (ssh_client->copilot == NULL || output == NULL)
        return;

    /* Look for error patterns in output */
    if (strstr(output, "error") != NULL ||
        strstr(output, "Error") != NULL ||
        strstr(output, "ERROR") != NULL ||
        strstr(output, "failed") != NULL ||
        strstr(output, "Failed") != NULL) {

        /* Store last error for context */
        char* error_snippet = guac_mem_alloc(256);
        int copy_len = (length < 255) ? length : 255;
        memcpy(error_snippet, output, copy_len);
        error_snippet[copy_len] = '\0';

        guac_mem_free(ssh_client->copilot->context->last_error);
        ssh_client->copilot->context->last_error = error_snippet;

        guac_client_log(ssh_client->client, GUAC_LOG_DEBUG,
                "Copilot detected error in output");
    }

    /* Track current directory from PS1 prompts if visible */
    /* This would parse prompt output to extract cwd */

}

void guac_ssh_copilot_detect_os(guac_ssh_client* ssh_client,
        const char* banner) {

    if (ssh_client->copilot == NULL || banner == NULL)
        return;

    const char* os_type = "Linux"; /* Default */

    if (strstr(banner, "Ubuntu") != NULL) {
        os_type = "Ubuntu";
    } else if (strstr(banner, "Debian") != NULL) {
        os_type = "Debian";
    } else if (strstr(banner, "CentOS") != NULL) {
        os_type = "CentOS";
    } else if (strstr(banner, "Red Hat") != NULL || strstr(banner, "RHEL") != NULL) {
        os_type = "RHEL";
    } else if (strstr(banner, "FreeBSD") != NULL) {
        os_type = "FreeBSD";
    } else if (strstr(banner, "Darwin") != NULL || strstr(banner, "macOS") != NULL) {
        os_type = "macOS";
    }

    guac_copilot_update_context(ssh_client->copilot, NULL, NULL, os_type);

    guac_client_log(ssh_client->client, GUAC_LOG_DEBUG,
            "Copilot detected OS: %s", os_type);

}
