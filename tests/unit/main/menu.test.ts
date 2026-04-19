import { buildMenuTemplate } from '../../../src/main/menu';

describe('main/menu', () => {
  it('builds expected top-level menus', () => {
    const template = buildMenuTemplate({
      onOpenFile: jest.fn(),
      onRunImposition: jest.fn(),
    });

    expect(template.map((item) => item.label)).toEqual(['File', 'Imposition', 'View']);
  });

  it('wires menu actions', () => {
    const onOpenFile = jest.fn();
    const onRunImposition = jest.fn();
    const template = buildMenuTemplate({ onOpenFile, onRunImposition });

    const fileMenu = template[0].submenu as Electron.MenuItemConstructorOptions[];
    const imposeMenu = template[1].submenu as Electron.MenuItemConstructorOptions[];

    fileMenu[0].click?.(undefined as never, undefined as never, undefined as never);
    imposeMenu[0].click?.(undefined as never, undefined as never, undefined as never);

    expect(onOpenFile).toHaveBeenCalledTimes(1);
    expect(onRunImposition).toHaveBeenCalledTimes(1);
  });
});
