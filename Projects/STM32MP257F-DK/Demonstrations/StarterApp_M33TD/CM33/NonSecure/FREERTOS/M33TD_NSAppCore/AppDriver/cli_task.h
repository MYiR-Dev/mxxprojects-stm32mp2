/**
  ******************************************************************************
  * @file    cli_task.h
  * @brief   UART5 command-line interface task for M33 minimal boot mode.
  *
  * @details Provides an interactive CLI over UART5 (same port as logger output).
  *          Reads characters via __io_getchar(), echoes them back, buffers
  *          input lines, and dispatches parsed commands to registered handlers.
  *
  *          Built-in commands: help, status, version
  ******************************************************************************
  */

#ifndef CLI_TASK_H
#define CLI_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Exported constants -------------------------------------------------------- */
#define CLI_MAX_CMD_LEN      64    /**< Max command line length (bytes).    */
#define CLI_MAX_ARGS         8     /**< Max arguments per command.          */
#define CLI_MAX_HANDLERS     16    /**< Max registered command handlers.    */
#define CLI_PROMPT           "M33> " /**< CLI prompt string.                */

/* Exported types ------------------------------------------------------------ */

/** Opaque CLI context (internal state, do not access directly). */
typedef struct CLI_Context CLI_Context_t;

/**
  * @brief Command handler function signature.
  * @param argc  Argument count.
  * @param argv  Argument vector (argv[0] is the command name).
  * @retval 0 on success, non-zero on error.
  */
typedef int (*CLI_CmdHandler_t)(int argc, char *argv[]);

/* Exported functions -------------------------------------------------------- */

/**
  * @brief  Initialize the CLI task and register built-in commands.
  * @note   Must be called after LoggerTask_Init() so UART is ready.
  * @retval None
  */
void CLI_Task_Init(void);

/**
  * @brief  Register a custom command handler.
  * @param  name    Command name (case-sensitive, no spaces).
  * @param  handler Callback function.
  * @param  help    One-line help string (shown by 'help' command).
  * @retval true on success, false if handler table is full.
  */
bool CLI_RegisterCommand(const char *name, CLI_CmdHandler_t handler, const char *help);

/**
  * @brief  Print a formatted line to the CLI output (UART5).
  * @param  fmt  printf-style format string.
  * @param  ...  Variable arguments.
  * @retval None
  */
void CLI_Printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* CLI_TASK_H */
