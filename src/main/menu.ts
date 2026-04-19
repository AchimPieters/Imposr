import { Menu, MenuItemConstructorOptions, app } from 'electron';

/** Action callbacks used by menu commands. */
export interface MenuActions {
  onOpenFile: () => void;
  onRunImposition: () => void;
}

/**
 * Build menu template for the desktop app.
 */
export function buildMenuTemplate(actions: MenuActions): MenuItemConstructorOptions[] {
  return [
    {
      label: 'File',
      submenu: [
        { label: 'Open PDF…', accelerator: 'CmdOrCtrl+O', click: actions.onOpenFile },
        { type: 'separator' },
        { role: 'quit' },
      ],
    },
    {
      label: 'Imposition',
      submenu: [{ label: 'Run Imposition', accelerator: 'CmdOrCtrl+R', click: actions.onRunImposition }],
    },
    {
      label: 'View',
      submenu: [{ role: 'reload' }, { role: 'toggleDevTools' }, { type: 'separator' }, { role: 'togglefullscreen' }],
    },
  ];
}

/**
 * Register application menu.
 */
export function registerAppMenu(actions: MenuActions): void {
  const menu = Menu.buildFromTemplate(buildMenuTemplate(actions));
  Menu.setApplicationMenu(menu);
  if (process.platform === 'darwin') {
    app.dock?.setMenu(menu);
  }
}
