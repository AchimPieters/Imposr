import { app } from 'electron';
import { logger } from '@utils/logger';
import { registerAppMenu } from './menu';
import { registerIpcHandlers } from './ipc-handlers';
import { WindowManager } from './window-manager';

const windowManager = new WindowManager();

async function bootstrap(): Promise<void> {
  await app.whenReady();

  const mainWindow = windowManager.createMainWindow();
  registerIpcHandlers(windowManager);
  registerAppMenu({
    onOpenFile: () => {
      mainWindow.webContents.send('menu:open-file');
    },
    onRunImposition: () => {
      mainWindow.webContents.send('menu:run-imposition');
    },
  });

  if (process.env.ELECTRON_START_URL) {
    await mainWindow.loadURL(process.env.ELECTRON_START_URL);
  } else {
    await mainWindow.loadFile('dist/renderer/index.html');
  }

  app.on('activate', () => {
    if (windowManager.getMainWindow() === null) {
      windowManager.createMainWindow();
    }
  });
}

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

bootstrap().catch((error) => {
  logger.error('Failed to bootstrap Electron app', error instanceof Error ? error : undefined);
  app.exit(1);
});
