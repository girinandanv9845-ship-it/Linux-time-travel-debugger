type MessageHandler = (type: string, payload: any) => void;

export class DebugWebSocket {
  private ws: WebSocket | null = null;
  private sessionId: string;
  private onMessage: MessageHandler;
  private onStatusChange?: (connected: boolean) => void;
  private shouldReconnect = true;
  private reconnectTimer: number | null = null;
  private pingInterval: number | null = null;
  private messageQueue: Array<{ type: string; payload: any }> = [];
  private flushTimer: number | null = null;

  constructor(
    sessionId: string,
    onMessage: MessageHandler,
    onStatusChange?: (connected: boolean) => void
  ) {
    this.sessionId = sessionId;
    this.onMessage = onMessage;
    this.onStatusChange = onStatusChange;
    this.connect();
  }

  private connect() {
    if (!this.sessionId) return;

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    // When proxied by Vite, connect through host
    const wsUrl = `${protocol}//${window.location.host}/ws/debug/${this.sessionId}`;

    try {
      this.ws = new WebSocket(wsUrl);

      this.ws.onopen = () => {
        this.onStatusChange?.(true);
        // Send subscribe action
        this.ws?.send(
          JSON.stringify({
            action: 'subscribe',
            sessionId: this.sessionId,
          })
        );

        // Setup ping interval
        if (this.pingInterval) clearInterval(this.pingInterval);
        this.pingInterval = window.setInterval(() => {
          if (this.ws?.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify({ action: 'ping' }));
          }
        }, 15000);
      };

      this.ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          if (data.type && data.payload !== undefined) {
            this.messageQueue.push({ type: data.type, payload: data.payload });
            if (!this.flushTimer) {
              this.flushTimer = window.setTimeout(() => {
                const batch = this.messageQueue;
                this.messageQueue = [];
                this.flushTimer = null;
                for (const msg of batch) {
                  this.onMessage(msg.type, msg.payload);
                }
              }, 16);
            }
          } else if (data.type === 'subscribed') {
            // Subscription acknowledged
          }
        } catch {
          // Ignore parse errors on raw messages
        }
      };

      this.ws.onclose = () => {
        this.onStatusChange?.(false);
        if (this.pingInterval) clearInterval(this.pingInterval);

        if (this.shouldReconnect) {
          this.reconnectTimer = window.setTimeout(() => {
            this.connect();
          }, 2000);
        }
      };

      this.ws.onerror = () => {
        this.onStatusChange?.(false);
      };
    } catch {
      this.onStatusChange?.(false);
    }
  }

  public disconnect() {
    this.shouldReconnect = false;
    if (this.reconnectTimer) clearTimeout(this.reconnectTimer);
    if (this.pingInterval) clearInterval(this.pingInterval);
    if (this.flushTimer) {
      clearTimeout(this.flushTimer);
      this.flushTimer = null;
    }
    this.messageQueue = [];
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
  }
}

