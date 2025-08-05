/**
 * VFX Trading Platform - Notification System
 * Professional alert and notification management
 */

class VFXNotifications {
    constructor(containerId = 'notification-container') {
        this.container = document.getElementById(containerId);
        this.notifications = new Map();
        this.nextId = 1;
        this.sounds = new Map();
        
        // Default settings
        this.settings = {
            maxNotifications: 10,
            defaultDuration: 5000,
            enableSounds: true,
            enableVibration: true,
            position: 'top-right'
        };
        
        this.init();
        console.log('🔔 VFX Notifications initialized');
    }
    
    /**
     * Initialize notification system
     */
    init() {
        this.createContainer();
        this.loadSounds();
        this.setupKeyboardShortcuts();
    }
    
    /**
     * Create notification container if it doesn't exist
     */
    createContainer() {
        if (!this.container) {
            this.container = document.createElement('div');
            this.container.id = 'notification-container';
            this.container.className = 'vfx-notification-container';
            document.body.appendChild(this.container);
        }
        
        this.container.style.cssText = `
            position: fixed;
            top: 1rem;
            right: 1rem;
            z-index: 10000;
            display: flex;
            flex-direction: column;
            gap: 0.5rem;
            max-width: 400px;
            pointer-events: none;
        `;
    }
    
    /**
     * Load notification sounds
     */
    loadSounds() {
        // Create simple audio context for beeps (no external files needed)
        try {
            this.audioContext = new (window.AudioContext || window.webkitAudioContext)();
            this.sounds.set('success', this.createTone(800, 0.1));
            this.sounds.set('warning', this.createTone(600, 0.15));
            this.sounds.set('error', this.createTone(400, 0.2));
            this.sounds.set('info', this.createTone(700, 0.1));
        } catch (e) {
            console.warn('Audio not available:', e);
            this.settings.enableSounds = false;
        }
    }
    
    /**
     * Create simple tone
     */
    createTone(frequency, duration) {
        return () => {
            if (!this.settings.enableSounds || !this.audioContext) return;
            
            const oscillator = this.audioContext.createOscillator();
            const gainNode = this.audioContext.createGain();
            
            oscillator.connect(gainNode);
            gainNode.connect(this.audioContext.destination);
            
            oscillator.frequency.setValueAtTime(frequency, this.audioContext.currentTime);
            oscillator.type = 'sine';
            
            gainNode.gain.setValueAtTime(0.1, this.audioContext.currentTime);
            gainNode.gain.exponentialRampToValueAtTime(0.01, this.audioContext.currentTime + duration);
            
            oscillator.start(this.audioContext.currentTime);
            oscillator.stop(this.audioContext.currentTime + duration);
        };
    }
    
    /**
     * Setup keyboard shortcuts
     */
    setupKeyboardShortcuts() {
        document.addEventListener('keydown', (e) => {
            // Ctrl/Cmd + Shift + C to clear all notifications
            if ((e.ctrlKey || e.metaKey) && e.shiftKey && e.key === 'C') {
                e.preventDefault();
                this.clearAll();
            }
        });
    }
    
    /**
     * Show notification
     */
    show(message, type = 'info', options = {}) {
        const id = this.nextId++;
        
        const notification = {
            id,
            message,
            type,
            timestamp: Date.now(),
            duration: options.duration || this.settings.defaultDuration,
            persistent: options.persistent || false,
            priority: options.priority || 'normal',
            metadata: options.metadata || {}
        };
        
        this.notifications.set(id, notification);
        this.renderNotification(notification);
        this.playSound(type);
        this.vibrate(type);
        
        // Auto-remove if not persistent
        if (!notification.persistent && notification.duration > 0) {
            setTimeout(() => {
                this.remove(id);
            }, notification.duration);
        }
        
        // Limit total notifications
        this.enforceLimit();
        
        console.log(`🔔 ${type.toUpperCase()}: ${message}`);
        return id;
    }
    
    /**
     * Render notification element
     */
    renderNotification(notification) {
        const element = document.createElement('div');
        element.className = `vfx-notification ${notification.type}`;
        element.dataset.id = notification.id;
        
        // Create notification content
        element.innerHTML = `
            <div class="notification-content">
                <div class="notification-icon">
                    ${this.getIcon(notification.type)}
                </div>
                <div class="notification-body">
                    <div class="notification-message">${notification.message}</div>
                    <div class="notification-time">${this.formatTime(notification.timestamp)}</div>
                </div>
                <button class="notification-close" onclick="window.vfxNotifications.remove(${notification.id})">
                    ×
                </button>
            </div>
            <div class="notification-progress"></div>
        `;
        
        // Apply styles
        this.styleNotification(element, notification.type);
        
        // Add to container
        this.container.appendChild(element);
        
        // Animate in
        requestAnimationFrame(() => {
            element.classList.add('show');
        });
        
        // Setup progress bar for non-persistent notifications
        if (!notification.persistent && notification.duration > 0) {
            this.animateProgress(element, notification.duration);
        }
    }
    
