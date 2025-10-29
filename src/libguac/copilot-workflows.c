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
#include "guacamole/copilot.h"
#include "guacamole/mem.h"
#include "guacamole/string.h"

#include <string.h>

/**
 * Helper to create a workflow step.
 */
static guac_copilot_workflow_step create_step(const char* description,
        const char* command, int wait_time) {

    guac_copilot_workflow_step step = {
        .description = guac_strdup(description),
        .command = guac_strdup(command),
        .expected_output = NULL,
        .wait_time = wait_time,
        .continue_on_error = 0
    };

    return step;

}

/**
 * Creates a system diagnostics workflow for SSH sessions.
 */
static guac_copilot_workflow* create_ssh_diagnostics_workflow() {

    guac_copilot_workflow* wf = guac_mem_zalloc(sizeof(guac_copilot_workflow));
    guac_strlcpy(wf->name, "system-diagnostics", GUAC_COPILOT_MAX_WORKFLOW_NAME);
    wf->description = guac_strdup("Run comprehensive system diagnostics");
    wf->protocol = guac_strdup("ssh");
    wf->step_count = 0;
    wf->requires_privileges = 0;
    wf->steps = guac_mem_alloc(10 * sizeof(guac_copilot_workflow_step));

    wf->steps[wf->step_count++] = create_step("Check disk usage", "df -h", 1000);
    wf->steps[wf->step_count++] = create_step("Check memory usage", "free -h", 1000);
    wf->steps[wf->step_count++] = create_step("Check CPU info", "lscpu", 1000);
    wf->steps[wf->step_count++] = create_step("Check running processes", "ps aux --sort=-%mem | head -10", 1000);
    wf->steps[wf->step_count++] = create_step("Check network connections", "netstat -tuln", 1000);
    wf->steps[wf->step_count++] = create_step("Check system uptime", "uptime", 500);

    return wf;

}

/**
 * Creates a quick security scan workflow for SSH.
 */
static guac_copilot_workflow* create_ssh_security_scan() {

    guac_copilot_workflow* wf = guac_mem_zalloc(sizeof(guac_copilot_workflow));
    guac_strlcpy(wf->name, "security-scan", GUAC_COPILOT_MAX_WORKFLOW_NAME);
    wf->description = guac_strdup("Quick security check");
    wf->protocol = guac_strdup("ssh");
    wf->step_count = 0;
    wf->requires_privileges = 1;
    wf->steps = guac_mem_alloc(10 * sizeof(guac_copilot_workflow_step));

    wf->steps[wf->step_count++] = create_step("Check for updates", "apt list --upgradable 2>/dev/null || yum list updates 2>/dev/null", 2000);
    wf->steps[wf->step_count++] = create_step("Check failed login attempts", "grep 'Failed password' /var/log/auth.log 2>/dev/null | tail -20", 1000);
    wf->steps[wf->step_count++] = create_step("Check open ports", "ss -tuln", 1000);
    wf->steps[wf->step_count++] = create_step("Check firewall status", "ufw status 2>/dev/null || firewall-cmd --state 2>/dev/null", 1000);
    wf->steps[wf->step_count++] = create_step("Check for rootkits", "which rkhunter && rkhunter --check --skip-keypress 2>/dev/null | tail -20", 3000);

    return wf;

}

/**
 * Creates a Docker management workflow.
 */
static guac_copilot_workflow* create_docker_management() {

    guac_copilot_workflow* wf = guac_mem_zalloc(sizeof(guac_copilot_workflow));
    guac_strlcpy(wf->name, "docker-status", GUAC_COPILOT_MAX_WORKFLOW_NAME);
    wf->description = guac_strdup("Check Docker containers and images");
    wf->protocol = guac_strdup("ssh");
    wf->step_count = 0;
    wf->requires_privileges = 0;
    wf->steps = guac_mem_alloc(8 * sizeof(guac_copilot_workflow_step));

    wf->steps[wf->step_count++] = create_step("List running containers", "docker ps", 1000);
    wf->steps[wf->step_count++] = create_step("List all containers", "docker ps -a", 1000);
    wf->steps[wf->step_count++] = create_step("Show images", "docker images", 1000);
    wf->steps[wf->step_count++] = create_step("Show disk usage", "docker system df", 1000);
    wf->steps[wf->step_count++] = create_step("Show networks", "docker network ls", 1000);

    return wf;

}

/**
 * Creates a log analysis workflow.
 */
