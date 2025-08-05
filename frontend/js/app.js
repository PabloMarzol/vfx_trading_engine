/**
 * VFX Trading Platform - Frontend Application
 * Connects to C++ Engine via WebSocket for ultra-low latency
 */

class VFXTradingApp {
    constructor() {
        this.websocket = null;
        this.isConnected = false;
        this.latencyStats = [];
        this.tradingData = [];
        this.positions = new Map();
        this.strategies = new Map();
        
        // Performance metrics
        this.metrics = {
            totalValue: 2847293.45,
            dailyPnL: 428156.78,
            winRate: 94.3,
            openPositions: 24,
            marginUsed: 32.1,
            cpuUsage: 15.8,
            memoryUsage: 8.2
        };
        
        this.initializeApp();
    }
    
    async initializeApp() {
        console.log('🚀 VFX Trading Platform v3.0.0 - Initializing...');
        
        // Initialize components
        this.initializeWebSocket();
        this.initializeEventListeners();
        this.initializeCharts();
        this.startPerformanceMonitoring();
        this.startDataUpdates();
        
        // Initialize strategies
        this.initializeStrategies();
        
        console.log('✅ VFX Trading Platform initialized successfully');
        this.showNotification('VFX Trading Platform Online - All systems operational', 'success');
    }
    
    initializeWebSocket() {
        // Connect to C++ trading engine
        const wsUrl = 'ws://localhost:8080/trading';
        
        try {
            this.websocket = new WebSocket(wsUrl);
            
            this.websocket.onopen = () => {
                this.isConnected = true;
                this.updateConnectionStatus(true);
                console.log('🔌 Connected to C++ Trading Engine');
                this.showNotification('Connected to C++ Trading Engine', 'success');
                
                // Subscribe to market data
                this.subscribeToMarketData(['BTC/USD', 'ETH/USD', 'AAPL', 'TSLA', 'NVDA']);
            };
            
            this.websocket.onmessage = (event) => {
                this.handleWebSocketMessage(JSON.parse(event.data));
            };
            
            this.websocket.onclose = () => {
                this.isConnected = false;
                this.updateConnectionStatus(false);
                console.log('❌ Disconnected from C++ Trading Engine');
                this.showNotification('Connection lost - Attempting to reconnect...', 'warning');
                
                // Attempt to reconnect after 5 seconds
                setTimeout(() => this.initializeWebSocket(), 5000);
            };
            
            this.websocket.onerror = (error) => {
                console.error('🚨 WebSocket error:', error);
                this.showNotification('WebSocket connection error', 'error');
            };
            
        } catch (error) {
            console.error('Failed to initialize WebSocket:', error);
            this.showNotification('Failed to connect to trading engine', 'error');
        }
    }
    
    handleWebSocketMessage(message) {
        const startTime = performance.now();
        
        switch (message.type) {
            case 'market_data':
                this.handleMarketData(message.data);
                break;
                
            case 'execution':
                this.handleExecution(message.data);
                break;
                
            case 'position_update':
                this.handlePositionUpdate(message.data);
                break;
                
            case 'strategy_signal':
                this.handleStrategySignal(message.data);
                break;
                
            case 'performance_metrics':
                this.handlePerformanceMetrics(message.data);
                break;
                
            case 'pong':
                this.handleLatencyMeasurement(message.timestamp, startTime);
                break;
                
            default:
                console.log('Unknown message type:', message.type);
        }
    }
    
    subscribeToMarketData(symbols) {
        if (this.websocket && this.isConnected) {
            const message = {
                type: 'subscribe',
                symbols: symbols,
                timestamp: Date.now()
            };
            this.websocket.send(JSON.stringify(message));
        }
    }
    
    handleMarketData(data) {
        // Update price displays
        this.updatePriceDisplay(data.symbol, data.price, data.change);
        
        // Update chart if needed
        if (this.priceChart) {
            this.updateChart(data);
        }
        
        // Store for analytics
        this.storePriceData(data);
    }
    
    handleExecution(execution) {
        console.log('💹 Trade Executed:', execution);
        
        // Add to trading log
        this.addToTradingLog(execution);
        
        // Update positions
        this.updatePosition(execution);
        
        // Show notification
        const side = execution.side === 'BUY' ? '🟢' : '🔴';
        this.showNotification(
            `${side} ${execution.side} ${execution.quantity} ${execution.symbol} @ $${execution.price}`,
            'success'
        );
        
        // Update performance metrics
        this.updateMetrics(execution);
    }
    
    handlePositionUpdate(position) {
        this.positions.set(position.symbol, position);
        this.updatePositionDisplay();
    }
    
