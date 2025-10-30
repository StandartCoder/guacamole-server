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

#ifndef _GUAC_COPILOT_H
#define _GUAC_COPILOT_H

/**
 * Guacamole Copilot - AI-powered assistant for remote sessions.
 * Provides intelligent automation, workflow execution, command suggestions,
 * and contextual help during RDP, SSH, VNC, and other protocol sessions.
 *
 * @file copilot.h
 */

#include "client.h"
#include "user.h"

/**
 * The maximum length of a copilot command, in characters.
 */
#define GUAC_COPILOT_MAX_COMMAND_LENGTH 1024

/**
 * The maximum length of a workflow name.
 */
#define GUAC_COPILOT_MAX_WORKFLOW_NAME 128

/**
 * The maximum number of steps in a workflow.
 */
#define GUAC_COPILOT_MAX_WORKFLOW_STEPS 100

/**
 * Copilot command types that can be executed.
 */
typedef enum guac_copilot_command_type {

    /** Suggest commands based on context */
    GUAC_COPILOT_CMD_SUGGEST,

    /** Execute a workflow/playbook */
    GUAC_COPILOT_CMD_EXECUTE_WORKFLOW,

    /** Get help for current context */
    GUAC_COPILOT_CMD_CONTEXT_HELP,

    /** Generate a script */
    GUAC_COPILOT_CMD_GENERATE_SCRIPT,

    /** Quick action (preset commands) */
    GUAC_COPILOT_CMD_QUICK_ACTION,

    /** Troubleshoot connection or session */
    GUAC_COPILOT_CMD_TROUBLESHOOT,

    /** Record current actions as workflow */
    GUAC_COPILOT_CMD_RECORD_WORKFLOW,

    /** List available workflows */
    GUAC_COPILOT_CMD_LIST_WORKFLOWS,

    /** Get session insights */
    GUAC_COPILOT_CMD_SESSION_INSIGHTS

} guac_copilot_command_type;

/**
 * Session context information for intelligent assistance.
 */
typedef struct guac_copilot_context {

    /** The protocol being used (rdp, ssh, vnc, etc.) */
    char* protocol;

    /** Current working directory (for shell sessions) */
    char* current_directory;

    /** Operating system type */
    char* os_type;

    /** Last executed commands (circular buffer) */
    char** command_history;

    /** Number of commands in history */
    int command_count;

    /** Current user on remote system */
    char* remote_user;

    /** Whether user has elevated privileges */
    int is_privileged;

    /** Active applications or windows */
    char** active_apps;

    /** Number of active apps */
    int app_count;

    /** Session duration in seconds */
    long session_duration;

    /** Last error message encountered */
    char* last_error;

} guac_copilot_context;

/**
 * A single step in a workflow.
 */
typedef struct guac_copilot_workflow_step {

    /** Step description */
    char* description;

    /** Command to execute */
    char* command;

    /** Expected output (for validation) */
    char* expected_output;

    /** Wait time after execution (milliseconds) */
    int wait_time;

    /** Whether to continue on error */
    int continue_on_error;

} guac_copilot_workflow_step;

/**
 * A workflow/playbook that can be executed.
 */
typedef struct guac_copilot_workflow {

    /** Workflow name */
    char name[GUAC_COPILOT_MAX_WORKFLOW_NAME];

    /** Workflow description */
    char* description;

    /** Protocol this workflow is for (NULL = all) */
    char* protocol;

    /** Array of steps */
    guac_copilot_workflow_step* steps;

    /** Number of steps */
    int step_count;

    /** Whether workflow requires privileges */
    int requires_privileges;

    /** Tags for categorization */
    char** tags;

    /** Number of tags */
    int tag_count;

} guac_copilot_workflow;

/**
 * Quick action preset.
 */
typedef struct guac_copilot_quick_action {

    /** Action name */
    char* name;

    /** Display label */
    char* label;

    /** Icon identifier */
    char* icon;

    /** Command to execute */
    char* command;

    /** Protocol (NULL = all) */
    char* protocol;

} guac_copilot_quick_action;

/**
 * The Guacamole Copilot instance.
 */
typedef struct guac_copilot {

    /** The client this copilot is attached to */
    guac_client* client;

    /** Whether copilot is enabled */
    int enabled;

    /** Current session context */
    guac_copilot_context* context;

    /** Available workflows */
    guac_copilot_workflow** workflows;

    /** Number of workflows */
    int workflow_count;

    /** Quick actions */
    guac_copilot_quick_action** quick_actions;

    /** Number of quick actions */
    int quick_action_count;

    /** Whether recording is active */
    int recording;

    /** Recorded workflow being built */
    guac_copilot_workflow* recorded_workflow;

    /** AI endpoint URL (if using external AI service) */
    char* ai_endpoint;

    /** API key for AI service */
    char* ai_api_key;

} guac_copilot;

