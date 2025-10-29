# CLIENT-SIDE IMPLEMENTATION PROMPT FOR AI AGENT

> **Purpose**: This document provides COMPLETE instructions for an AI agent to implement the client-side code for three major features added to the Guacamole server.

---

## 🎯 MISSION

You are tasked with implementing the **client-side code** for three major features that have been fully implemented on the **Guacamole server side**:

1. **Multi-Monitor Support** - Dynamic multi-monitor layouts with drag-and-drop positioning
2. **Webcam Redirection** - Client webcam streaming to remote desktop
3. **Guacamole Copilot** - AI-powered assistant with workflows, suggestions, and automation

**Server code is 100% complete and tested.** Your job is to implement the client-side JavaScript/TypeScript code to integrate with the server.

---

## 📚 BACKGROUND CONTEXT

### What is Guacamole?
Apache Guacamole is a clientless remote desktop gateway. It supports:
- RDP (Remote Desktop Protocol)
- SSH (Secure Shell)
- VNC (Virtual Network Computing)
- Kubernetes

The architecture:
```
Browser (HTML5 Client)
    ↕ WebSocket
Guacamole Server (guacd)
    ↕ Native Protocol
Remote Machine (RDP/SSH/VNC)
```

### Client Technology Stack
- **Language**: JavaScript/TypeScript
- **Framework**: Guacamole client library (guacamole-common-js)
- **Protocol**: Guacamole protocol over WebSocket
- **UI**: HTML5 Canvas, DOM manipulation

### Server Changes Overview
We made 3 commits adding functionality:

**Commit 1**: Multi-monitor optimizations
- Buffer overflow fixes
- Performance improvements (O(n²) → O(n))
- Monitor layout tracking
- JSON protocol for monitor positions

**Commit 2**: Webcam redirection
- Camera channel implementation
- Device path configuration
- Connection status tracking

**Commit 3**: Guacamole Copilot
- AI assistant with 8 built-in workflows
- Command suggestions
- Quick actions
- Recording system
- Context tracking

---

## 🎨 FEATURE 1: MULTI-MONITOR SUPPORT

### Overview
Users can have multiple monitors in their RDP/SSH sessions. The server tracks monitor positions and sends layout info to the client. The client needs to:
1. Display all monitors
2. Allow drag-and-drop positioning
3. Send monitor positions to server
4. Handle dynamic add/remove monitors

### Server Implementation Details

#### Protocol Message
Server sends monitor layout via the `set` instruction:

```
set <layer> multimon-layout <json>
```

Example JSON:
```json
{
  "0": {
    "left": 0,
    "top": 0,
    "width": 1920,
    "height": 1080
  },
  "1": {
    "left": 1920,
    "top": 0,
    "width": 1920,
    "height": 1080
  }
}
```

#### Size Instruction Enhanced
The `size` instruction now accepts 4 parameters:
```
size <width> <height> <x_position> <top_offset>
```

- `width`: Monitor width in pixels
- `height`: Monitor height in pixels
- `x_position`: Monitor index (0 = primary, 1 = secondary, etc.)
- `top_offset`: Vertical offset from top edge in pixels

#### Configuration Parameter
```
secondary-monitors=2
```
Maximum number of secondary monitors (default: 0 = disabled)

### Client Implementation Requirements

#### 1. Parse Monitor Layout
```javascript
// Listen for 'set' instruction with multimon-layout parameter
client.oninstruction = function(opcode, args) {
    if (opcode === 'set' && args[1] === 'multimon-layout') {
        const layout = JSON.parse(args[2]);
        updateMonitorLayout(layout);
    }
};
```

#### 2. Display Multiple Monitors
```javascript
function updateMonitorLayout(layout) {
    // layout = {
    //   "0": {left: 0, top: 0, width: 1920, height: 1080},
    //   "1": {left: 1920, top: 0, width: 1920, height: 1080}
    // }

    // Create/update monitor divs
    Object.keys(layout).forEach(monitorId => {
        const mon = layout[monitorId];
        let monitorDiv = document.getElementById(`monitor-${monitorId}`);

        if (!monitorDiv) {
            monitorDiv = createMonitorDiv(monitorId);
        }

        positionMonitor(monitorDiv, mon.left, mon.top, mon.width, mon.height);
    });
}
```

#### 3. Send Monitor Positions to Server
When user resizes or moves monitors:
```javascript
function sendMonitorSize(monitorIndex, width, height, left, top) {
    // Send size instruction: size width height x_position top_offset
    client.sendSize(width, height, monitorIndex, top);
}
```

#### 4. Add/Remove Monitors UI
```javascript
// Button to add secondary monitor
function addMonitor() {
    const nextIndex = getMonitorCount();
    const maxMonitors = getMaxSecondaryMonitors(); // From connection params

    if (nextIndex <= maxMonitors) {
        // Position new monitor to the right of last monitor
        const lastMonitor = getLastMonitor();
        const left = lastMonitor.left + lastMonitor.width;

        sendMonitorSize(nextIndex, 1920, 1080, left, 0);
    }
}

// Remove monitor (send 0x0 size)
function removeMonitor(index) {
    if (index > 0) { // Can't remove primary
        sendMonitorSize(index, 0, 0, 0, 0);
    }
}
```

