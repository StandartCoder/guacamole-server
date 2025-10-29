# Guacamole Copilot - AI-Powered Assistant for Remote Sessions

**Guacamole Copilot** is an intelligent AI assistant integrated into the Guacamole remote desktop system. It provides context-aware automation, workflow execution, command suggestions, and troubleshooting assistance during RDP, SSH, VNC, and other protocol sessions.

## Features

### 🚀 Core Capabilities

- **Context-Aware Suggestions**: Get intelligent command suggestions based on your current session, OS type, and command history
- **Workflow/Playbook Execution**: Run pre-built automation workflows or create your own
- **Quick Actions**: One-click access to common tasks
- **Session Recording**: Record your actions and turn them into reusable workflows
- **Error Detection**: Automatically detect and track errors in command output
- **Multi-Protocol Support**: Works with RDP, SSH, VNC, and more

### 📋 Built-in Workflows

#### SSH Workflows

1. **system-diagnostics**: Comprehensive system health check
   - Disk usage, memory, CPU info
   - Running processes and network connections
   - System uptime

2. **security-scan**: Quick security audit
   - Check for available updates
   - Review failed login attempts
   - List open ports and firewall status
   - Scan for rootkits

3. **docker-status**: Docker container management
   - List running and stopped containers
   - Show images and disk usage
   - Display networks

4. **analyze-logs**: System log analysis
   - Check system logs for errors
   - Review authentication logs
   - Inspect kernel messages

5. **verify-backups**: Backup verification
   - Check backup directories
   - Review cron jobs
   - Verify disk space

6. **webserver-health**: Web server diagnostics
   - Check nginx/apache status
   - Review error logs
   - Test localhost connections

7. **database-health**: Database status check
   - MySQL, PostgreSQL, MongoDB status
   - Connection counts

#### RDP Workflows

1. **windows-diagnostics**: Windows system diagnostics
   - System information
   - Disk space analysis
   - Running processes and services
   - Network configuration

### ⚡ Quick Actions

#### SSH Quick Actions
- **List Files**: `ls -lah`
- **Disk Usage**: `df -h`
- **System Load**: `top -b -n 1 | head -20`
- **Network Status**: `ip addr show`

#### RDP Quick Actions
- **Task Manager**: Launch Windows Task Manager
- **Command Prompt**: Open CMD
- **PowerShell**: Launch PowerShell

## Architecture

### Components

```
src/libguac/
├── guacamole/copilot.h          # Public API and data structures
├── copilot.c                     # Core implementation
└── copilot-workflows.c           # Built-in workflows and quick actions

src/protocols/rdp/
├── copilot-rdp.c                 # RDP-specific integration
└── copilot-rdp.h                 # RDP copilot interface

src/protocols/ssh/
├── copilot-ssh.c                 # SSH-specific integration
└── copilot-ssh.h                 # SSH copilot interface
```

### Data Structures

**guac_copilot**: Main copilot instance
```c
typedef struct guac_copilot {
    guac_client* client;
    int enabled;
    guac_copilot_context* context;
    guac_copilot_workflow** workflows;
    guac_copilot_quick_action** quick_actions;
    int recording;
    // ...
} guac_copilot;
```

**guac_copilot_context**: Session context for intelligent assistance
```c
typedef struct guac_copilot_context {
    char* protocol;              // rdp, ssh, vnc, etc.
    char* current_directory;
    char* os_type;
    char** command_history;
    char* remote_user;
    int is_privileged;
    // ...
} guac_copilot_context;
```

**guac_copilot_workflow**: Automation workflow/playbook
```c
typedef struct guac_copilot_workflow {
    char name[128];
    char* description;
    char* protocol;              // NULL = all protocols
    guac_copilot_workflow_step* steps;
    int step_count;
    int requires_privileges;
    // ...
} guac_copilot_workflow;
```

## Configuration

### RDP Configuration

Enable Copilot for RDP sessions:

```
enable-copilot=true
```

### SSH Configuration

Enable Copilot for SSH sessions:

```
enable-copilot=true
```

### Server Configuration

Add to `guacamole.properties`:

```properties
# Enable Copilot system-wide
copilot-enabled=true

# Optional: Configure external AI endpoint
copilot-ai-endpoint=https://your-ai-service.com/api
copilot-ai-api-key=your-api-key
```

## Client Integration

### Keyboard Shortcut

The Copilot UI is triggered by the client using a keyboard shortcut (typically **Ctrl+Alt+H** or **Ctrl+Shift+Space**). The client sends a copilot command to the server.

### Protocol Messages

Copilot uses the Guacamole protocol's `argv` instruction to communicate:

```
Server → Client: argv stream "copilot" data
Client → Server: argv stream "copilot-command" data
```

### Message Format

Messages are JSON-formatted:

