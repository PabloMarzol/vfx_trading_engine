/**
 * VFX Trading Platform - WebSocket Connection Manager
 * Handles real-time communication with trading engine
 */

class VFXWebSocket {

    constructor(url = 'ws://localhost:8080') {
        this.url = url;
        this.socket = null;
        this.isConnected = false;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.reconnectDelay = 2000;
        this.pingInterval = null;
        this.callbacks = new Map();
        
        // Event handlers
        this.onConnect = null;
        this.onDisconnect = null;
        this.onError = null;
        this.onMessage = null;
        
        console.log('🔌 VFX WebSocket Manager initialized');
    }
    
    /**
     * Connect to WebSocket server
     */
    connect() {
        try {
            console.log(`🔄 Connecting to ${this.url}...`);
            
            this.socket = new WebSocket(this.url);
            
            this.socket.onopen = (event) => {
                this.isConnected = true;
                this.reconnectAttempts = 0;
                console.log('✅ WebSocket connected successfully');
                
                // Start ping/pong for connection health
                this.startPingPong();
                
                // Trigger connect callback
                if (this.onConnect) {
                    this.onConnect(event);
                }
                
                // Update UI connection status
                this.updateConnectionUI(true);
            };
            
            this.socket.onclose = (event) => {
                this.isConnected = false;
                console.log('❌ WebSocket connection closed', event.code, event.reason);
                
                // Stop ping/pong
                this.stopPingPong();
                
                // Trigger disconnect callback
                if (this.onDisconnect) {
                    this.onDisconnect(event);
                }
                
                // Update UI connection status
                this.updateConnectionUI(false);
                
                // Attempt reconnection if not intentional close
                if (event.code !== 1000 && this.reconnectAttempts < this.maxReconnectAttempts) {
                    this.attemptReconnect();
                }
            };
            
            this.socket.onerror = (error) => {
                console.error('🚨 WebSocket error:', error);
                
                if (this.onError) {
                    this.onError(error);
                }
            };
            
            this.socket.onmessage = (event) => {
                this.handleMessage(event.data);
            };
            
        } catch (error) {
            console.error('Failed to create WebSocket connection:', error);
            this.updateConnectionUI(false);
        }
    }
    
    /**
     * Disconnect from WebSocket server
     */
    disconnect() {
        if (this.socket) {
            this.socket.close(1000, 'Client disconnect');
            this.socket = null;
        }
        this.stopPingPong();
    }
    
    /**
     * Send message to server
     */
    send(data) {
        if (!this.isConnected || !this.socket) {
            console.warn('⚠️ Cannot send message - not connected');
            return false;
        }
        
        try {
            const message = typeof data === 'string' ? data : JSON.stringify(data);
            this.socket.send(message);
            console.log('📤 Sent:', data);
            return true;
        } catch (error) {
            console.error('Failed to send message:', error);
            return false;
        }
    }
    
    /**
     * Handle incoming messages
     */
    handleMessage(data) {
        try {
            const message = JSON.parse(data);
            console.log('📥 Received:', message);
            
            // Handle specific message types
            switch (message.type) {
                case 'pong':
                    this.handlePong(message);
                    break;
                    
                case 'market_data':
                    this.triggerCallback('market_data', message);
                    break;
                    
                case 'execution':
                    this.triggerCallback('execution', message);
                    break;
                    
                case 'order_confirmation':
                    this.triggerCallback('order_confirmation', message);
                    break;
                    
                case 'strategy_signal':
                    this.triggerCallback('strategy_signal', message);
                    break;
                    
                case 'error':
                    console.error('Server error:', message.message);
                    this.triggerCallback('error', message);
                    break;
                    
                default:
                    console.log('Unknown message type:', message.type);
            }
            
            // Trigger general message callback
            if (this.onMessage) {
                this.onMessage(message);
            }
            
        } catch (error) {
            console.error('Failed to parse message:', error);
        }
    }
    
    /**
     * Register callback for specific message type
     */
    on(eventType, callback) {
        if (!this.callbacks.has(eventType)) {
            this.callbacks.set(eventType, []);
        }
        this.callbacks.get(eventType).push(callback);
    }
    