#### 5. Drag and Drop Positioning
```javascript
function enableMonitorDragging(monitorDiv, monitorIndex) {
    let isDragging = false;
    let startX, startY;

    monitorDiv.onmousedown = function(e) {
        isDragging = true;
        startX = e.clientX - monitorDiv.offsetLeft;
        startY = e.clientY - monitorDiv.offsetTop;
    };

    document.onmousemove = function(e) {
        if (isDragging) {
            const left = e.clientX - startX;
            const top = e.clientY - startY;

            monitorDiv.style.left = left + 'px';
            monitorDiv.style.top = top + 'px';
        }
    };

    document.onmouseup = function(e) {
        if (isDragging) {
            isDragging = false;

            // Send new position to server
            const left = parseInt(monitorDiv.style.left);
            const top = parseInt(monitorDiv.style.top);
            const width = monitorDiv.offsetWidth;
            const height = monitorDiv.offsetHeight;

            sendMonitorSize(monitorIndex, width, height, left, top);
        }
    };
}
```

#### 6. Monitor Settings UI
Create a settings panel:
```html
<div id="monitor-settings">
    <h3>Monitor Layout</h3>
    <div id="monitor-list">
        <!-- Monitor items here -->
    </div>
    <button onclick="addMonitor()">Add Monitor</button>
    <button onclick="autoArrange()">Auto Arrange</button>
</div>
```

#### 7. Handle Connection Parameters
```javascript
// Parse connection parameters
const params = getConnectionParams();
const maxSecondaryMonitors = parseInt(params['secondary-monitors'] || '0');

// Store for later use
connection.maxSecondaryMonitors = maxSecondaryMonitors;
```

### Testing Scenarios

1. **Single Monitor** (default)
   - Connect normally
   - Should see one monitor
   - Resize should work as before

2. **Add Secondary Monitor**
   - Click "Add Monitor"
   - New monitor appears to the right
   - Server receives size instruction with index 1

3. **Drag Monitors**
   - Drag secondary monitor to different position
   - Release mouse
   - Server receives updated position

4. **Remove Monitor**
   - Click "Remove" on secondary monitor
   - Monitor disappears
   - Server receives 0x0 size for that index

5. **Reconnect**
   - Disconnect and reconnect
   - Monitor layout should be restored from server

### Edge Cases to Handle

- **Maximum monitors reached**: Disable "Add Monitor" button
- **Primary monitor**: Should not be removable or have index changed
- **Overlapping monitors**: Visual feedback or snap-to-grid
- **Negative positions**: Allow (monitors can be above primary)
- **Layout persistence**: Save/restore layout in localStorage

---

## 📹 FEATURE 2: WEBCAM REDIRECTION

### Overview
User's local webcam is redirected to the remote desktop. Applications on the remote machine can use the webcam as if it were locally attached.

### Server Implementation Details

#### Channel
Server loads the `camera` channel when enabled:
```c
enable-camera=true
camera-device=/dev/video0  // Optional, server will use default
```

#### Protocol
Uses FreeRDP's camera redirection channel. Server expects:
1. Video stream from client
2. Camera capabilities (resolution, formats)
3. Control messages (start/stop)

### Client Implementation Requirements

#### 1. Check if Camera Enabled
```javascript
// From connection parameters
const cameraEnabled = params['enable-camera'] === 'true';

if (cameraEnabled) {
    initializeCameraRedirection();
}
```

#### 2. Request Camera Access
```javascript
async function initializeCameraRedirection() {
    try {
        const stream = await navigator.mediaDevices.getUserMedia({
            video: {
                width: { ideal: 1280 },
                height: { ideal: 720 },
                frameRate: { ideal: 30 }
            }
        });

        setupCameraStream(stream);

    } catch (error) {
        console.error('Camera access denied:', error);
        showCameraError('Please allow camera access');
    }
}
```

#### 3. Display Camera Preview (Optional)
```javascript
function setupCameraStream(stream) {
    // Optional: Show preview to user
    const preview = document.getElementById('camera-preview');
    if (preview) {
        preview.srcObject = stream;
        preview.play();
    }

    // Get video track
    const videoTrack = stream.getVideoTracks()[0];
    const settings = videoTrack.getSettings();

    console.log('Camera ready:', settings.width, 'x', settings.height);

    // Send camera capabilities to server
    sendCameraCapabilities(settings);

    // Start streaming
    startCameraStreaming(stream);
}
```

#### 4. Send Video Stream to Server
```javascript
function startCameraStreaming(stream) {
    const canvas = document.createElement('canvas');
    const ctx = canvas.getContext('2d');
    const video = document.createElement('video');

    video.srcObject = stream;
    video.play();

    video.onloadedmetadata = function() {
        canvas.width = video.videoWidth;
        canvas.height = video.videoHeight;

        // Capture frames and send to server
        const frameRate = 30; // FPS
        const interval = 1000 / frameRate;

        setInterval(() => {
            ctx.drawImage(video, 0, 0);
            const imageData = canvas.toDataURL('image/jpeg', 0.8);

            // Send frame to server via blob stream
            sendCameraFrame(imageData);
        }, interval);
    };
}
```

