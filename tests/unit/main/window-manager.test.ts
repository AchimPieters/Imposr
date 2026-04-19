import { WindowManager } from '../../../src/main/window-manager';

type EventName = 'ready-to-show' | 'closed';

interface WindowStub {
  once: jest.Mock;
  on: jest.Mock;
  isDestroyed: jest.Mock<boolean, []>;
  show: jest.Mock<void, []>;
  destroy: () => void;
  trigger: (event: EventName) => void;
  wasShown: () => boolean;
}

function createWindowStub() {
  const onceHandlers: Partial<Record<EventName, () => void>> = {};
  const onHandlers: Partial<Record<EventName, () => void>> = {};
  let destroyed = false;
  let shown = false;

  const stub: WindowStub = {
    once: jest.fn((event: EventName, handler: () => void) => {
      onceHandlers[event] = handler;
      return stub;
    }),
    on: jest.fn((event: EventName, handler: () => void) => {
      onHandlers[event] = handler;
      return stub;
    }),
    isDestroyed: jest.fn(() => destroyed),
    show: jest.fn(() => {
      shown = true;
    }),
    destroy: () => {
      destroyed = true;
    },
    trigger: (event: EventName) => {
      onceHandlers[event]?.();
      onHandlers[event]?.();
    },
    wasShown: () => shown,
  };

  return stub;
}

describe('WindowManager', () => {
  it('creates and reuses main window instance', () => {
    const win = createWindowStub();
    const factory = jest.fn(() => win as unknown as Electron.BrowserWindow);
    const manager = new WindowManager({ browserWindowFactory: factory });

    const first = manager.createMainWindow();
    const second = manager.createMainWindow();

    expect(factory).toHaveBeenCalledTimes(1);
    expect(first).toBe(second);
  });

  it('handles ready and closed events', () => {
    const win = createWindowStub();
    const manager = new WindowManager({
      browserWindowFactory: () => win as unknown as Electron.BrowserWindow,
    });

    manager.createMainWindow();
    win.trigger('ready-to-show');
    expect(win.wasShown()).toBe(true);

    win.trigger('closed');
    expect(manager.getMainWindow()).toBeNull();
  });
});
