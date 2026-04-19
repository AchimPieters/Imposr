/** CLI-safe logger utility. */
export class CliLogger {
  /** Write informational message. */
  info(message: string): void {
    // eslint-disable-next-line no-console
    console.log(message);
  }

  /** Write warning message. */
  warn(message: string): void {
    // eslint-disable-next-line no-console
    console.warn(message);
  }

  /** Write error message. */
  error(message: string): void {
    // eslint-disable-next-line no-console
    console.error(message);
  }
}

/** Shared CLI logger instance. */
export const cliLogger = new CliLogger();