#### 5. Send Camera Capabilities
```javascript
function sendCameraCapabilities(settings) {
    const capabilities = {
        width: settings.width,
        height: settings.height,
        frameRate: settings.frameRate,
        format: 'MJPEG' // Or H.264 if supported
    };

    // Send via argv instruction
    client.sendArgv('camera-capabilities', JSON.stringify(capabilities));
}
```

#### 6. Handle Camera Controls
```javascript
// Server may send control commands
client.oninstruction = function(opcode, args) {
    if (opcode === 'argv' && args[0] === 'camera-control') {
        const command = JSON.parse(args[1]);

        switch (command.action) {
            case 'start':
                startCameraStreaming(currentStream);
                break;
            case 'stop':
                stopCameraStreaming();
                break;
            case 'change-resolution':
                changeCameraResolution(command.width, command.height);
                break;
        }
    }
};
```

#### 7. Camera Status Indicator
```html
<div id="camera-status">
    <span class="status-icon" id="camera-icon">📹</span>
    <span id="camera-text">Camera: Active</span>
</div>
```

```javascript
function updateCameraStatus(active) {
    const icon = document.getElementById('camera-icon');
    const text = document.getElementById('camera-text');

    if (active) {
        icon.style.color = 'green';
        text.textContent = 'Camera: Active';
    } else {
        icon.style.color = 'gray';
        text.textContent = 'Camera: Inactive';
    }
}
```

### Testing Scenarios

1. **Camera Permission Granted**
   - User allows camera access
   - Preview shows camera feed
   - Remote desktop can use camera

2. **Camera Permission Denied**
   - Show error message
   - Offer button to retry
   - Connection continues without camera

3. **Multiple Cameras**
   - Allow user to select camera
   - Update camera-device parameter
   - Switch cameras during session

4. **Camera Disconnect**
   - Camera unplugged
   - Detect and notify user
   - Auto-reconnect when plugged back in

5. **Low Bandwidth**
   - Reduce frame rate automatically
   - Lower JPEG quality
   - Show bandwidth indicator

### Edge Cases

- **No camera available**: Graceful degradation
- **Camera in use**: Show "Camera busy" message
- **Browser compatibility**: Check for getUserMedia support
- **HTTPS required**: Camera only works on HTTPS
- **Mobile devices**: Handle front/back camera switching

---

## 🤖 FEATURE 3: GUACAMOLE COPILOT

### Overview
AI-powered assistant that provides:
- Command suggestions based on context
- Pre-built automation workflows (8 total)
- Quick action buttons
- Workflow recording
- Session insights

### Server Implementation Details

#### Configuration
```
enable-copilot=true
```

#### Protocol - Server → Client Messages
Server sends copilot messages via `argv` instruction:
```
argv <timestamp> "text/plain" "copilot" <json_data>
```

#### Message Types

**1. Suggestions**
```json
{
  "type": "suggestions",
  "items": ["ls -la", "cd ~", "pwd"]
}
```

**2. Workflow List**
```json
{
  "type": "workflows",
  "items": [
    {
      "name": "system-diagnostics",
      "description": "Run comprehensive system diagnostics",
      "steps": 6,
      "protocol": "ssh"
    },
    {
      "name": "security-scan",
      "description": "Quick security check",
      "steps": 5,
      "protocol": "ssh"
    }
  ]
}
```

**3. Workflow Execution**
```json
{
  "type": "workflow_start",
  "name": "system-diagnostics",
  "steps": 6
}
```

```json
{
  "type": "workflow_step",
  "step": 1,
  "description": "Check disk usage",
  "command": "df -h"
}
```

```json
{
  "type": "workflow_complete",
  "name": "system-diagnostics"
}
```

**4. Quick Actions**
```json
{
  "type": "quick_actions",
  "items": [
    {
      "name": "list-files",
      "label": "List Files",
      "icon": "folder",
      "command": "ls -lah"
    }
  ]
}
```

**5. Context Help**
```json
{
  "type": "help",
  "protocol": "ssh",
  "os": "Ubuntu",
  "directory": "/home/user"
}
```

**6. Session Insights**
```json
{
  "type": "insights",
  "session_duration": 3600,
  "commands_executed": 45,
  "protocol": "ssh",
  "privileged": false
}
```

**7. Recording Status**
```json
{
  "type": "recording_started",
  "name": "my-workflow"
}
```

```json
{
  "type": "recording_stopped",
  "name": "my-workflow",
  "steps": 5
}
```

#### Protocol - Client → Server Messages
Client sends copilot commands via `argv`:
```
argv <timestamp> "text/plain" "copilot-command" <json_data>
```

**Request Suggestions**
```json
{
  "command": "suggest",
  "input": "ls"
}
```

