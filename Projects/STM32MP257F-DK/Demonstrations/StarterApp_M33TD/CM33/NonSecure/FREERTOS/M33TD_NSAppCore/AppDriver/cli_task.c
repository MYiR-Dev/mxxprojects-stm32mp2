/**
  ******************************************************************************
  * @file    cli_task.c
  * @brief   UART5 command-line interface task implementation.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "cli_task.h"
#include "main.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* External (BSP impl, not in headers) --------------------------------------- */
extern int __io_getchar(void);

/* Task Configuration -------------------------------------------------------- */
#ifndef CLI_TASK_STACK_SIZE
#define CLI_TASK_STACK_SIZE     1024
#endif
#ifndef CLI_TASK_PRIORITY
#define CLI_TASK_PRIORITY       osPriorityLow
#endif

/* Types ---------------------------------------------------------------------*/
typedef struct {
    const char       *name;
    CLI_CmdHandler_t  handler;
    const char       *help;
} CLI_CmdEntry_t;

struct CLI_Context {
    osThreadId_t  task_handle;
    char          line_buf[CLI_MAX_CMD_LEN + 1];
    int           line_pos;
    CLI_CmdEntry_t handlers[CLI_MAX_HANDLERS];
    int           handler_count;
    bool          initialized;
};

/* Statics -------------------------------------------------------------------*/
static CLI_Context_t  cli_ctx;
static const osThreadAttr_t CLI_TaskAttr = {
    .name       = "CLI_Task",
    .priority   = CLI_TASK_PRIORITY,
    .stack_size = CLI_TASK_STACK_SIZE
};

/* Forward declarations ------------------------------------------------------*/
static void CLI_TaskFunc(void *argument);
static void CLI_ProcessLine(char *line);
static void CLI_PrintPrompt(void);
static int  CLI_Tokenize(char *line, char *argv[], int max_args);
static int  CLI_CmdHelp(int argc, char *argv[]);
static int  CLI_CmdStatus(int argc, char *argv[]);
static int  CLI_CmdVersion(int argc, char *argv[]);

/* Public API ----------------------------------------------------------------*/

void CLI_Task_Init(void)
{
    memset(&cli_ctx, 0, sizeof(cli_ctx));

    CLI_RegisterCommand("help",    CLI_CmdHelp,    "Show available commands");
    CLI_RegisterCommand("status",  CLI_CmdStatus,  "Show system status");
    CLI_RegisterCommand("version", CLI_CmdVersion, "Show firmware version");

    cli_ctx.task_handle = osThreadNew(CLI_TaskFunc, NULL, &CLI_TaskAttr);
    if (cli_ctx.task_handle == NULL) return;

    cli_ctx.initialized = true;
}

bool CLI_RegisterCommand(const char *name, CLI_CmdHandler_t handler, const char *help)
{
    if (!name || !handler || !help) return false;
    if (cli_ctx.handler_count >= CLI_MAX_HANDLERS) return false;

    for (int i = 0; i < cli_ctx.handler_count; i++) {
        if (strcmp(cli_ctx.handlers[i].name, name) == 0) return false;
    }

    CLI_CmdEntry_t *e = &cli_ctx.handlers[cli_ctx.handler_count++];
    e->name    = name;
    e->handler = handler;
    e->help    = help;
    return true;
}

void CLI_Printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

/* Task loop -----------------------------------------------------------------*/

