import { dialog, ipcMain } from 'electron';
import { WindowManager } from './window-manager';

/** Register app IPC handlers. */
export function registerIpcHandlers(windowManager: WindowManager): void {
  ipcMain.handle('app:open-pdf', async () => {
    const targetWindow = windowManager.getMainWindow();
    const options: Electron.OpenDialogOptions = {
      title: 'Open PDF',
      properties: ['openFile'],
      filters: [{ name: 'PDF Files', extensions: ['pdf'] }],
    };
    const result = targetWindow
      ? await dialog.showOpenDialog(targetWindow, options)
      : await dialog.showOpenDialog(options);

    if (result.canceled || result.filePaths.length === 0) {
      return null;
    }

    return result.filePaths[0];
  });
}