    /**
     * Get icon for notification type
     */
    getIcon(type) {
        const icons = {
            success: '✅',
            warning: '⚠️',
            error: '❌',
            info: 'ℹ️',
            trade: '💹',
            connection: '🔌',
            strategy: '🎯',
            profit: '💰',
            loss: '📉'
        };
        return icons[type] || 'ℹ️';
    }
    
    /**
     * Style notification element
     */
    styleNotification(element, type) {
        const baseStyles = `
            background: linear-gradient(135deg, rgba(0, 0, 0, 0.95), rgba(10, 10, 10, 0.95));
            border: 1px solid;
            border-radius: 12px;
            padding: 1rem;
            margin-bottom: 0.5rem;
            backdrop-filter: blur(20px);
            transform: translateX(100%);
            opacity: 0;
            transition: all 0.4s cubic-bezier(0.4, 0, 0.2, 1);
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
            position: relative;
            pointer-events: auto;
            overflow: hidden;
        `;
        
        const typeColors = {
            success: '#00ff88',
            warning: '#ffdd00',
            error: '#ff4444',
            info: '#00aaff',
            trade: '#00ff88',
            connection: '#00aaff',
            strategy: '#ffdd00',
            profit: '#00ff88',
            loss: '#ff4444'
        };
        
        element.style.cssText = baseStyles;
        element.style.borderColor = typeColors[type] || '#00aaff';
        element.style.boxShadow += `, 0 0 20px ${typeColors[type] || '#00aaff'}40`;
        
        // Content styles
        const content = element.querySelector('.notification-content');
        content.style.cssText = `
            display: flex;
            align-items: flex-start;
            gap: 0.75rem;
        `;
        
        const icon = element.querySelector('.notification-icon');
        icon.style.cssText = `
            font-size: 1.2rem;
            margin-top: 0.1rem;
        `;
        
        const body = element.querySelector('.notification-body');
        body.style.cssText = `
            flex: 1;
            min-width: 0;
        `;
        
        const message = element.querySelector('.notification-message');
        message.style.cssText = `
            color: #ffffff;
            font-size: 0.9rem;
            line-height: 1.4;
            margin-bottom: 0.25rem;
            word-wrap: break-word;
        `;
        
        const time = element.querySelector('.notification-time');
        time.style.cssText = `
            color: ${typeColors[type] || '#00aaff'};
            font-size: 0.75rem;
            font-family: monospace;
        `;
        
        const closeBtn = element.querySelector('.notification-close');
        closeBtn.style.cssText = `
            background: none;
            border: none;
            color: #ffffff;
            font-size: 1.2rem;
            cursor: pointer;
            opacity: 0.7;
            transition: opacity 0.3s ease;
            padding: 0;
            margin-left: 0.5rem;
        `;
        
        const progress = element.querySelector('.notification-progress');
        progress.style.cssText = `
            position: absolute;
            bottom: 0;
            left: 0;
            height: 3px;
            background: ${typeColors[type] || '#00aaff'};
            width: 100%;
            transform-origin: left;
            border-radius: 0 0 12px 12px;
        `;
    }
    
    /**
     * Animate progress bar
     */
    animateProgress(element, duration) {
        const progress = element.querySelector('.notification-progress');
        if (!progress) return;
        
        progress.style.transition = `transform ${duration}ms linear`;
        
        // Start animation
        requestAnimationFrame(() => {
            progress.style.transform = 'scaleX(0)';
        });
    }
    
    /**
     * Format timestamp
     */
    formatTime(timestamp) {
        const date = new Date(timestamp);
        return date.toLocaleTimeString([], { 
            hour: '2-digit', 
            minute: '2-digit', 
            second: '2-digit' 
        });
    }
    
    /**
     * Play notification sound
     */
    playSound(type) {
        const sound = this.sounds.get(type) || this.sounds.get('info');
        if (sound && this.settings.enableSounds) {
            try {
                sound();
            } catch (e) {
                console.warn('Could not play notification sound:', e);
            }
        }
    }
    
    /**
     * Vibrate device (mobile)
     */
    vibrate(type) {
        if (!this.settings.enableVibration || !navigator.vibrate) return;
        
        const patterns = {
            success: [100],
            warning: [100, 50, 100],
            error: [200, 100, 200],
            info: [50]
        };
        
        const pattern = patterns[type] || patterns.info;
        navigator.vibrate(pattern);
    }
    