/**
 * Allocates and initializes a new Copilot instance.
 *
 * @param client
 *     The client to attach the copilot to.
 *
 * @return
 *     A new copilot instance, or NULL if allocation fails.
 */
guac_copilot* guac_copilot_alloc(guac_client* client);

/**
 * Frees the given copilot instance.
 *
 * @param copilot
 *     The copilot to free.
 */
void guac_copilot_free(guac_copilot* copilot);

/**
 * Updates the session context with new information.
 *
 * @param copilot
 *     The copilot instance.
 *
 * @param protocol
 *     The protocol being used (can be NULL to keep existing).
 *
 * @param current_dir
 *     The current directory (can be NULL).
 *
 * @param os_type
 *     The operating system type (can be NULL).
 */
void guac_copilot_update_context(guac_copilot* copilot,
        const char* protocol, const char* current_dir, const char* os_type);

/**
 * Adds a command to the history for context tracking.
 *
 * @param copilot
 *     The copilot instance.
 *
 * @param command
 *     The command that was executed.
 */
void guac_copilot_add_command(guac_copilot* copilot, const char* command);

/**
 * Handles a copilot command from the client.
 *
 * @param copilot
 *     The copilot instance.
 *
 * @param command_type
 *     The type of command.
 *
 * @param command_data
 *     Additional data for the command.
 *
 * @return
 *     Zero on success, non-zero on error.
 */
int guac_copilot_handle_command(guac_copilot* copilot,
        guac_copilot_command_type command_type, const char* command_data);

/**
 * Registers a workflow with the copilot.
 *
 * @param copilot
 *     The copilot instance.
 *
 * @param workflow
 *     The workflow to register. The copilot takes ownership.
 *
 * @return
 *     Zero on success, non-zero on error.
 */
int guac_copilot_register_workflow(guac_copilot* copilot,
        guac_copilot_workflow* workflow);

/**
 * Executes a workflow by name.
 *
 * @param copilot
 *     The copilot instance.
 *
 * @param workflow_name
 *     The name of the workflow to execute.
 *
 * @return
 *     Zero on success, non-zero on error.
 */
int guac_copilot_execute_workflow(guac_copilot* copilot,
        const char* workflow_name);

/**
 * Generates command suggestions based on current context.
 *
 * @param copilot
 *     The copilot instance.
 *
 * @param input
 *     Partial input from the user (can be NULL).
 *
 * @param suggestions
 *     Array to fill with suggestions (caller must free).
 *
 * @param max_suggestions
 *     Maximum number of suggestions to return.
 *
 * @return
 *     The number of suggestions returned.
 */
int guac_copilot_suggest_commands(guac_copilot* copilot, const char* input,
        char*** suggestions, int max_suggestions);

/**
 * Starts recording actions for workflow creation.
 *
 * @param copilot
 *     The copilot instance.
 *
 * @param workflow_name
 *     The name for the new workflow.
 *
 * @return
 *     Zero on success, non-zero on error.
 */
int guac_copilot_start_recording(guac_copilot* copilot,
        const char* workflow_name);

/**
 * Stops recording and saves the workflow.
 *
 * @param copilot
 *     The copilot instance.
 *
 * @return
 *     Zero on success, non-zero on error.
 */
int guac_copilot_stop_recording(guac_copilot* copilot);

/**
 * Registers a quick action.
 *
 * @param copilot
 *     The copilot instance.
 *
 * @param action
 *     The quick action to register.
 *
 * @return
 *     Zero on success, non-zero on error.
 */
int guac_copilot_register_quick_action(guac_copilot* copilot,
        guac_copilot_quick_action* action);

/**
 * Sends a copilot message to the client.
 *
 * @param copilot
 *     The copilot instance.
 *
 * @param message_type
 *     The type of message.
 *
 * @param message
 *     The message content (JSON format recommended).
 */
void guac_copilot_send_message(guac_copilot* copilot,
        const char* message_type, const char* message);

/**
 * Queries OpenAI API for AI-powered assistance.
 *
 * @param copilot
 *     The copilot instance.
 *
 * @param api_key
 *     The OpenAI API key.
 *
 * @param prompt
 *     The prompt/question to send to OpenAI.
 *
 * @param response_buffer
 *     Buffer to store the response.
 *
 * @param buffer_size
 *     Size of the response buffer.
 *
 * @return
 *     Zero on success, non-zero on error.
 */
int guac_copilot_query_openai(guac_copilot* copilot, const char* api_key,
        const char* prompt, char* response_buffer, int buffer_size);

#endif