**Execute Workflow**
```json
{
  "command": "execute_workflow",
  "workflow_name": "system-diagnostics"
}
```

**List Workflows**
```json
{
  "command": "list_workflows"
}
```

**Start Recording**
```json
{
  "command": "record_workflow",
  "workflow_name": "my-custom-workflow"
}
```

**Stop Recording**
```json
{
  "command": "record_workflow",
  "workflow_name": ""
}
```

**Get Session Insights**
```json
{
  "command": "session_insights"
}
```

### Client Implementation Requirements

#### 1. Copilot Panel UI

Create a slide-out panel:

```html
<div id="copilot-panel" class="copilot-panel hidden">
    <div class="copilot-header">
        <h2>🤖 Guacamole Copilot</h2>
        <button class="close-btn" onclick="closeCopilot()">×</button>
    </div>

    <div class="copilot-tabs">
        <button class="tab active" onclick="showTab('workflows')">Workflows</button>
        <button class="tab" onclick="showTab('quick-actions')">Quick Actions</button>
        <button class="tab" onclick="showTab('suggestions')">Suggestions</button>
        <button class="tab" onclick="showTab('insights')">Insights</button>
    </div>

    <div class="copilot-content">
        <div id="workflows-tab" class="tab-content active">
            <!-- Workflow list here -->
        </div>

        <div id="quick-actions-tab" class="tab-content">
            <!-- Quick actions here -->
        </div>

        <div id="suggestions-tab" class="tab-content">
            <input type="text" id="suggestion-input"
                   placeholder="Type a command..."
                   oninput="requestSuggestions(this.value)">
            <div id="suggestions-list"></div>
        </div>

        <div id="insights-tab" class="tab-content">
            <!-- Session insights here -->
        </div>
    </div>

    <div class="copilot-footer">
        <button id="record-btn" onclick="toggleRecording()">
            ⏺ Start Recording
        </button>
    </div>
</div>
```

#### 2. Keyboard Shortcut to Open Copilot

```javascript
// Listen for Ctrl+Alt+H or Ctrl+Shift+Space
document.addEventListener('keydown', function(e) {
    // Ctrl+Alt+H
    if (e.ctrlKey && e.altKey && e.key === 'h') {
        e.preventDefault();
        toggleCopilot();
    }

    // Or Ctrl+Shift+Space
    if (e.ctrlKey && e.shiftKey && e.key === ' ') {
        e.preventDefault();
        toggleCopilot();
    }
});

function toggleCopilot() {
    const panel = document.getElementById('copilot-panel');

    if (panel.classList.contains('hidden')) {
        showCopilot();
    } else {
        closeCopilot();
    }
}

function showCopilot() {
    const panel = document.getElementById('copilot-panel');
    panel.classList.remove('hidden');
    panel.classList.add('visible');

    // Request initial data from server
    requestWorkflowList();
    requestSessionInsights();
}

function closeCopilot() {
    const panel = document.getElementById('copilot-panel');
    panel.classList.remove('visible');
    panel.classList.add('hidden');
}
```

#### 3. Handle Server Messages

```javascript
// Listen for copilot messages from server
client.oninstruction = function(opcode, args) {
    if (opcode === 'argv' && args[2] === 'copilot') {
        const message = JSON.parse(args[3]);
        handleCopilotMessage(message);
    }
};

function handleCopilotMessage(message) {
    switch (message.type) {
        case 'suggestions':
            displaySuggestions(message.items);
            break;

        case 'workflows':
            displayWorkflows(message.items);
            break;

        case 'workflow_start':
            onWorkflowStart(message);
            break;

        case 'workflow_step':
            onWorkflowStep(message);
            break;

        case 'workflow_complete':
            onWorkflowComplete(message);
            break;

        case 'quick_actions':
            displayQuickActions(message.items);
            break;

        case 'insights':
            displayInsights(message);
            break;

        case 'recording_started':
            onRecordingStarted(message);
            break;

        case 'recording_stopped':
            onRecordingStopped(message);
            break;

        case 'help':
            displayContextHelp(message);
            break;
    }
}
```

#### 4. Display Workflows

```javascript
function displayWorkflows(workflows) {
    const container = document.getElementById('workflows-tab');
    container.innerHTML = '<h3>Available Workflows</h3>';

    workflows.forEach(workflow => {
        const workflowDiv = document.createElement('div');
        workflowDiv.className = 'workflow-item';
        workflowDiv.innerHTML = `
            <div class="workflow-header">
                <h4>${workflow.name}</h4>
                <span class="protocol-badge">${workflow.protocol}</span>
            </div>
            <p class="workflow-description">${workflow.description}</p>
            <div class="workflow-footer">
                <span class="step-count">${workflow.steps} steps</span>
                <button onclick="executeWorkflow('${workflow.name}')">
                    Run Workflow
                </button>
            </div>
        `;

        container.appendChild(workflowDiv);
    });
}
```

#### 5. Execute Workflow