    /**
     * Remove callback
     */
    off(eventType, callback) {
        if (this.callbacks.has(eventType)) {
            const callbacks = this.callbacks.get(eventType);
            const index = callbacks.indexOf(callback);
            if (index > -1) {
                callbacks.splice(index, 1);
            }
        }
    }
    
    /**
     * Trigger callbacks for event type
     */
    triggerCallback(eventType, data) {
        if (this.callbacks.has(eventType)) {
            this.callbacks.get(eventType).forEach(callback => {
                try {
                    callback(data);
                } catch (error) {
                    console.error(`Error in ${eventType} callback:`, error);
                }
            });
        }
    }
    
    /**
     * Attempt to reconnect
     */
    attemptReconnect() {
        this.reconnectAttempts++;
        const delay = this.reconnectDelay * Math.pow(2, this.reconnectAttempts - 1); // Exponential backoff
        
        console.log(`🔄 Attempting reconnect ${this.reconnectAttempts}/${this.maxReconnectAttempts} in ${delay}ms`);
        
        setTimeout(() => {
            if (this.reconnectAttempts <= this.maxReconnectAttempts) {
                this.connect();
            } else {
                console.error('❌ Max reconnection attempts reached');
            }
        }, delay);
    }
    
    /**
     * Start ping/pong for connection health monitoring
     */
    startPingPong() {
        this.pingInterval = setInterval(() => {
            if (this.isConnected) {
                this.send({
                    type: 'ping',
                    timestamp: Date.now()
                });
            }
        }, 30000); // Ping every 30 seconds
    }
    
    /**
     * Stop ping/pong
     */
    stopPingPong() {
        if (this.pingInterval) {
            clearInterval(this.pingInterval);
            this.pingInterval = null;
        }
    }
    
    /**
     * Handle pong response
     */
    handlePong(message) {
        const latency = Date.now() - message.timestamp;
        console.log(`🏓 Pong received - Latency: ${latency}ms`);
        
        // Update latency display
        const latencyElement = document.getElementById('latency');
        if (latencyElement) {
            latencyElement.textContent = `${latency}ms`;
            
            // Color code based on latency
            if (latency < 50) {
                latencyElement.style.color = '#00ff88';
            } else if (latency < 100) {
                latencyElement.style.color = '#ffdd00';
            } else {
                latencyElement.style.color = '#ff4444';
            }
        }
    }
    
    /**
     * Update connection status in UI
     */
    updateConnectionUI(connected) {
        // Update status lights
        const statusLights = document.querySelectorAll('.status-light');
        statusLights.forEach(light => {
            light.style.backgroundColor = connected ? '#00ff88' : '#ff4444';
            light.style.animation = connected ? 'pulse 2s infinite' : 'none';
        });
        
        // Update connection status text
        const statusTexts = document.querySelectorAll('.connection-status span');
        statusTexts.forEach(text => {
            if (text.textContent.includes('Engine')) {
                text.textContent = connected ? 'C++ Engine Connected' : 'C++ Engine Offline';
            }
        });
        
        // Update pulse dots
        const pulseDots = document.querySelectorAll('.pulse-dot');
        pulseDots.forEach(dot => {
            dot.style.backgroundColor = connected ? '#00ff88' : '#ff4444';
        });
    }
    
    /**
     * Subscribe to market data
     */
    subscribeToMarketData(symbols) {
        return this.send({
            type: 'subscribe',
            symbols: symbols,
            timestamp: Date.now()
        });
    }
    
    /**
     * Submit trading order
     */
    submitOrder(order) {
        return this.send({
            type: 'order',
            ...order,
            timestamp: Date.now()
        });
    }
    
    /**
     * Control strategy
     */
    controlStrategy(action, strategyName) {
        return this.send({
            type: 'strategy_control',
            action: action, // 'activate', 'deactivate', 'pause'
            strategy: strategyName,
            timestamp: Date.now()
        });
    }
    
    /**
     * Emergency stop all trading
     */
    emergencyStop() {
        return this.send({
            type: 'emergency_stop',
            timestamp: Date.now()
        });
    }
    
    /**
     * Get connection status
     */
    getStatus() {
        return {
            connected: this.isConnected,
            reconnectAttempts: this.reconnectAttempts,
            url: this.url
        };
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = VFXWebSocket;
} else {
    window.VFXWebSocket = VFXWebSocket;
}