    /**
     * Remove notification
     */
    remove(id) {
        const element = this.container.querySelector(`[data-id="${id}"]`);
        if (element) {
            element.classList.remove('show');
            element.style.transform = 'translateX(100%)';
            element.style.opacity = '0';
            
            setTimeout(() => {
                if (element.parentNode) {
                    element.parentNode.removeChild(element);
                }
            }, 400);
        }
        
        this.notifications.delete(id);
    }
    
    /**
     * Clear all notifications
     */
    clearAll() {
        this.notifications.forEach((_, id) => {
            this.remove(id);
        });
    }
    
    /**
     * Enforce notification limit
     */
    enforceLimit() {
        const notificationIds = Array.from(this.notifications.keys());
        if (notificationIds.length > this.settings.maxNotifications) {
            const oldestId = notificationIds[0];
            this.remove(oldestId);
        }
    }
    
    /**
     * Update settings
     */
    updateSettings(newSettings) {
        this.settings = { ...this.settings, ...newSettings };
    }
    
    /**
     * Show specific notification types
     */
    success(message, options = {}) {
        return this.show(message, 'success', options);
    }
    
    warning(message, options = {}) {
        return this.show(message, 'warning', options);
    }
    
    error(message, options = {}) {
        return this.show(message, 'error', options);
    }
    
    info(message, options = {}) {
        return this.show(message, 'info', options);
    }
    
    trade(message, options = {}) {
        return this.show(message, 'trade', options);
    }
    
    connection(message, options = {}) {
        return this.show(message, 'connection', options);
    }
    
    strategy(message, options = {}) {
        return this.show(message, 'strategy', options);
    }
    
    profit(message, options = {}) {
        return this.show(message, 'profit', options);
    }
    
    loss(message, options = {}) {
        return this.show(message, 'loss', options);
    }
    
    /**
     * Show trading-specific notifications
     */
    orderExecuted(order) {
        const side = order.side === 'BUY' ? '🟢' : '🔴';
        const message = `${side} ${order.side} ${order.quantity} ${order.symbol} @ ${order.price}`;
        return this.trade(message, { metadata: order });
    }
    
    strategySignal(signal) {
        const confidence = (signal.confidence * 100).toFixed(1);
        const message = `${signal.strategy}: ${signal.side} ${signal.symbol} (${confidence}% confidence)`;
        return this.strategy(message, { metadata: signal });
    }
    
    connectionStatus(connected) {
        const message = connected ? 'Connected to trading engine' : 'Connection lost - attempting reconnect';
        const type = connected ? 'success' : 'warning';
        return this.show(message, type, { duration: 3000 });
    }
    
    profitAlert(amount, symbol) {
        const message = `💰 Profit: +${amount.toFixed(2)} on ${symbol}`;
        return this.profit(message);
    }
    
    lossAlert(amount, symbol) {
        const message = `📉 Loss: -${Math.abs(amount).toFixed(2)} on ${symbol}`;
        return this.loss(message);
    }
    
    /**
     * Get notification statistics
     */
    getStats() {
        const notifications = Array.from(this.notifications.values());
        const typeCount = {};
        
        notifications.forEach(n => {
            typeCount[n.type] = (typeCount[n.type] || 0) + 1;
        });
        
        return {
            total: notifications.length,
            byType: typeCount,
            oldest: notifications.length > 0 ? Math.min(...notifications.map(n => n.timestamp)) : null,
            newest: notifications.length > 0 ? Math.max(...notifications.map(n => n.timestamp)) : null
        };
    }
    
    /**
     * Export notifications for logging
     */
    exportNotifications() {
        return Array.from(this.notifications.values()).map(n => ({
            id: n.id,
            message: n.message,
            type: n.type,
            timestamp: n.timestamp,
            formatted_time: this.formatTime(n.timestamp)
        }));
    }
}

// Auto-initialize on window load
window.addEventListener('DOMContentLoaded', () => {
    window.vfxNotifications = new VFXNotifications();
});

// Add show class styles
const showStyles = `
.vfx-notification.show {
    transform: translateX(0) !important;
    opacity: 1 !important;
}

.notification-close:hover {
    opacity: 1 !important;
    transform: scale(1.1);
}
`;

// Inject styles
const styleSheet = document.createElement('style');
styleSheet.textContent = showStyles;
document.head.appendChild(styleSheet);

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = VFXNotifications;
} else {
    window.VFXNotifications = VFXNotifications;
}