```javascript
function executeWorkflow(workflowName) {
    console.log('Executing workflow:', workflowName);

    // Show progress indicator
    showWorkflowProgress(workflowName);

    // Send command to server
    const command = {
        command: 'execute_workflow',
        workflow_name: workflowName
    };

    client.sendArgv('copilot-command', JSON.stringify(command));
}

function showWorkflowProgress(workflowName) {
    const progressDiv = document.createElement('div');
    progressDiv.id = 'workflow-progress';
    progressDiv.className = 'workflow-progress';
    progressDiv.innerHTML = `
        <h4>Executing: ${workflowName}</h4>
        <div class="progress-bar">
            <div class="progress-fill" id="progress-fill"></div>
        </div>
        <div id="progress-steps"></div>
    `;

    document.getElementById('copilot-panel').appendChild(progressDiv);
}

function onWorkflowStep(message) {
    const stepsDiv = document.getElementById('progress-steps');
    const stepDiv = document.createElement('div');
    stepDiv.className = 'workflow-step';
    stepDiv.innerHTML = `
        <span class="step-number">${message.step}</span>
        <span class="step-description">${message.description}</span>
        <span class="step-status">✓</span>
    `;

    stepsDiv.appendChild(stepDiv);

    // Execute the command (send to terminal)
    sendCommand(message.command);

    // Update progress bar
    updateProgressBar(message.step);
}

function onWorkflowComplete(message) {
    const progressDiv = document.getElementById('workflow-progress');

    // Add completion message
    const completeDiv = document.createElement('div');
    completeDiv.className = 'workflow-complete';
    completeDiv.innerHTML = `
        <span class="success-icon">✓</span>
        <span>Workflow completed successfully!</span>
    `;

    progressDiv.appendChild(completeDiv);

    // Auto-close after 3 seconds
    setTimeout(() => {
        progressDiv.remove();
    }, 3000);
}
```

#### 6. Command Suggestions

```javascript
function requestSuggestions(input) {
    // Debounce requests
    clearTimeout(suggestionTimeout);

    suggestionTimeout = setTimeout(() => {
        const command = {
            command: 'suggest',
            input: input
        };

        client.sendArgv('copilot-command', JSON.stringify(command));
    }, 300); // Wait 300ms after typing stops
}

function displaySuggestions(suggestions) {
    const listDiv = document.getElementById('suggestions-list');
    listDiv.innerHTML = '';

    if (suggestions.length === 0) {
        listDiv.innerHTML = '<p class="no-suggestions">No suggestions available</p>';
        return;
    }

    suggestions.forEach(suggestion => {
        const suggestionDiv = document.createElement('div');
        suggestionDiv.className = 'suggestion-item';
        suggestionDiv.innerHTML = `
            <span class="suggestion-command">${suggestion}</span>
            <button onclick="applySuggestion('${suggestion}')">Apply</button>
        `;

        listDiv.appendChild(suggestionDiv);
    });
}

function applySuggestion(command) {
    // Send command to terminal
    sendCommand(command);

    // Visual feedback
    const input = document.getElementById('suggestion-input');
    input.value = command;
    input.style.backgroundColor = '#d4edda'; // Green flash

    setTimeout(() => {
        input.style.backgroundColor = '';
    }, 500);
}
```

#### 7. Quick Actions

```javascript
function displayQuickActions(actions) {
    const container = document.getElementById('quick-actions-tab');
    container.innerHTML = '<h3>Quick Actions</h3>';

    const actionsGrid = document.createElement('div');
    actionsGrid.className = 'quick-actions-grid';

    actions.forEach(action => {
        const actionBtn = document.createElement('button');
        actionBtn.className = 'quick-action-btn';
        actionBtn.innerHTML = `
            <span class="action-icon">${action.icon}</span>
            <span class="action-label">${action.label}</span>
        `;

        actionBtn.onclick = function() {
            sendCommand(action.command);
            showNotification(`Executed: ${action.label}`);
        };

        actionsGrid.appendChild(actionBtn);
    });

    container.appendChild(actionsGrid);
}
```

#### 8. Recording System

```javascript
let isRecording = false;
let recordingName = '';

function toggleRecording() {
    if (!isRecording) {
        // Start recording
        const name = prompt('Enter workflow name:');
        if (name) {
            startRecording(name);
        }
    } else {
        // Stop recording
        stopRecording();
    }
}

function startRecording(name) {
    recordingName = name;
    isRecording = true;

    // Update UI
    const btn = document.getElementById('record-btn');
    btn.innerHTML = '⏹ Stop Recording';
    btn.classList.add('recording');

    // Send to server
    const command = {
        command: 'record_workflow',
        workflow_name: name
    };

    client.sendArgv('copilot-command', JSON.stringify(command));

    showNotification(`Recording workflow: ${name}`);
}

function stopRecording() {
    isRecording = false;

    // Update UI
    const btn = document.getElementById('record-btn');
    btn.innerHTML = '⏺ Start Recording';
    btn.classList.remove('recording');

    // Send to server
    const command = {
        command: 'record_workflow',
        workflow_name: ''  // Empty name stops recording
    };

    client.sendArgv('copilot-command', JSON.stringify(command));

    showNotification('Recording stopped');
}

function onRecordingStarted(message) {
    showNotification(`Started recording: ${message.name}`);
}

function onRecordingStopped(message) {
    showNotification(`Saved workflow: ${message.name} (${message.steps} steps)`);

    // Refresh workflow list
    requestWorkflowList();
}
```

