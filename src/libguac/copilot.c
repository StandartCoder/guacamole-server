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
#include "guacamole/client.h"
#include "guacamole/copilot.h"
#include "guacamole/mem.h"
#include "guacamole/protocol.h"
#include "guacamole/socket.h"
#include "guacamole/string.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define GUAC_COPILOT_HISTORY_SIZE 50

guac_copilot* guac_copilot_alloc(guac_client* client) {

    guac_copilot* copilot = guac_mem_zalloc(sizeof(guac_copilot));
    copilot->client = client;
    copilot->enabled = 1;

    /* Allocate context */
    copilot->context = guac_mem_zalloc(sizeof(guac_copilot_context));
    copilot->context->command_history = guac_mem_zalloc(
            GUAC_COPILOT_HISTORY_SIZE * sizeof(char*));
    copilot->context->command_count = 0;

    /* Allocate workflow array */
    copilot->workflows = guac_mem_zalloc(10 * sizeof(guac_copilot_workflow*));
    copilot->workflow_count = 0;

    /* Allocate quick actions array */
    copilot->quick_actions = guac_mem_zalloc(20 * sizeof(guac_copilot_quick_action*));
    copilot->quick_action_count = 0;

    copilot->recording = 0;
    copilot->recorded_workflow = NULL;

    guac_client_log(client, GUAC_LOG_INFO, "Guacamole Copilot initialized");

    return copilot;

}

void guac_copilot_free(guac_copilot* copilot) {

    if (copilot == NULL)
        return;

    /* Free context */
    if (copilot->context != NULL) {
        guac_mem_free(copilot->context->protocol);
        guac_mem_free(copilot->context->current_directory);
        guac_mem_free(copilot->context->os_type);
        guac_mem_free(copilot->context->remote_user);
        guac_mem_free(copilot->context->last_error);

        /* Free command history */
        for (int i = 0; i < copilot->context->command_count; i++)
            guac_mem_free(copilot->context->command_history[i]);
        guac_mem_free(copilot->context->command_history);

        /* Free active apps */
        for (int i = 0; i < copilot->context->app_count; i++)
            guac_mem_free(copilot->context->active_apps[i]);
        guac_mem_free(copilot->context->active_apps);

        guac_mem_free(copilot->context);
    }

    /* Free workflows */
    for (int i = 0; i < copilot->workflow_count; i++) {
        guac_copilot_workflow* wf = copilot->workflows[i];
        if (wf != NULL) {
            guac_mem_free(wf->description);
            guac_mem_free(wf->protocol);

            for (int j = 0; j < wf->step_count; j++) {
                guac_mem_free(wf->steps[j].description);
                guac_mem_free(wf->steps[j].command);
                guac_mem_free(wf->steps[j].expected_output);
            }
            guac_mem_free(wf->steps);

            for (int j = 0; j < wf->tag_count; j++)
                guac_mem_free(wf->tags[j]);
            guac_mem_free(wf->tags);

            guac_mem_free(wf);
        }
    }
    guac_mem_free(copilot->workflows);

    /* Free quick actions */
    for (int i = 0; i < copilot->quick_action_count; i++) {
        guac_copilot_quick_action* action = copilot->quick_actions[i];
        if (action != NULL) {
            guac_mem_free(action->name);
            guac_mem_free(action->label);
            guac_mem_free(action->icon);
            guac_mem_free(action->command);
            guac_mem_free(action->protocol);
            guac_mem_free(action);
        }
    }
    guac_mem_free(copilot->quick_actions);

    guac_mem_free(copilot->ai_endpoint);
    guac_mem_free(copilot->ai_api_key);

    guac_mem_free(copilot);

}

void guac_copilot_update_context(guac_copilot* copilot,
        const char* protocol, const char* current_dir, const char* os_type) {

    if (copilot == NULL || copilot->context == NULL)
        return;

    guac_copilot_context* ctx = copilot->context;

    if (protocol != NULL) {
        guac_mem_free(ctx->protocol);
        ctx->protocol = guac_strdup(protocol);
    }

    if (current_dir != NULL) {
        guac_mem_free(ctx->current_directory);
        ctx->current_directory = guac_strdup(current_dir);
    }

    if (os_type != NULL) {
        guac_mem_free(ctx->os_type);
        ctx->os_type = guac_strdup(os_type);
    }

}