```json
{
  "type": "suggestions",
  "items": ["ls -la", "cd ~", "pwd"]
}
```

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

## API Usage

### For Protocol Implementations

#### Initialize Copilot

```c
// In RDP client init
guac_rdp_copilot_init(client, rdp_client);

// In SSH client init
guac_ssh_copilot_init(client, ssh_client);
```

#### Track Commands (SSH)

```c
guac_ssh_copilot_track_command(ssh_client, "ls -la");
```

#### Track Output (SSH)

```c
guac_ssh_copilot_track_output(ssh_client, output_buffer, length);
```

#### Update Context

```c
guac_copilot_update_context(copilot, "ssh", "/home/user", "Ubuntu");
```

### For Client Applications

#### Request Suggestions

```c
guac_copilot_handle_command(copilot,
    GUAC_COPILOT_CMD_SUGGEST,
    "ls");  // Partial input
```

#### Execute Workflow

```c
guac_copilot_handle_command(copilot,
    GUAC_COPILOT_CMD_EXECUTE_WORKFLOW,
    "system-diagnostics");
```

#### Start Recording

```c
guac_copilot_start_recording(copilot, "my-workflow");
// User performs actions...
guac_copilot_stop_recording(copilot);
```

#### Get Session Insights

```c
guac_copilot_handle_command(copilot,
    GUAC_COPILOT_CMD_SESSION_INSIGHTS,
    NULL);
```

## Creating Custom Workflows

### Example: Custom Backup Workflow

```c
guac_copilot_workflow* create_my_backup_workflow() {
    guac_copilot_workflow* wf = guac_mem_zalloc(sizeof(guac_copilot_workflow));
    guac_strlcpy(wf->name, "my-backup", GUAC_COPILOT_MAX_WORKFLOW_NAME);
    wf->description = guac_strdup("Backup critical directories");
    wf->protocol = guac_strdup("ssh");
    wf->step_count = 0;
    wf->steps = guac_mem_alloc(5 * sizeof(guac_copilot_workflow_step));

    wf->steps[wf->step_count++] = create_step(
        "Create backup directory",
        "mkdir -p ~/backups/$(date +%Y%m%d)",
        500);

    wf->steps[wf->step_count++] = create_step(
        "Backup home directory",
        "tar czf ~/backups/$(date +%Y%m%d)/home.tar.gz ~",
        5000);

    wf->steps[wf->step_count++] = create_step(
        "List backups",
        "ls -lh ~/backups/$(date +%Y%m%d)/",
        500);

    return wf;
}

// Register it
guac_copilot_register_workflow(copilot, create_my_backup_workflow());
```

## Performance Considerations

- **Command History**: Limited to last 50 commands (configurable via `GUAC_COPILOT_HISTORY_SIZE`)
- **Workflow Limit**: Maximum 10 workflows per copilot instance (expandable)
- **Quick Actions**: Maximum 20 quick actions per instance
- **Memory Usage**: Typical overhead is ~50-100KB per active session

## Security Considerations

1. **Privilege Tracking**: Copilot tracks when users elevate privileges (sudo/su)
2. **Command Logging**: All commands are logged for audit purposes
3. **Workflow Validation**: Workflows can be marked as requiring privileges
4. **Error Tracking**: Errors are captured but sanitized before storage

## Future Enhancements

- [ ] AI-powered command generation using external LLMs
- [ ] Natural language workflow creation
- [ ] Predictive command completion
- [ ] Session anomaly detection
- [ ] Integration with monitoring tools
- [ ] Multi-session correlation
- [ ] Custom scripting language for workflows
- [ ] Workflow marketplace/sharing

## Troubleshooting

### Copilot Not Initializing

Check that:
1. `enable-copilot=true` is set in connection parameters
2. Client properly handles copilot messages
3. Server logs show "Copilot initialized" message

### Workflows Not Executing

- Verify workflow name is correct (case-sensitive)
- Check protocol matches (SSH workflows won't run on RDP)
- Ensure user has required privileges for the workflow

### Commands Not Being Tracked

- SSH: Commands are tracked when sent through the terminal
- Ensure copilot pointer is not NULL
- Check that tracking functions are called

## Contributing

To add new workflows:

1. Edit `src/libguac/copilot-workflows.c`
2. Create workflow using `create_step()` helper
3. Register in `guac_copilot_init_workflows()`

To add protocol support:

1. Create `copilot-{protocol}.c` and `.h`
2. Implement `guac_{protocol}_copilot_init()`
3. Add tracking hooks in protocol code
4. Update protocol client structure

## License

Apache License 2.0 - Same as Guacamole

## Credits

Guacamole Copilot is part of the Apache Guacamole project.

---

**For more information**, visit the [Apache Guacamole Documentation](https://guacamole.apache.org/doc/gug/)