#### 9. Session Insights

```javascript
function displayInsights(insights) {
    const container = document.getElementById('insights-tab');

    const duration = formatDuration(insights.session_duration);

    container.innerHTML = `
        <h3>Session Insights</h3>
        <div class="insight-grid">
            <div class="insight-card">
                <span class="insight-label">Session Duration</span>
                <span class="insight-value">${duration}</span>
            </div>
            <div class="insight-card">
                <span class="insight-label">Commands Executed</span>
                <span class="insight-value">${insights.commands_executed}</span>
            </div>
            <div class="insight-card">
                <span class="insight-label">Protocol</span>
                <span class="insight-value">${insights.protocol.toUpperCase()}</span>
            </div>
            <div class="insight-card">
                <span class="insight-label">Privileges</span>
                <span class="insight-value">${insights.privileged ? 'Elevated' : 'Normal'}</span>
            </div>
        </div>
    `;
}

function formatDuration(seconds) {
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;

    if (hours > 0) {
        return `${hours}h ${minutes}m`;
    } else if (minutes > 0) {
        return `${minutes}m ${secs}s`;
    } else {
        return `${secs}s`;
    }
}
```

#### 10. Helper Functions

```javascript
function requestWorkflowList() {
    const command = {
        command: 'list_workflows'
    };

    client.sendArgv('copilot-command', JSON.stringify(command));
}

function requestSessionInsights() {
    const command = {
        command: 'session_insights'
    };

    client.sendArgv('copilot-command', JSON.stringify(command));
}

function sendCommand(cmd) {
    // This function sends a command to the terminal
    // Implementation depends on your terminal implementation

    // For SSH:
    if (client.protocol === 'ssh') {
        client.sendText(cmd + '\n');
    }

    // For RDP:
    if (client.protocol === 'rdp') {
        // Send via clipboard or text injection
        client.sendKeys(cmd);
        client.sendKey(0xFF0D); // Enter key
    }
}

function showNotification(message) {
    // Show a toast notification
    const notification = document.createElement('div');
    notification.className = 'copilot-notification';
    notification.textContent = message;

    document.body.appendChild(notification);

    setTimeout(() => {
        notification.classList.add('show');
    }, 10);

    setTimeout(() => {
        notification.classList.remove('show');
        setTimeout(() => {
            notification.remove();
        }, 300);
    }, 3000);
}
```

#### 11. Styling (CSS)