void guac_copilot_add_command(guac_copilot* copilot, const char* command) {

    if (copilot == NULL || copilot->context == NULL || command == NULL)
        return;

    guac_copilot_context* ctx = copilot->context;

    /* If history is full, remove oldest */
    if (ctx->command_count >= GUAC_COPILOT_HISTORY_SIZE) {
        guac_mem_free(ctx->command_history[0]);
        memmove(ctx->command_history, ctx->command_history + 1,
                (GUAC_COPILOT_HISTORY_SIZE - 1) * sizeof(char*));
        ctx->command_count--;
    }

    /* Add new command */
    ctx->command_history[ctx->command_count++] = guac_strdup(command);

    /* If recording, add to recorded workflow */
    if (copilot->recording && copilot->recorded_workflow != NULL) {
        guac_copilot_workflow* wf = copilot->recorded_workflow;

        if (wf->step_count < GUAC_COPILOT_MAX_WORKFLOW_STEPS) {
            guac_copilot_workflow_step* step = &wf->steps[wf->step_count++];
            step->command = guac_strdup(command);
            step->description = guac_strdup(command);
            step->wait_time = 100;
            step->continue_on_error = 0;
        }
    }

}

int guac_copilot_handle_command(guac_copilot* copilot,
        guac_copilot_command_type command_type, const char* command_data) {

    if (copilot == NULL || !copilot->enabled)
        return -1;

    guac_client* client = copilot->client;

    switch (command_type) {

        case GUAC_COPILOT_CMD_SUGGEST:
            {
                char** suggestions = NULL;
                int count = guac_copilot_suggest_commands(copilot,
                        command_data, &suggestions, 5);

                /* Build JSON response */
                char response[4096];
                int pos = snprintf(response, sizeof(response),
                        "{\"type\":\"suggestions\",\"items\":[");

                for (int i = 0; i < count; i++) {
                    if (i > 0)
                        pos += snprintf(response + pos, sizeof(response) - pos, ",");
                    pos += snprintf(response + pos, sizeof(response) - pos,
                            "\"%s\"", suggestions[i]);
                    guac_mem_free(suggestions[i]);
                }
                snprintf(response + pos, sizeof(response) - pos, "]}");

                guac_copilot_send_message(copilot, "suggestions", response);
                guac_mem_free(suggestions);
            }
            break;

        case GUAC_COPILOT_CMD_EXECUTE_WORKFLOW:
            return guac_copilot_execute_workflow(copilot, command_data);

        case GUAC_COPILOT_CMD_CONTEXT_HELP:
            {
                char help[2048];
                snprintf(help, sizeof(help),
                        "{\"type\":\"help\",\"protocol\":\"%s\","
                        "\"os\":\"%s\",\"directory\":\"%s\"}",
                        copilot->context->protocol ? copilot->context->protocol : "unknown",
                        copilot->context->os_type ? copilot->context->os_type : "unknown",
                        copilot->context->current_directory ?
                                copilot->context->current_directory : "/");
                guac_copilot_send_message(copilot, "help", help);
            }
            break;

        case GUAC_COPILOT_CMD_LIST_WORKFLOWS:
            {
                char list[4096];
                int pos = snprintf(list, sizeof(list),
                        "{\"type\":\"workflows\",\"items\":[");

                for (int i = 0; i < copilot->workflow_count; i++) {
                    guac_copilot_workflow* wf = copilot->workflows[i];
                    if (i > 0)
                        pos += snprintf(list + pos, sizeof(list) - pos, ",");
                    pos += snprintf(list + pos, sizeof(list) - pos,
                            "{\"name\":\"%s\",\"description\":\"%s\","
                            "\"steps\":%d,\"protocol\":\"%s\"}",
                            wf->name,
                            wf->description ? wf->description : "",
                            wf->step_count,
                            wf->protocol ? wf->protocol : "all");
                }
                snprintf(list + pos, sizeof(list) - pos, "]}");

                guac_copilot_send_message(copilot, "workflows", list);
            }
            break;

        case GUAC_COPILOT_CMD_RECORD_WORKFLOW:
            if (copilot->recording) {
                guac_copilot_stop_recording(copilot);
            } else {
                guac_copilot_start_recording(copilot, command_data);
            }
            break;

        case GUAC_COPILOT_CMD_SESSION_INSIGHTS:
            {
                char insights[2048];
                snprintf(insights, sizeof(insights),
                        "{\"type\":\"insights\","
                        "\"session_duration\":%ld,"
                        "\"commands_executed\":%d,"
                        "\"protocol\":\"%s\","
                        "\"privileged\":%d}",
                        copilot->context->session_duration,
                        copilot->context->command_count,
                        copilot->context->protocol ? copilot->context->protocol : "unknown",
                        copilot->context->is_privileged);
                guac_copilot_send_message(copilot, "insights", insights);
            }
            break;

        default:
            guac_client_log(client, GUAC_LOG_WARNING,
                    "Unknown copilot command type: %d", command_type);
            return -1;
    }

    return 0;

}