static guac_copilot_workflow* create_log_analysis() {

    guac_copilot_workflow* wf = guac_mem_zalloc(sizeof(guac_copilot_workflow));
    guac_strlcpy(wf->name, "analyze-logs", GUAC_COPILOT_MAX_WORKFLOW_NAME);
    wf->description = guac_strdup("Analyze system logs for errors");
    wf->protocol = guac_strdup("ssh");
    wf->step_count = 0;
    wf->requires_privileges = 1;
    wf->steps = guac_mem_alloc(8 * sizeof(guac_copilot_workflow_step));

    wf->steps[wf->step_count++] = create_step("Check system log errors", "journalctl -p err -n 20 --no-pager 2>/dev/null || tail -50 /var/log/syslog | grep -i error", 2000);
    wf->steps[wf->step_count++] = create_step("Check authentication logs", "tail -50 /var/log/auth.log 2>/dev/null || tail -50 /var/log/secure", 1000);
    wf->steps[wf->step_count++] = create_step("Check kernel messages", "dmesg | tail -30", 1000);
    wf->steps[wf->step_count++] = create_step("Check application errors", "journalctl -p warning -n 20 --no-pager 2>/dev/null", 2000);

    return wf;

}

/**
 * Creates a backup verification workflow.
 */
static guac_copilot_workflow* create_backup_verification() {

    guac_copilot_workflow* wf = guac_mem_zalloc(sizeof(guac_copilot_workflow));
    guac_strlcpy(wf->name, "verify-backups", GUAC_COPILOT_MAX_WORKFLOW_NAME);
    wf->description = guac_strdup("Check backup status and schedule");
    wf->protocol = guac_strdup("ssh");
    wf->step_count = 0;
    wf->requires_privileges = 0;
    wf->steps = guac_mem_alloc(6 * sizeof(guac_copilot_workflow_step));

    wf->steps[wf->step_count++] = create_step("Check backup directory", "ls -lh /backup 2>/dev/null || ls -lh ~/backup 2>/dev/null", 1000);
    wf->steps[wf->step_count++] = create_step("Check cron jobs", "crontab -l 2>/dev/null | grep -i backup", 500);
    wf->steps[wf->step_count++] = create_step("Check disk space", "df -h /backup 2>/dev/null", 500);

    return wf;

}

/**
 * Creates a web server health check workflow.
 */
static guac_copilot_workflow* create_webserver_healthcheck() {

    guac_copilot_workflow* wf = guac_mem_zalloc(sizeof(guac_copilot_workflow));
    guac_strlcpy(wf->name, "webserver-health", GUAC_COPILOT_MAX_WORKFLOW_NAME);
    wf->description = guac_strdup("Check web server status and logs");
    wf->protocol = guac_strdup("ssh");
    wf->step_count = 0;
    wf->requires_privileges = 1;
    wf->steps = guac_mem_alloc(8 * sizeof(guac_copilot_workflow_step));

    wf->steps[wf->step_count++] = create_step("Check nginx status", "systemctl status nginx 2>/dev/null | head -15", 1000);
    wf->steps[wf->step_count++] = create_step("Check apache status", "systemctl status apache2 2>/dev/null || systemctl status httpd 2>/dev/null | head -15", 1000);
    wf->steps[wf->step_count++] = create_step("Check error log", "tail -20 /var/log/nginx/error.log 2>/dev/null || tail -20 /var/log/apache2/error.log 2>/dev/null", 1000);
    wf->steps[wf->step_count++] = create_step("Check active connections", "ss -tan | grep :80 | wc -l", 500);
    wf->steps[wf->step_count++] = create_step("Test localhost", "curl -I http://localhost 2>&1", 1000);

    return wf;

}

/**
 * Creates a database health check workflow.
 */
static guac_copilot_workflow* create_database_healthcheck() {

    guac_copilot_workflow* wf = guac_mem_zalloc(sizeof(guac_copilot_workflow));
    guac_strlcpy(wf->name, "database-health", GUAC_COPILOT_MAX_WORKFLOW_NAME);
    wf->description = guac_strdup("Check database status");
    wf->protocol = guac_strdup("ssh");
    wf->step_count = 0;
    wf->requires_privileges = 1;
    wf->steps = guac_mem_alloc(6 * sizeof(guac_copilot_workflow_step));

    wf->steps[wf->step_count++] = create_step("Check MySQL status", "systemctl status mysql 2>/dev/null | head -15", 1000);
    wf->steps[wf->step_count++] = create_step("Check PostgreSQL status", "systemctl status postgresql 2>/dev/null | head -15", 1000);
    wf->steps[wf->step_count++] = create_step("Check MongoDB status", "systemctl status mongod 2>/dev/null | head -15", 1000);
    wf->steps[wf->step_count++] = create_step("Check database connections", "ss -tan | grep :3306 | wc -l && ss -tan | grep :5432 | wc -l", 500);

    return wf;

}

/**
 * Creates a Windows diagnostics workflow for RDP.
 */
