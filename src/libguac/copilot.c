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

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

#define GUAC_COPILOT_HISTORY_SIZE 50
#define GUAC_OPENAI_API_ENDPOINT "https://api.openai.com/v1/chat/completions"
#define GUAC_OPENAI_MODEL "gpt-4"

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

    /* If OpenAI API key is available, use AI for suggestions */
    if (copilot->ai_api_key != NULL && strlen(copilot->ai_api_key) > 0) {
        
        char prompt[2048];
        snprintf(prompt, sizeof(prompt),
                "User is in a %s session on %s. Current directory: %s. "
                "Recent commands: ",
                ctx->protocol ? ctx->protocol : "remote",
                ctx->os_type ? ctx->os_type : "unknown OS",
                ctx->current_directory ? ctx->current_directory : "/");
        
        /* Add recent command history to prompt */
        int history_len = strlen(prompt);
        int cmd_start = (ctx->command_count > 3) ? ctx->command_count - 3 : 0;
        for (int i = cmd_start; i < ctx->command_count && history_len < 1800; i++) {
            int added = snprintf(prompt + history_len, sizeof(prompt) - history_len,
                    "'%s', ", ctx->command_history[i]);
            if (added > 0)
                history_len += added;
        }
        
        /* Add the actual request */
        snprintf(prompt + history_len, sizeof(prompt) - history_len,
                ". User typed: '%s'. Suggest %d relevant commands (one per line, no explanations).",
                input ? input : "", max_suggestions);
        
        char ai_response[4096];
        if (guac_copilot_query_openai(copilot, copilot->ai_api_key, prompt,
                ai_response, sizeof(ai_response)) == 0) {
            
            /* Parse response into suggestions (split by newlines) */
            char* line = strtok(ai_response, "\n");
            while (line != NULL && count < max_suggestions) {
                /* Skip empty lines and trim whitespace */
                while (*line == ' ' || *line == '\t') line++;
                if (strlen(line) > 0) {
                    (*suggestions)[count++] = guac_strdup(line);
                }
                line = strtok(NULL, "\n");
            }
            
            guac_client_log(copilot->client, GUAC_LOG_DEBUG,
                    "OpenAI provided %d suggestions", count);
            
            if (count > 0)
                return count; /* Return AI suggestions */
        }
        
        guac_client_log(copilot->client, GUAC_LOG_DEBUG,
                "Falling back to local suggestions");
    }

    /* Fallback: Provide context-based suggestions locally */
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

    /* Send as a custom instruction to the client */
    guac_socket_write_string(client->socket, "4.argv,");
    guac_socket_write_string(client->socket, "10.text/plain,");
    guac_socket_write_string(client->socket, "7.copilot,");
    
     /* length_str must be large enough to hold the decimal representation
         of strlen(message) (up to 20 digits on 64-bit) plus the trailing
         dot and NUL terminator. 32 bytes is plenty. */
     char length_str[32];
     snprintf(length_str, sizeof(length_str), "%zu.", strlen(message));
    guac_socket_write_string(client->socket, length_str);
    guac_socket_write_string(client->socket, message);
    guac_socket_write_string(client->socket, ";");

    guac_socket_flush(client->socket);

}

#ifdef HAVE_LIBCURL

/**
 * Callback for libcurl to write response data.
 */
static size_t guac_copilot_curl_write_callback(void* contents, size_t size,
        size_t nmemb, void* userp) {

    size_t realsize = size * nmemb;
    char** response = (char**)userp;

    /* Reallocate buffer to fit new data */
    size_t current_len = *response ? strlen(*response) : 0;
    char* new_response = realloc(*response, current_len + realsize + 1);

    if (new_response == NULL) {
        /* Out of memory */
        return 0;
    }

    *response = new_response;
    memcpy(*response + current_len, contents, realsize);
    (*response)[current_len + realsize] = '\0';

    return realsize;
}

/**
 * Escapes a JSON string by replacing special characters.
 */
static char* guac_copilot_escape_json_string(const char* str) {
    if (str == NULL)
        return guac_strdup("");

    size_t len = strlen(str);
    char* escaped = guac_mem_alloc(len * 2 + 1); /* Worst case: all chars escaped */
    char* dst = escaped;

    for (const char* src = str; *src; src++) {
        switch (*src) {
            case '"':
                *dst++ = '\\';
                *dst++ = '"';
                break;
            case '\\':
                *dst++ = '\\';
                *dst++ = '\\';
                break;
            case '\n':
                *dst++ = '\\';
                *dst++ = 'n';
                break;
            case '\r':
                *dst++ = '\\';
                *dst++ = 'r';
                break;
            case '\t':
                *dst++ = '\\';
                *dst++ = 't';
                break;
            default:
                *dst++ = *src;
        }
    }
    *dst = '\0';

    return escaped;
}