int guac_copilot_register_workflow(guac_copilot* copilot,
        guac_copilot_workflow* workflow) {

    if (copilot == NULL || workflow == NULL)
        return -1;

    if (copilot->workflow_count >= 10) {
        guac_client_log(copilot->client, GUAC_LOG_WARNING,
                "Maximum number of workflows reached");
        return -1;
    }

    copilot->workflows[copilot->workflow_count++] = workflow;

    guac_client_log(copilot->client, GUAC_LOG_INFO,
            "Registered workflow: %s (%d steps)", workflow->name, workflow->step_count);

    return 0;

}

int guac_copilot_execute_workflow(guac_copilot* copilot,
        const char* workflow_name) {

    if (copilot == NULL || workflow_name == NULL)
        return -1;

    guac_client* client = copilot->client;

    /* Find workflow */
    guac_copilot_workflow* workflow = NULL;
    for (int i = 0; i < copilot->workflow_count; i++) {
        if (strcmp(copilot->workflows[i]->name, workflow_name) == 0) {
            workflow = copilot->workflows[i];
            break;
        }
    }

    if (workflow == NULL) {
        guac_client_log(client, GUAC_LOG_WARNING,
                "Workflow not found: %s", workflow_name);
        return -1;
    }

    guac_client_log(client, GUAC_LOG_INFO,
            "Executing workflow: %s (%d steps)", workflow->name, workflow->step_count);

    /* Send workflow start notification */
    char start_msg[512];
    snprintf(start_msg, sizeof(start_msg),
            "{\"type\":\"workflow_start\",\"name\":\"%s\",\"steps\":%d}",
            workflow->name, workflow->step_count);
    guac_copilot_send_message(copilot, "workflow", start_msg);

    /* Execute each step */
    for (int i = 0; i < workflow->step_count; i++) {
        guac_copilot_workflow_step* step = &workflow->steps[i];

        /* Send step start notification */
        char step_msg[1024];
        snprintf(step_msg, sizeof(step_msg),
                "{\"type\":\"workflow_step\",\"step\":%d,\"description\":\"%s\","
                "\"command\":\"%s\"}",
                i + 1, step->description, step->command);
        guac_copilot_send_message(copilot, "workflow", step_msg);

        /* The actual command execution will be handled by the client
         * We just send the commands to execute */
    }

    /* Send workflow complete notification */
    char complete_msg[256];
    snprintf(complete_msg, sizeof(complete_msg),
            "{\"type\":\"workflow_complete\",\"name\":\"%s\"}",
            workflow->name);
    guac_copilot_send_message(copilot, "workflow", complete_msg);

    return 0;

}

