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

#ifndef GUAC_SSH_COPILOT_H
#define GUAC_SSH_COPILOT_H

#include "ssh.h"

#include <guacamole/client.h>

/**
 * Initializes Copilot for an SSH session.
 *
 * @param client
 *     The guac_client.
 *
 * @param ssh_client
 *     The SSH client data.
 */
void guac_ssh_copilot_init(guac_client* client, guac_ssh_client* ssh_client);

/**
 * Tracks a command executed in the SSH session.
 *
 * @param ssh_client
 *     The SSH client data.
 *
 * @param command
 *     The command that was executed.
 */
void guac_ssh_copilot_track_command(guac_ssh_client* ssh_client,
        const char* command);

/**
 * Tracks output from the SSH session to detect errors and context.
 *
 * @param ssh_client
 *     The SSH client data.
 *
 * @param output
 *     The output received.
 *
 * @param length
 *     The length of the output.
 */
void guac_ssh_copilot_track_output(guac_ssh_client* ssh_client,
        const char* output, int length);

/**
 * Detects the operating system from the SSH banner.
 *
 * @param ssh_client
 *     The SSH client data.
 *
 * @param banner
 *     The SSH banner string.
 */
void guac_ssh_copilot_detect_os(guac_ssh_client* ssh_client,
        const char* banner);

#endif