int guac_copilot_query_openai(guac_copilot* copilot, const char* api_key,
        const char* prompt, char* response_buffer, int buffer_size) {

    if (copilot == NULL || api_key == NULL || prompt == NULL || 
            response_buffer == NULL || buffer_size <= 0) {
        return -1;
    }

    guac_client* client = copilot->client;
    CURL* curl;
    CURLcode res;
    char* response_data = NULL;
    int result = -1;

    guac_client_log(client, GUAC_LOG_DEBUG,
            "Querying OpenAI API for copilot assistance");

    /* Initialize curl */
    curl = curl_easy_init();
    if (!curl) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Failed to initialize curl for OpenAI API");
        return -1;
    }

    /* Escape the prompt for JSON */
    char* escaped_prompt = guac_copilot_escape_json_string(prompt);

    /* Build context information */
    char context_info[1024] = "";
    if (copilot->context) {
        snprintf(context_info, sizeof(context_info),
                "Context: Protocol=%s, OS=%s, Directory=%s, CommandHistory=%d commands",
                copilot->context->protocol ? copilot->context->protocol : "unknown",
                copilot->context->os_type ? copilot->context->os_type : "unknown",
                copilot->context->current_directory ? copilot->context->current_directory : "/",
                copilot->context->command_count);
    }
    char* escaped_context = guac_copilot_escape_json_string(context_info);

    /* Build JSON payload */
    char json_payload[8192];
    snprintf(json_payload, sizeof(json_payload),
            "{"
            "\"model\":\"%s\","
            "\"messages\":["
            "{\"role\":\"system\",\"content\":\"You are a helpful AI assistant for remote desktop and SSH sessions. Provide concise, actionable advice. %s\"},"
            "{\"role\":\"user\",\"content\":\"%s\"}"
            "],"
            "\"max_tokens\":500,"
            "\"temperature\":0.7"
            "}",
            GUAC_OPENAI_MODEL,
            escaped_context,
            escaped_prompt);

    guac_mem_free(escaped_prompt);
    guac_mem_free(escaped_context);

    /* Set up headers */
    struct curl_slist* headers = NULL;
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);
    
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);

    /* Configure curl */
    curl_easy_setopt(curl, CURLOPT_URL, GUAC_OPENAI_API_ENDPOINT);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, guac_copilot_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    /* Perform request */
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "OpenAI API request failed: %s", curl_easy_strerror(res));
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code == 200 && response_data) {
            guac_client_log(client, GUAC_LOG_DEBUG,
                    "OpenAI API response received successfully");

            /* Parse JSON response to extract the message content */
            /* Look for "content": in the response */
            char* content_start = strstr(response_data, "\"content\":");
            if (content_start) {
                content_start = strchr(content_start, '"');
                if (content_start) {
                    content_start++; /* Skip opening quote */
                    content_start = strchr(content_start, '"');
                    if (content_start) {
                        content_start++; /* Skip second quote */
                        char* content_end = strstr(content_start, "\",");
                        if (!content_end)
                            content_end = strstr(content_start, "\"");
                        
                        if (content_end) {
                            int len = content_end - content_start;
                            if (len > buffer_size - 1)
                                len = buffer_size - 1;
                            
                            strncpy(response_buffer, content_start, len);
                            response_buffer[len] = '\0';
                            
                            /* Unescape basic JSON escape sequences */
                            char* src = response_buffer;
                            char* dst = response_buffer;
                            while (*src) {
                                if (*src == '\\' && *(src + 1)) {
                                    src++;
                                    switch (*src) {
                                        case 'n': *dst++ = '\n'; break;
                                        case 'r': *dst++ = '\r'; break;
                                        case 't': *dst++ = '\t'; break;
                                        case '"': *dst++ = '"'; break;
                                        case '\\': *dst++ = '\\'; break;
                                        default: *dst++ = *src; break;
                                    }
                                    src++;
                                } else {
                                    *dst++ = *src++;
                                }
                            }
                            *dst = '\0';
                            
                            result = 0; /* Success */
                        }
                    }
                }
            }

            if (result != 0) {
                guac_client_log(client, GUAC_LOG_ERROR,
                        "Failed to parse OpenAI API response");
                snprintf(response_buffer, buffer_size,
                        "Error: Could not parse AI response");
            }
        } else {
            guac_client_log(client, GUAC_LOG_ERROR,
                    "OpenAI API returned error. HTTP code: %ld", http_code);
            snprintf(response_buffer, buffer_size,
                    "Error: OpenAI API returned HTTP %ld", http_code);
        }
    }

    /* Cleanup */
    if (response_data)
        free(response_data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return result;
}

#else

/* Fallback implementation when libcurl is not available */
int guac_copilot_query_openai(guac_copilot* copilot, const char* api_key,
        const char* prompt, char* response_buffer, int buffer_size) {

    if (copilot == NULL || response_buffer == NULL || buffer_size <= 0)
        return -1;

    guac_client_log(copilot->client, GUAC_LOG_WARNING,
            "OpenAI integration is not available. libcurl was not found during build.");

    snprintf(response_buffer, buffer_size,
            "Error: OpenAI integration requires libcurl");

    return -1;
}

#endif
