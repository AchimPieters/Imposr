import { BrowserWindow } from 'electron';
import path from 'node:path';
import { logger } from '@utils/logger';

/** Dependencies used by {@link WindowManager} for easier testing. */
export interface WindowManagerDeps {
  browserWindowFactory: (options: Electron.BrowserWindowConstructorOptions) => BrowserWindow;
}

/** Manages lifecycle of Electron application windows. */
export class WindowManager {
  private mainWindow: BrowserWindow | null = null;

  constructor(private readonly deps: WindowManagerDeps = { browserWindowFactory: (o) => new BrowserWindow(o) }) {}

  /**
   * Create and return the primary app window.
   */
  createMainWindow(): BrowserWindow {
    if (this.mainWindow && !this.mainWindow.isDestroyed()) {
      return this.mainWindow;
    }

    const preloadPath = path.join(__dirname, 'preload.js');
    this.mainWindow = this.deps.browserWindowFactory({
      width: 1400,
      height: 900,
      minWidth: 1024,
      minHeight: 720,
      show: false,
      webPreferences: {
        contextIsolation: true,
        nodeIntegration: false,
        preload: preloadPath,
      },
    });

    this.mainWindow.once('ready-to-show', () => {
      this.mainWindow?.show();
      logger.info('Main window ready');
    });

    this.mainWindow.on('closed', () => {
      logger.info('Main window closed');
      this.mainWindow = null;
    });

    return this.mainWindow;
  }

  /**
   * Read current main window reference.
   */
  getMainWindow(): BrowserWindow | null {
    return this.mainWindow;
  }
}