    handleStrategySignal(signal) {
        console.log('📊 Strategy Signal:', signal);
        
        // Show strategy notification
        const confidence = (signal.confidence * 100).toFixed(1);
        this.showNotification(
            `${signal.strategy}: ${signal.side} ${signal.symbol} (${confidence}% confidence)`,
            signal.side === 'BUY' ? 'success' : 'warning'
        );
        
        // Update strategy display
        this.updateStrategyDisplay(signal);
    }
    
    handlePerformanceMetrics(metrics) {
        this.metrics = { ...this.metrics, ...metrics };
        this.updateMetricsDisplay();
    }
    
    handleLatencyMeasurement(timestamp, startTime) {
        const latency = performance.now() - startTime;
        this.latencyStats.push(latency);
        
        // Keep only last 100 measurements
        if (this.latencyStats.length > 100) {
            this.latencyStats.shift();
        }
        
        // Update latency display
        const avgLatency = this.latencyStats.reduce((a, b) => a + b, 0) / this.latencyStats.length;
        this.updateLatencyDisplay(avgLatency);
    }
    
    initializeEventListeners() {
        // Trade button handlers
        document.getElementById('buy-btn')?.addEventListener('click', () => {
            this.executeTrade('BUY');
        });
        
        document.getElementById('sell-btn')?.addEventListener('click', () => {
            this.executeTrade('SELL');
        });
        
        // Emergency stop button
        document.querySelector('.emergency-stop')?.addEventListener('click', () => {
            this.emergencyStop();
        });
        
        // Keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            if ((e.ctrlKey || e.metaKey) && e.key === 'b') {
                e.preventDefault();
                this.executeTrade('BUY');
            } else if ((e.ctrlKey || e.metaKey) && e.key === 's') {
                e.preventDefault();
                this.executeTrade('SELL');
            } else if (e.key === 'Escape') {
                this.emergencyStop();
            }
        });
        
        // Strategy toggle handlers
        document.querySelectorAll('.strategy-item').forEach(item => {
            item.addEventListener('click', () => {
                this.toggleStrategy(item.dataset.strategy);
            });
        });
    }
    
    executeTrade(side) {
        const symbol = document.getElementById('asset-select')?.value || 'BTC/USD';
        const quantity = parseFloat(document.getElementById('quantity')?.value || '1');
        const price = document.getElementById('price')?.value;
        
        if (!symbol || !quantity || quantity <= 0) {
            this.showNotification('Please enter valid trade parameters', 'warning');
            return;
        }
        
        const order = {
            type: 'order',
            symbol: symbol,
            side: side,
            quantity: quantity,
            order_type: price ? 'LIMIT' : 'MARKET',
            price: price ? parseFloat(price) : undefined,
            timestamp: Date.now(),
            source: 'manual'
        };
        
        if (this.websocket && this.isConnected) {
            this.websocket.send(JSON.stringify(order));
            console.log(`📤 Sent ${side} order:`, order);
            
            // Add visual feedback
            const button = document.getElementById(side.toLowerCase() + '-btn');
            button?.classList.add('loading');
            setTimeout(() => button?.classList.remove('loading'), 1000);
        } else {
            this.showNotification('Not connected to trading engine', 'error');
        }
    }
    
    emergencyStop() {
        if (confirm('⚠️ EMERGENCY STOP - This will cancel all orders and close all positions. Are you sure?')) {
            const stopMessage = {
                type: 'emergency_stop',
                timestamp: Date.now()
            };
            
            if (this.websocket && this.isConnected) {
                this.websocket.send(JSON.stringify(stopMessage));
                this.showNotification('🛑 EMERGENCY STOP ACTIVATED', 'error');
                console.log('🛑 Emergency stop activated');
            }
        }
    }
    
    initializeCharts() {
        const chartCanvas = document.getElementById('price-chart');
        if (!chartCanvas) return;
        
        this.priceChart = new PriceChart(chartCanvas);
        this.priceChart.initialize();
    }
    
    updateChart(data) {
        if (this.priceChart) {
            this.priceChart.addDataPoint({
                timestamp: new Date(data.timestamp),
                price: data.price,
                volume: data.volume
            });
        }
    }
    
    addToTradingLog(execution) {
        const logBody = document.getElementById('trading-log');
        if (!logBody) return;
        
        // Remove old entries if too many
        while (logBody.children.length > 50) {
            logBody.removeChild(logBody.lastChild);
        }
        
        const entry = document.createElement('div');
        entry.className = 'trade-entry';
        
        const time = new Date(execution.timestamp).toLocaleTimeString();
        const sideClass = execution.side === 'BUY' ? 'buy' : 'sell';
        
        entry.innerHTML = `
            <span>${time}</span>
            <span>${execution.symbol}</span>
            <span class="trade-side ${sideClass}">${execution.side}</span>
            <span>${execution.price.toFixed(2)}</span>
            <span>${execution.quantity}</span>
            <span class="positive">+${(execution.quantity * execution.price * 0.001).toFixed(2)}</span>
        `;
        
        // Add with animation
        entry.style.opacity = '0';
        entry.style.transform = 'translateY(-10px)';
        logBody.insertBefore(entry, logBody.firstChild);
        
        // Animate in
        setTimeout(() => {
            entry.style.transition = 'all 0.3s ease';
            entry.style.opacity = '1';
            entry.style.transform = 'translateY(0)';
        }, 10);
    }
    
    updatePosition(execution) {
        const symbol = execution.symbol;
        let position = this.positions.get(symbol) || {
            symbol: symbol,
            quantity: 0,
            avgPrice: 0,
            unrealizedPnL: 0
        };
        
        // Update position
        if (execution.side === 'BUY') {
            const newQuantity = position.quantity + execution.quantity;
            position.avgPrice = ((position.avgPrice * position.quantity) + 
                               (execution.price * execution.quantity)) / newQuantity;
            position.quantity = newQuantity;
        } else {
            position.quantity -= execution.quantity;
            if (position.quantity <= 0) {
                this.positions.delete(symbol);
                return;
            }
        }
        
        this.positions.set(symbol, position);
        this.updatePositionDisplay();
    }
    
    updatePositionDisplay() {
        // Update positions count
        const positionsElement = document.getElementById('open-positions');
        if (positionsElement) {
            positionsElement.textContent = this.positions.size;
        }
    }
    
    updateMetricsDisplay() {
        // Update all metric displays
        const updates = {
            'total-value': `${this.metrics.totalValue.toLocaleString()}`,
            'daily-pnl': `+${this.metrics.dailyPnL.toLocaleString()}`,
            'open-positions': this.metrics.openPositions,
            'margin-used': `${this.metrics.marginUsed}%`,
            'cpu-usage': `${this.metrics.cpuUsage}%`,
            'memory-usage': `${this.metrics.memoryUsage}GB`
        };
        
        Object.entries(updates).forEach(([id, value]) => {
            const element = document.getElementById(id);
            if (element) {
                // Add animation for value changes
                if (element.textContent !== value.toString()) {
                    element.classList.add('number-animate');
                    element.textContent = value;
                    setTimeout(() => element.classList.remove('number-animate'), 300);
                }
            }
        });
    }
    
    updateConnectionStatus(connected) {
        const statusElements = document.querySelectorAll('.status-light');
        statusElements.forEach(element => {
            element.style.backgroundColor = connected ? '#00ff88' : '#ff4444';
        });
    }
    
    updateLatencyDisplay(latency) {
        const latencyElement = document.getElementById('latency');
        if (latencyElement) {
            latencyElement.textContent = `${latency.toFixed(2)}ms`;
            
            // Color code based on latency
            if (latency < 1) {
                latencyElement.style.color = '#00ff88';
            } else if (latency < 5) {
                latencyElement.style.color = '#ffdd00';
            } else {
                latencyElement.style.color = '#ff4444';
            }
        }
    }
    
    initializeStrategies() {
        const strategies = [
            { name: 'Momentum_Alpha', active: true, performance: 24.8 },
            { name: 'Mean_Reversion', active: true, performance: 12.3 },
            { name: 'Statistical_Arbitrage', active: false, performance: 8.7 }
        ];
        
        strategies.forEach(strategy => {
            this.strategies.set(strategy.name, strategy);
        });
        
        this.updateStrategyList();
    }
    
    updateStrategyList() {
        const strategyList = document.getElementById('strategy-list');
        if (!strategyList) return;
        
        strategyList.innerHTML = '';
        
        this.strategies.forEach(strategy => {
            const item = document.createElement('div');
            item.className = `strategy-item ${strategy.active ? 'active' : ''}`;
            item.dataset.strategy = strategy.name;
            
            item.innerHTML = `
                <div class="strategy-info">
                    <span class="strategy-name">${strategy.name.replace('_', ' ')}</span>
                    <span class="strategy-performance">+${strategy.performance}%</span>
                </div>
                <div class="strategy-status ${strategy.active ? 'running' : 'paused'}"></div>
            `;
            
            item.addEventListener('click', () => this.toggleStrategy(strategy.name));
            strategyList.appendChild(item);
        });
    }
    
    toggleStrategy(strategyName) {
        const strategy = this.strategies.get(strategyName);
        if (!strategy) return;
        
        strategy.active = !strategy.active;
        
        // Send to backend
        const message = {
            type: 'strategy_control',
            action: strategy.active ? 'activate' : 'deactivate',
            strategy: strategyName,
            timestamp: Date.now()
        };
        
        if (this.websocket && this.isConnected) {
            this.websocket.send(JSON.stringify(message));
        }
        
        this.updateStrategyList();
        
        const action = strategy.active ? 'activated' : 'deactivated';
        this.showNotification(`Strategy ${strategyName.replace('_', ' ')} ${action}`, 'info');
    }
    
    updateStrategyDisplay(signal) {
        const strategy = this.strategies.get(signal.strategy);
        if (strategy) {
            // Update performance (simulate)
            const performanceChange = (Math.random() - 0.5) * 2; // ±1%
            strategy.performance += performanceChange;
            strategy.performance = Math.max(0, strategy.performance);
            
            this.updateStrategyList();
        }
    }
    
    startPerformanceMonitoring() {
        // Send ping to measure latency
        setInterval(() => {
            if (this.websocket && this.isConnected) {
                const pingMessage = {
                    type: 'ping',
                    timestamp: Date.now()
                };
                this.websocket.send(JSON.stringify(pingMessage));
            }
        }, 5000);
        
        // Update system metrics
        setInterval(() => {
            this.updateSystemMetrics();
        }, 2000);
    }
    
    updateSystemMetrics() {
        // Simulate metric updates (in real implementation, get from backend)
        this.metrics.cpuUsage += (Math.random() - 0.5) * 2;
        this.metrics.cpuUsage = Math.max(0, Math.min(100, this.metrics.cpuUsage));
        
        this.metrics.memoryUsage += (Math.random() - 0.5) * 0.1;
        this.metrics.memoryUsage = Math.max(0, this.metrics.memoryUsage);
        
        this.updateMetricsDisplay();
    }
    
    startDataUpdates() {
        // Simulate real-time data updates
        setInterval(() => {
            this.simulateMarketData();
        }, 1000);
        
        // Update portfolio value
        setInterval(() => {
            this.updatePortfolioValue();
        }, 5000);
    }
    
    simulateMarketData() {
        const symbols = ['BTC/USD', 'ETH/USD', 'AAPL', 'TSLA', 'NVDA'];
        const symbol = symbols[Math.floor(Math.random() * symbols.length)];
        
        // Simulate price update
        const basePrice = this.getBasePrice(symbol);
        const variation = (Math.random() - 0.5) * 0.02; // ±1%
        const newPrice = basePrice * (1 + variation);
        
        const marketData = {
            symbol: symbol,
            price: newPrice,
            volume: Math.floor(Math.random() * 1000000) + 100000,
            bid: newPrice - 0.01,
            ask: newPrice + 0.01,
            change: variation * 100,
            timestamp: Date.now()
        };
        
        this.handleMarketData(marketData);
    }
    
    getBasePrice(symbol) {
        const basePrices = {
            'BTC/USD': 68450,
            'ETH/USD': 3892,
            'AAPL': 175,
            'TSLA': 245,
            'NVDA': 875
        };
        return basePrices[symbol] || 100;
    }
    
    updatePortfolioValue() {
        // Simulate portfolio growth
        const change = (Math.random() - 0.4) * 1000; // Slight upward bias
        this.metrics.totalValue += change;
        this.metrics.dailyPnL += change;
        
        this.updateMetricsDisplay();
    }
    
    updatePriceDisplay(symbol, price, change) {
        // Update any price displays in the UI
        const priceElements = document.querySelectorAll(`[data-symbol="${symbol}"]`);
        priceElements.forEach(element => {
            element.textContent = `${price.toFixed(2)}`;
            
            // Add color coding
            if (change > 0) {
                element.style.color = '#00ff88';
            } else if (change < 0) {
                element.style.color = '#ff4444';
            }
        });
    }
    
    storePriceData(data) {
        // Store for charts and analytics
        this.tradingData.push({
            timestamp: data.timestamp,
            symbol: data.symbol,
            price: data.price,
            volume: data.volume
        });
        
        // Keep only recent data
        if (this.tradingData.length > 10000) {
            this.tradingData = this.tradingData.slice(-5000);
        }
    }
    
    showNotification(message, type = 'info') {
        const container = document.getElementById('notification-container');
        if (!container) return;
        
        const notification = document.createElement('div');
        notification.className = `notification ${type}`;
        
        notification.innerHTML = `
            <div class="notification-icon"></div>
            <div class="notification-text">${message}</div>
            <button class="notification-close">&times;</button>
        `;
        
        // Add event listener for close button
        notification.querySelector('.notification-close').addEventListener('click', () => {
            notification.remove();
        });
        
        container.appendChild(notification);
        
        // Show with animation
        setTimeout(() => notification.classList.add('show'), 10);
        
        // Auto remove after 5 seconds
        setTimeout(() => {
            notification.classList.remove('show');
            setTimeout(() => notification.remove(), 400);
        }, 5000);
        
        console.log(`📢 ${type.toUpperCase()}: ${message}`);
    }
    
    // Public API methods
    getMetrics() {
        return { ...this.metrics };
    }
    
    getPositions() {
        return Array.from(this.positions.values());
    }
    
    getStrategies() {
        return Array.from(this.strategies.values());
    }
    
    getTradingData() {
        return [...this.tradingData];
    }
    
    isConnectedToEngine() {
        return this.isConnected;
    }
}