int guac_copilot_suggest_commands(guac_copilot* copilot, const char* input,
        char*** suggestions, int max_suggestions) {

    if (copilot == NULL || suggestions == NULL)
        return 0;

    *suggestions = guac_mem_alloc(max_suggestions * sizeof(char*));
    int count = 0;

    guac_copilot_context* ctx = copilot->context;

    /* Provide context-based suggestions */
    if (ctx->protocol != NULL && strcmp(ctx->protocol, "ssh") == 0) {

        if (input == NULL || strlen(input) == 0) {
            /* Common SSH commands */
            if (count < max_suggestions)
                (*suggestions)[count++] = guac_strdup("ls -la");
            if (count < max_suggestions)
                (*suggestions)[count++] = guac_strdup("pwd");
            if (count < max_suggestions)
                (*suggestions)[count++] = guac_strdup("cd ~");
        } else if (strncmp(input, "l", 1) == 0) {
            if (count < max_suggestions)
                (*suggestions)[count++] = guac_strdup("ls -la");
            if (count < max_suggestions)
                (*suggestions)[count++] = guac_strdup("ll");
        } else if (strncmp(input, "cd", 2) == 0) {
            if (count < max_suggestions)
                (*suggestions)[count++] = guac_strdup("cd ~");
            if (count < max_suggestions)
                (*suggestions)[count++] = guac_strdup("cd ..");
        }

    } else if (ctx->protocol != NULL && strcmp(ctx->protocol, "rdp") == 0) {

        /* RDP-specific suggestions */
        if (count < max_suggestions)
            (*suggestions)[count++] = guac_strdup("Open Task Manager");
        if (count < max_suggestions)
            (*suggestions)[count++] = guac_strdup("Open Command Prompt");
        if (count < max_suggestions)
            (*suggestions)[count++] = guac_strdup("Open PowerShell");
    }

    /* Add suggestions from command history */
    if (count < max_suggestions && ctx->command_count > 0) {
        /* Get last unique command */
        const char* last_cmd = ctx->command_history[ctx->command_count - 1];
        if (last_cmd != NULL)
            (*suggestions)[count++] = guac_strdup(last_cmd);
    }

    return count;

}

int guac_copilot_start_recording(guac_copilot* copilot,
        const char* workflow_name) {

    if (copilot == NULL || workflow_name == NULL)
        return -1;

    if (copilot->recording) {
        guac_client_log(copilot->client, GUAC_LOG_WARNING,
                "Already recording a workflow");
        return -1;
    }

    /* Allocate new workflow */
    guac_copilot_workflow* wf = guac_mem_zalloc(sizeof(guac_copilot_workflow));
    guac_strlcpy(wf->name, workflow_name, GUAC_COPILOT_MAX_WORKFLOW_NAME);
    wf->steps = guac_mem_zalloc(GUAC_COPILOT_MAX_WORKFLOW_STEPS *
            sizeof(guac_copilot_workflow_step));
    wf->step_count = 0;

    copilot->recorded_workflow = wf;
    copilot->recording = 1;

    guac_client_log(copilot->client, GUAC_LOG_INFO,
            "Started recording workflow: %s", workflow_name);

    /* Notify client */
    char msg[256];
    snprintf(msg, sizeof(msg),
            "{\"type\":\"recording_started\",\"name\":\"%s\"}",
            workflow_name);
    guac_copilot_send_message(copilot, "recording", msg);

    return 0;

}

int guac_copilot_stop_recording(guac_copilot* copilot) {

    if (copilot == NULL || !copilot->recording)
        return -1;

    copilot->recording = 0;

    /* Register the recorded workflow */
    if (copilot->recorded_workflow != NULL) {
        guac_copilot_register_workflow(copilot, copilot->recorded_workflow);

        char msg[256];
        snprintf(msg, sizeof(msg),
                "{\"type\":\"recording_stopped\",\"name\":\"%s\",\"steps\":%d}",
                copilot->recorded_workflow->name,
                copilot->recorded_workflow->step_count);
        guac_copilot_send_message(copilot, "recording", msg);

        copilot->recorded_workflow = NULL;
    }

    guac_client_log(copilot->client, GUAC_LOG_INFO, "Stopped recording workflow");

    return 0;

}

int guac_copilot_register_quick_action(guac_copilot* copilot,
        guac_copilot_quick_action* action) {

    if (copilot == NULL || action == NULL)
        return -1;

    if (copilot->quick_action_count >= 20) {
        guac_client_log(copilot->client, GUAC_LOG_WARNING,
                "Maximum number of quick actions reached");
        return -1;
    }

    copilot->quick_actions[copilot->quick_action_count++] = action;

    guac_client_log(copilot->client, GUAC_LOG_DEBUG,
            "Registered quick action: %s", action->name);

    return 0;

}

void guac_copilot_send_message(guac_copilot* copilot,
        const char* message_type, const char* message) {

    if (copilot == NULL || copilot->client == NULL)
        return;

    guac_client* client = copilot->client;

    /* Send as a special "copilot" instruction to the client */
    guac_protocol_send_argv(client->socket, client->socket->last_write_timestamp,
            "text/plain", "copilot", message);

    guac_socket_flush(client->socket);

}