static void CLI_TaskFunc(void *argument)
{
    (void)argument;
    osDelay(500);

    CLI_Printf("\r\n");
    CLI_Printf("========================================\r\n");
    CLI_Printf("  M33 CLI - Type 'help' for commands\r\n");
    CLI_Printf("========================================\r\n");
    CLI_PrintPrompt();

    for (;;) {
        int ch = __io_getchar();

        /* Backspace: DEL=0x7F, BS=0x08 */
        if (ch == 0x7F || ch == 0x08) {
            if (cli_ctx.line_pos > 0) {
                cli_ctx.line_pos--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }

        /* Enter: CR or LF */
        if (ch == '\r' || ch == '\n') {
            printf("\r\n");
            fflush(stdout);
            cli_ctx.line_buf[cli_ctx.line_pos] = '\0';
            if (cli_ctx.line_pos > 0) {
                CLI_ProcessLine(cli_ctx.line_buf);
            }
            cli_ctx.line_pos = 0;
            CLI_PrintPrompt();
            continue;
        }

        /* Skip other control chars (except TAB) */
        if (ch < 0x20 && ch != '\t') continue;

        /* Buffer + echo */
        if (cli_ctx.line_pos < CLI_MAX_CMD_LEN) {
            cli_ctx.line_buf[cli_ctx.line_pos++] = (char)ch;
            printf("%c", ch);
            fflush(stdout);
        }
    }
}

/* Manual tokenizer ---------------------------------------------------------*/

/**
  * @brief  Tokenize a line into argv, replacing strtok() to avoid newlib
  *         reentrancy malloc assertion on embedded FreeRTOS.
  * @param  line     Null-terminated input string (modified in-place).
  * @param  argv     Output argument vector.
  * @param  max_args Max number of arguments.
  * @retval Number of tokens found.
  */
static int CLI_Tokenize(char *line, char *argv[], int max_args)
{
    int   argc = 0;
    char *p    = line;

    while (*p && argc < max_args) {
        /* Skip leading whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        argv[argc++] = p;

        /* Scan to end of token */
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) {
            *p = '\0';
            p++;
        }
    }
    return argc;
}

/* Command dispatch ----------------------------------------------------------*/

static void CLI_ProcessLine(char *line)
{
    char *argv[CLI_MAX_ARGS];
    int   argc;

    argc = CLI_Tokenize(line, argv, CLI_MAX_ARGS);
    if (argc == 0) return;

    for (int i = 0; i < cli_ctx.handler_count; i++) {
        if (strcmp(argv[0], cli_ctx.handlers[i].name) == 0) {
            int ret = cli_ctx.handlers[i].handler(argc, argv);
            if (ret != 0) {
                CLI_Printf("Command error: %d\r\n", ret);
            }
            return;
        }
    }

    CLI_Printf("Unknown: '%s'. Type 'help'.\r\n", argv[0]);
}

static void CLI_PrintPrompt(void)
{
    printf(CLI_PROMPT);
    fflush(stdout);
}

/* Built-in commands ---------------------------------------------------------*/

static int CLI_CmdHelp(int argc, char *argv[])
{
    (void)argc; (void)argv;
    CLI_Printf("\r\nCommands:\r\n");
    for (int i = 0; i < cli_ctx.handler_count; i++) {
        CLI_Printf("  %-12s - %s\r\n",
                   cli_ctx.handlers[i].name,
                   cli_ctx.handlers[i].help);
    }
    CLI_Printf("\r\n");
    return 0;
}

static int CLI_CmdStatus(int argc, char *argv[])
{
    (void)argc; (void)argv;

    CLI_Printf("\r\n=== M33 Status ===\r\n");
    CLI_Printf("Tick       : %lu ms\r\n", (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS));
    CLI_Printf("Tasks      : %lu\r\n", (unsigned long)uxTaskGetNumberOfTasks());
    CLI_Printf("Free heap  : %lu\r\n", (unsigned long)xPortGetFreeHeapSize());
    CLI_Printf("Min heap   : %lu\r\n", (unsigned long)xPortGetMinimumEverFreeHeapSize());
    CLI_Printf("\r\n");
    return 0;
}

static int CLI_CmdVersion(int argc, char *argv[])
{
    (void)argc; (void)argv;
    uint32_t v = HAL_GetHalVersion();
    CLI_Printf("\r\nSTM32Cube FW v%lu.%lu.%lu-rc%lu\r\n",
               (unsigned long)((v>>24)&0xFF), (unsigned long)((v>>16)&0xFF),
               (unsigned long)((v>>8)&0xFF),  (unsigned long)(v&0xFF));
    CLI_Printf("Build: M33 minimal boot\r\n\r\n");
    return 0;
}