static guac_copilot_workflow* create_rdp_diagnostics() {

    guac_copilot_workflow* wf = guac_mem_zalloc(sizeof(guac_copilot_workflow));
    guac_strlcpy(wf->name, "windows-diagnostics", GUAC_COPILOT_MAX_WORKFLOW_NAME);
    wf->description = guac_strdup("Run Windows system diagnostics");
    wf->protocol = guac_strdup("rdp");
    wf->step_count = 0;
    wf->requires_privileges = 0;
    wf->steps = guac_mem_alloc(8 * sizeof(guac_copilot_workflow_step));

    wf->steps[wf->step_count++] = create_step("Check system info", "systeminfo", 2000);
    wf->steps[wf->step_count++] = create_step("Check disk space", "wmic logicaldisk get name,size,freespace", 1000);
    wf->steps[wf->step_count++] = create_step("Check running processes", "tasklist /V | findstr /i \"exe\"", 1000);
    wf->steps[wf->step_count++] = create_step("Check services", "sc query state= all | findstr /i \"running\"", 1000);
    wf->steps[wf->step_count++] = create_step("Check network", "ipconfig /all", 1000);

    return wf;

}

/**
 * Initializes all built-in workflows.
 */
void guac_copilot_init_workflows(guac_copilot* copilot) {

    if (copilot == NULL)
        return;

    /* Register SSH workflows */
    guac_copilot_register_workflow(copilot, create_ssh_diagnostics_workflow());
    guac_copilot_register_workflow(copilot, create_ssh_security_scan());
    guac_copilot_register_workflow(copilot, create_docker_management());
    guac_copilot_register_workflow(copilot, create_log_analysis());
    guac_copilot_register_workflow(copilot, create_backup_verification());
    guac_copilot_register_workflow(copilot, create_webserver_healthcheck());
    guac_copilot_register_workflow(copilot, create_database_healthcheck());

    /* Register RDP workflows */
    guac_copilot_register_workflow(copilot, create_rdp_diagnostics());

}

/**
 * Initializes quick actions for common tasks.
 */
void guac_copilot_init_quick_actions(guac_copilot* copilot) {

    if (copilot == NULL)
        return;

    /* SSH quick actions */
    guac_copilot_quick_action* action1 = guac_mem_zalloc(sizeof(guac_copilot_quick_action));
    action1->name = guac_strdup("list-files");
    action1->label = guac_strdup("List Files");
    action1->icon = guac_strdup("folder");
    action1->command = guac_strdup("ls -lah");
    action1->protocol = guac_strdup("ssh");
    guac_copilot_register_quick_action(copilot, action1);

    guac_copilot_quick_action* action2 = guac_mem_zalloc(sizeof(guac_copilot_quick_action));
    action2->name = guac_strdup("disk-usage");
    action2->label = guac_strdup("Disk Usage");
    action2->icon = guac_strdup("disk");
    action2->command = guac_strdup("df -h");
    action2->protocol = guac_strdup("ssh");
    guac_copilot_register_quick_action(copilot, action2);

    guac_copilot_quick_action* action3 = guac_mem_zalloc(sizeof(guac_copilot_quick_action));
    action3->name = guac_strdup("system-load");
    action3->label = guac_strdup("System Load");
    action3->icon = guac_strdup("cpu");
    action3->command = guac_strdup("top -b -n 1 | head -20");
    action3->protocol = guac_strdup("ssh");
    guac_copilot_register_quick_action(copilot, action3);

    guac_copilot_quick_action* action4 = guac_mem_zalloc(sizeof(guac_copilot_quick_action));
    action4->name = guac_strdup("network-status");
    action4->label = guac_strdup("Network Status");
    action4->icon = guac_strdup("network");
    action4->command = guac_strdup("ip addr show");
    action4->protocol = guac_strdup("ssh");
    guac_copilot_register_quick_action(copilot, action4);

    /* RDP quick actions */
    guac_copilot_quick_action* rdp_action1 = guac_mem_zalloc(sizeof(guac_copilot_quick_action));
    rdp_action1->name = guac_strdup("task-manager");
    rdp_action1->label = guac_strdup("Task Manager");
    rdp_action1->icon = guac_strdup("tasks");
    rdp_action1->command = guac_strdup("taskmgr");
    rdp_action1->protocol = guac_strdup("rdp");
    guac_copilot_register_quick_action(copilot, rdp_action1);

    guac_copilot_quick_action* rdp_action2 = guac_mem_zalloc(sizeof(guac_copilot_quick_action));
    rdp_action2->name = guac_strdup("cmd");
    rdp_action2->label = guac_strdup("Command Prompt");
    rdp_action2->icon = guac_strdup("terminal");
    rdp_action2->command = guac_strdup("cmd");
    rdp_action2->protocol = guac_strdup("rdp");
    guac_copilot_register_quick_action(copilot, rdp_action2);

    guac_copilot_quick_action* rdp_action3 = guac_mem_zalloc(sizeof(guac_copilot_quick_action));
    rdp_action3->name = guac_strdup("powershell");
    rdp_action3->label = guac_strdup("PowerShell");
    rdp_action3->icon = guac_strdup("shell");
    rdp_action3->command = guac_strdup("powershell");
    rdp_action3->protocol = guac_strdup("rdp");
    guac_copilot_register_quick_action(copilot, rdp_action3);

}
