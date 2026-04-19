jest.mock('electron-log', () => ({
  transports: { file: { level: 'info' }, console: { level: 'debug' } },
  debug: jest.fn(),
  info: jest.fn(),
  warn: jest.fn(),
  error: jest.fn(),
}));

jest.mock('@sentry/electron', () => ({
  init: jest.fn(),
  captureMessage: jest.fn(),
  captureException: jest.fn(),
}));

import log from 'electron-log';
import * as Sentry from '@sentry/electron';
import { LogLevel, logger } from '../../../src/utils/logger';

describe('logger', () => {
  it('initializes sentry in production bootstrap', () => {
    const originalEnv = process.env.NODE_ENV;
    process.env.NODE_ENV = 'production';
    jest.isolateModules(() => {
      // eslint-disable-next-line @typescript-eslint/no-var-requires
      require('../../../src/utils/logger');
    });
    expect((Sentry.init as jest.Mock).mock.calls.length).toBeGreaterThan(0);
    process.env.NODE_ENV = originalEnv;
  });

  it('logs info and debug at debug level', () => {
    logger.setLevel(LogLevel.DEBUG);
    logger.debug('d');
    logger.info('i');
    expect((log.debug as jest.Mock).mock.calls.length).toBeGreaterThan(0);
    expect((log.info as jest.Mock).mock.calls.length).toBeGreaterThan(0);
  });

  it('reports warnings/errors to sentry in production', () => {
    const originalEnv = process.env.NODE_ENV;
    process.env.NODE_ENV = 'production';
    logger.warn('warning', { x: 1 });
    logger.error('error', new Error('x'), { y: 2 });
    expect((Sentry.captureMessage as jest.Mock).mock.calls.length).toBeGreaterThan(0);
    expect((Sentry.captureException as jest.Mock).mock.calls.length).toBeGreaterThan(0);
    process.env.NODE_ENV = originalEnv;
  });

  it('does not log debug when level is INFO', () => {
    const before = (log.debug as jest.Mock).mock.calls.length;
    logger.setLevel(LogLevel.INFO);
    logger.debug('hidden');
    expect((log.debug as jest.Mock).mock.calls.length).toBe(before);
  });

  it('does not send sentry error without error object', () => {
    const before = (Sentry.captureException as jest.Mock).mock.calls.length;
    logger.error('error-without-object');
    expect((Sentry.captureException as jest.Mock).mock.calls.length).toBe(before);
  });
});