```css
.copilot-panel {
    position: fixed;
    right: -400px;
    top: 0;
    width: 400px;
    height: 100%;
    background: #ffffff;
    box-shadow: -2px 0 10px rgba(0,0,0,0.2);
    transition: right 0.3s ease;
    z-index: 10000;
    display: flex;
    flex-direction: column;
}

.copilot-panel.visible {
    right: 0;
}

.copilot-panel.hidden {
    right: -400px;
}

.copilot-header {
    padding: 20px;
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    color: white;
    display: flex;
    justify-content: space-between;
    align-items: center;
}

.copilot-header h2 {
    margin: 0;
    font-size: 20px;
}

.close-btn {
    background: transparent;
    border: none;
    color: white;
    font-size: 30px;
    cursor: pointer;
    line-height: 1;
}

.copilot-tabs {
    display: flex;
    background: #f5f5f5;
    border-bottom: 1px solid #ddd;
}

.copilot-tabs .tab {
    flex: 1;
    padding: 12px 8px;
    background: transparent;
    border: none;
    cursor: pointer;
    font-size: 13px;
    transition: background 0.2s;
}

.copilot-tabs .tab.active {
    background: white;
    border-bottom: 3px solid #667eea;
}

.copilot-content {
    flex: 1;
    overflow-y: auto;
    padding: 20px;
}

.tab-content {
    display: none;
}

.tab-content.active {
    display: block;
}

.workflow-item {
    background: #f8f9fa;
    border-radius: 8px;
    padding: 15px;
    margin-bottom: 15px;
    border: 1px solid #e9ecef;
}

.workflow-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 10px;
}

.workflow-header h4 {
    margin: 0;
    font-size: 16px;
}

.protocol-badge {
    background: #667eea;
    color: white;
    padding: 4px 12px;
    border-radius: 12px;
    font-size: 11px;
    text-transform: uppercase;
}

.workflow-description {
    font-size: 14px;
    color: #6c757d;
    margin: 10px 0;
}

.workflow-footer {
    display: flex;
    justify-content: space-between;
    align-items: center;
}

.step-count {
    font-size: 12px;
    color: #6c757d;
}

.workflow-footer button {
    background: #667eea;
    color: white;
    border: none;
    padding: 8px 16px;
    border-radius: 6px;
    cursor: pointer;
    font-size: 14px;
}

.workflow-footer button:hover {
    background: #5568d3;
}

.quick-actions-grid {
    display: grid;
    grid-template-columns: repeat(2, 1fr);
    gap: 12px;
}

.quick-action-btn {
    background: white;
    border: 2px solid #e9ecef;
    border-radius: 8px;
    padding: 20px;
    cursor: pointer;
    transition: all 0.2s;
    text-align: center;
}

.quick-action-btn:hover {
    border-color: #667eea;
    box-shadow: 0 4px 12px rgba(102,126,234,0.15);
}

.action-icon {
    font-size: 32px;
    display: block;
    margin-bottom: 8px;
}

.action-label {
    font-size: 13px;
    display: block;
}

.suggestion-item {
    background: white;
    border: 1px solid #e9ecef;
    border-radius: 6px;
    padding: 12px;
    margin-bottom: 8px;
    display: flex;
    justify-content: space-between;
    align-items: center;
}

.suggestion-command {
    font-family: 'Courier New', monospace;
    font-size: 14px;
}

.suggestion-item button {
    background: #28a745;
    color: white;
    border: none;
    padding: 6px 12px;
    border-radius: 4px;
    cursor: pointer;
    font-size: 12px;
}

.workflow-progress {
    position: fixed;
    bottom: 20px;
    right: 420px;
    width: 350px;
    background: white;
    border-radius: 12px;
    padding: 20px;
    box-shadow: 0 8px 24px rgba(0,0,0,0.15);
    z-index: 10001;
}

.progress-bar {
    height: 8px;
    background: #e9ecef;
    border-radius: 4px;
    overflow: hidden;
    margin: 15px 0;
}

.progress-fill {
    height: 100%;
    background: linear-gradient(90deg, #667eea 0%, #764ba2 100%);
    width: 0%;
    transition: width 0.3s ease;
}

.workflow-step {
    display: flex;
    align-items: center;
    gap: 10px;
    padding: 8px 0;
    border-bottom: 1px solid #f0f0f0;
}

.step-number {
    background: #667eea;
    color: white;
    width: 24px;
    height: 24px;
    border-radius: 50%;
    display: flex;
    align-items: center;
    justify-content: center;
    font-size: 12px;
    flex-shrink: 0;
}

.step-description {
    flex: 1;
    font-size: 13px;
}

.step-status {
    color: #28a745;
    font-size: 18px;
}

.workflow-complete {
    text-align: center;
    padding: 15px;
    background: #d4edda;
    border-radius: 6px;
    margin-top: 15px;
    color: #155724;
}

.success-icon {
    font-size: 24px;
    margin-right: 8px;
}

.insight-grid {
    display: grid;
    grid-template-columns: repeat(2, 1fr);
    gap: 15px;
    margin-top: 20px;
}

.insight-card {
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    color: white;
    padding: 20px;
    border-radius: 12px;
    text-align: center;
}

.insight-label {
    display: block;
    font-size: 12px;
    opacity: 0.9;
    margin-bottom: 8px;
    text-transform: uppercase;
    letter-spacing: 0.5px;
}

.insight-value {
    display: block;
    font-size: 24px;
    font-weight: bold;
}

.copilot-footer {
    padding: 15px 20px;
    border-top: 1px solid #e9ecef;
    background: #f8f9fa;
}

#record-btn {
    width: 100%;
    padding: 12px;
    background: #dc3545;
    color: white;
    border: none;
    border-radius: 8px;
    cursor: pointer;
    font-size: 14px;
    font-weight: 500;
}

#record-btn.recording {
    background: #ffc107;
    color: #000;
    animation: pulse 1.5s infinite;
}

@keyframes pulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.7; }
}

.copilot-notification {
    position: fixed;
    bottom: 20px;
    left: 50%;
    transform: translateX(-50%) translateY(100px);
    background: #333;
    color: white;
    padding: 12px 24px;
    border-radius: 8px;
    box-shadow: 0 4px 12px rgba(0,0,0,0.2);
    opacity: 0;
    transition: all 0.3s ease;
    z-index: 10002;
}

.copilot-notification.show {
    transform: translateX(-50%) translateY(0);
    opacity: 1;
}

#suggestion-input {
    width: 100%;
    padding: 12px;
    border: 2px solid #e9ecef;
    border-radius: 8px;
    font-size: 14px;
    margin-bottom: 15px;
    font-family: 'Courier New', monospace;
}

#suggestion-input:focus {
    outline: none;
    border-color: #667eea;
}

.no-suggestions {
    text-align: center;
    color: #6c757d;
    padding: 20px;
    font-style: italic;
}
```

### Testing Scenarios

1. **Open Copilot Panel**
   - Press Ctrl+Alt+H
   - Panel slides in from right
   - Workflows tab is active

2. **Browse Workflows**
   - See 8 built-in workflows listed
   - Each shows name, description, steps, protocol

3. **Execute Workflow**
   - Click "Run Workflow" on "system-diagnostics"
   - Progress popup appears
   - See each step execute in real-time
   - Commands appear in terminal
   - Completion message shows

