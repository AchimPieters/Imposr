import log from 'electron-log';
import * as Sentry from '@sentry/electron';

/** Supported log levels. */
export enum LogLevel {
  DEBUG = 'debug',
  INFO = 'info',
  WARN = 'warn',
  ERROR = 'error',
}

/** Metadata object for structured logs. */
export type LogMeta = Record<string, unknown>;

/**
 * Singleton logger wrapper around electron-log and optional Sentry reporting.
 */
class Logger {
  private static instance: Logger;
  private level: LogLevel = LogLevel.INFO;

  private constructor() {
    log.transports.file.level = 'info';
    log.transports.console.level = 'debug';

    if (process.env.NODE_ENV === 'production') {
      Sentry.init({
        dsn: process.env.SENTRY_DSN,
        environment: process.env.NODE_ENV,
        release: `imposr@${process.env.npm_package_version ?? 'unknown'}`,
      });
    }
  }

  /** Returns singleton logger instance. */
  static getInstance(): Logger {
    if (!Logger.instance) {
      Logger.instance = new Logger();
    }
    return Logger.instance;
  }

  /** Sets minimum log level. */
  setLevel(level: LogLevel): void {
    this.level = level;
  }

  /** Writes debug log. */
  debug(message: string, meta?: LogMeta): void {
    if (this.shouldLog(LogLevel.DEBUG)) {
      log.debug(message, meta);
    }
  }

  /** Writes info log. */
  info(message: string, meta?: LogMeta): void {
    if (this.shouldLog(LogLevel.INFO)) {
      log.info(message, meta);
    }
  }

  /** Writes warning log and optionally reports to Sentry in production. */
  warn(message: string, meta?: LogMeta): void {
    if (!this.shouldLog(LogLevel.WARN)) {
      return;
    }

    log.warn(message, meta);

    if (process.env.NODE_ENV === 'production') {
      Sentry.captureMessage(message, {
        level: 'warning',
        extra: meta,
      });
    }
  }

  /** Writes error log and optionally reports exception to Sentry in production. */
  error(message: string, error?: Error, meta?: LogMeta): void {
    if (!this.shouldLog(LogLevel.ERROR)) {
      return;
    }

    log.error(message, error, meta);

    if (process.env.NODE_ENV === 'production' && error) {
      Sentry.captureException(error, {
        extra: { message, ...meta },
      });
    }
  }

  private shouldLog(level: LogLevel): boolean {
    const levels = [LogLevel.DEBUG, LogLevel.INFO, LogLevel.WARN, LogLevel.ERROR];
    return levels.indexOf(level) >= levels.indexOf(this.level);
  }
}

/** Shared logger singleton. */
export const logger = Logger.getInstance();
