/** Storage contract for processed webhook ids (idempotency/replay protection). */
export interface WebhookReplayStore {
  has(eventId: string): Promise<boolean>;
  mark(eventId: string): Promise<void>;
}

/** In-memory implementation for test/dev usage. */
export class InMemoryWebhookReplayStore implements WebhookReplayStore {
  private readonly processed = new Set<string>();

  async has(eventId: string): Promise<boolean> {
    return this.processed.has(eventId);
  }

  async mark(eventId: string): Promise<void> {
    this.processed.add(eventId);
  }
}