4. **Quick Actions**
   - Switch to Quick Actions tab
   - See buttons for common commands
   - Click "Disk Usage"
   - Command "df -h" executes in terminal

5. **Command Suggestions**
   - Switch to Suggestions tab
   - Type "l"
   - See suggestions: "ls -la", "ll"
   - Click "Apply" on suggestion
   - Command executes

6. **Record Workflow**
   - Click "Start Recording"
   - Enter workflow name "my-test"
   - Execute some commands manually
   - Click "Stop Recording"
   - New workflow appears in list

7. **Session Insights**
   - Switch to Insights tab
   - See session duration, command count, protocol
   - Refresh shows updated values

### Edge Cases

- **Copilot disabled**: Don't show activation button/shortcut
- **Long workflow execution**: Show estimated time remaining
- **Network disconnection**: Show "Reconnecting..." in panel
- **Invalid workflow**: Show error message
- **Recording with no commands**: Warn user before saving
- **Mobile devices**: Adapt panel to bottom sheet on small screens

---

## 🎯 IMPLEMENTATION PRIORITIES

### Phase 1: Core Multi-Monitor (Week 1)
1. Parse monitor layout JSON
2. Display monitors on canvas
3. Send size instructions with monitor index
4. Basic add/remove monitor UI

### Phase 2: Advanced Multi-Monitor (Week 1-2)
1. Drag-and-drop positioning
2. Monitor settings panel
3. Layout persistence
4. Visual improvements

### Phase 3: Webcam Redirection (Week 2)
1. Camera permission handling
2. Stream capture and encoding
3. Send frames to server
4. Status indicators
5. Camera selection UI

### Phase 4: Copilot UI (Week 3)
1. Copilot panel structure
2. Keyboard shortcut
3. Message handling from server
4. Workflow display

### Phase 5: Copilot Workflows (Week 3-4)
1. Workflow execution with progress
2. Quick actions
3. Command suggestions
4. Recording system

### Phase 6: Copilot Polish (Week 4)
1. Session insights
2. Animations and transitions
3. Error handling
4. Mobile responsive design

---

## 🧪 TESTING CHECKLIST

### Multi-Monitor Testing
- [ ] Single monitor (default behavior)
- [ ] Add secondary monitor
- [ ] Remove secondary monitor
- [ ] Drag monitor to new position
- [ ] Negative Y offset (monitor above primary)
- [ ] Reconnect preserves layout
- [ ] Maximum monitor limit respected
- [ ] Primary monitor can't be removed

### Webcam Testing
- [ ] Camera permission granted
- [ ] Camera permission denied
- [ ] No camera available
- [ ] Switch between cameras
- [ ] Camera preview visible
- [ ] Remote app can use camera
- [ ] Reconnect restores camera
- [ ] Low bandwidth handling

### Copilot Testing
- [ ] Open with keyboard shortcut
- [ ] Close with X button or shortcut
- [ ] Workflow list loads
- [ ] Execute workflow shows progress
- [ ] All steps execute in order
- [ ] Quick actions work
- [ ] Suggestions appear on typing
- [ ] Apply suggestion executes command
- [ ] Recording starts/stops
- [ ] Recorded workflow saved
- [ ] Session insights display
- [ ] All tabs work correctly

---

## 📦 DELIVERABLES

When complete, you should have:

1. **Multi-Monitor System**
   - Monitor layout parser
   - Display rendering
   - Drag-and-drop UI
   - Settings panel
   - Size instruction sender

2. **Webcam Redirection**
   - Camera access handler
   - Video stream encoder
   - Frame sender
   - Status UI
   - Camera selector

3. **Copilot System**
   - Panel UI with all tabs
   - Workflow executor
   - Quick actions grid
   - Suggestion engine
   - Recording system
   - Session insights
   - Message protocol handler

4. **Documentation**
   - API documentation
   - User guide
   - Troubleshooting guide
   - Video demos (optional)

---

## 💡 TIPS FOR SUCCESS

1. **Start Small**: Get basic functionality working first, then add polish

2. **Test Frequently**: Test each feature as you build it

3. **Follow Patterns**: Look at existing Guacamole client code for patterns

4. **Handle Errors**: Gracefully handle all error cases

5. **Performance**: Optimize rendering and message handling

6. **Accessibility**: Add ARIA labels and keyboard navigation

7. **Mobile**: Make sure it works on tablets

8. **Browser Compat**: Test on Chrome, Firefox, Safari, Edge

9. **Documentation**: Comment complex code

10. **Ask Questions**: If something is unclear, ask!

---

## 📞 SUPPORT

If you need clarification on any part of the implementation:

1. Check the server code in the repository
2. Review the COPILOT_README.md for Copilot details
3. Look at existing Guacamole protocol documentation
4. Test the server endpoints to understand behavior

---

## 🚀 YOU GOT THIS!

This is an EXCITING project that will make Guacamole 10x better! The server is solid and ready. Now it's time to build an amazing user experience on the client side.

Take it step by step, test thoroughly, and create something users will love!

Good luck! 🎉
