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

#ifndef GUAC_RDP_COPILOT_H
#define GUAC_RDP_COPILOT_H

#include "rdp.h"

#include <guacamole/client.h>

/**
 * Initializes Copilot for an RDP session.
 *
 * @param client
 *     The guac_client.
 *
 * @param rdp_client
 *     The RDP client data.
 */
void guac_rdp_copilot_init(guac_client* client, guac_rdp_client* rdp_client);

/**
 * Tracks a keystroke for Copilot context.
 *
 * @param rdp_client
 *     The RDP client data.
 *
 * @param keysym
 *     The key symbol.
 *
 * @param pressed
 *     Whether the key was pressed (1) or released (0).
 */
void guac_rdp_copilot_track_keystroke(guac_rdp_client* rdp_client,
        int keysym, int pressed);

/**
 * Tracks an active application for context awareness.
 *
 * @param rdp_client
 *     The RDP client data.
 *
 * @param app_name
 *     The application name.
 */
void guac_rdp_copilot_track_app(guac_rdp_client* rdp_client,
        const char* app_name);

#endif