// Simple chart implementation
class PriceChart {
    constructor(canvas) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
        this.data = [];
        this.maxDataPoints = 200;
    }
    
    initialize() {
        this.resize();
        window.addEventListener('resize', () => this.resize());
        this.startAnimation();
    }
    
    resize() {
        const rect = this.canvas.getBoundingClientRect();
        this.canvas.width = rect.width * window.devicePixelRatio;
        this.canvas.height = rect.height * window.devicePixelRatio;
        this.ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
        this.canvas.style.width = rect.width + 'px';
        this.canvas.style.height = rect.height + 'px';
    }
    
    addDataPoint(point) {
        this.data.push(point);
        if (this.data.length > this.maxDataPoints) {
            this.data.shift();
        }
    }
    
    startAnimation() {
        const animate = () => {
            this.draw();
            requestAnimationFrame(animate);
        };
        animate();
    }
    
    draw() {
        const ctx = this.ctx;
        const width = this.canvas.width / window.devicePixelRatio;
        const height = this.canvas.height / window.devicePixelRatio;
        
        // Clear canvas
        ctx.clearRect(0, 0, width, height);
        
        if (this.data.length < 2) return;
        
        // Calculate price range
        const prices = this.data.map(d => d.price);
        const minPrice = Math.min(...prices);
        const maxPrice = Math.max(...prices);
        const priceRange = maxPrice - minPrice;
        
        // Draw grid
        ctx.strokeStyle = 'rgba(0, 255, 136, 0.1)';
        ctx.lineWidth = 1;
        
        for (let i = 0; i <= 4; i++) {
            const y = (height / 4) * i;
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(width, y);
            ctx.stroke();
        }
        
        // Draw price line
        ctx.strokeStyle = '#00ff88';
        ctx.lineWidth = 2;
        ctx.shadowColor = '#00ff88';
        ctx.shadowBlur = 10;
        
        ctx.beginPath();
        this.data.forEach((point, index) => {
            const x = (width / (this.data.length - 1)) * index;
            const y = height - ((point.price - minPrice) / priceRange) * height;
            
            if (index === 0) {
                ctx.moveTo(x, y);
            } else {
                ctx.lineTo(x, y);
            }
        });
        ctx.stroke();
        
        // Draw gradient fill
        ctx.shadowBlur = 0;
        const gradient = ctx.createLinearGradient(0, 0, 0, height);
        gradient.addColorStop(0, 'rgba(0, 255, 136, 0.3)');
        gradient.addColorStop(1, 'rgba(0, 255, 136, 0.0)');
        
        ctx.fillStyle = gradient;
        ctx.beginPath();
        this.data.forEach((point, index) => {
            const x = (width / (this.data.length - 1)) * index;
            const y = height - ((point.price - minPrice) / priceRange) * height;
            
            if (index === 0) {
                ctx.moveTo(x, y);
            } else {
                ctx.lineTo(x, y);
            }
        });
        ctx.lineTo(width, height);
        ctx.lineTo(0, height);
        ctx.closePath();
        ctx.fill();
        
        // Draw current price indicator
        if (this.data.length > 0) {
            const lastPoint = this.data[this.data.length - 1];
            const x = width - 10;
            const y = height - ((lastPoint.price - minPrice) / priceRange) * height;
            
            ctx.fillStyle = '#ffdd00';
            ctx.shadowColor = '#ffdd00';
            ctx.shadowBlur = 15;
            ctx.beginPath();
            ctx.arc(x, y, 4, 0, Math.PI * 2);
            ctx.fill();
        }
    }
}

// Initialize the application when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    window.vfxApp = new VFXTradingApp();
});

// Export for debugging
window.VFXTradingApp = VFXTradingApp;