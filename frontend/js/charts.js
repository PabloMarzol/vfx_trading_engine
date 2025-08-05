/**
 * VFX Trading Platform - Real-time Trading Charts
 * High-performance charting with neon VFX styling
 */

class VFXChart {
    constructor(canvas, options = {}) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
        this.animationId = null;
        
        // Default options
        this.options = {
            maxDataPoints: 200,
            backgroundColor: 'rgba(0, 0, 0, 0.9)',
            gridColor: 'rgba(0, 255, 136, 0.1)',
            lineColor: '#00ff88',
            fillColor: 'rgba(0, 255, 136, 0.1)',
            textColor: '#00ff88',
            glowColor: '#00ff88',
            updateInterval: 100, // ms
            smoothing: true,
            showVolume: true,
            showGrid: true,
            showCurrentPrice: true,
            ...options
        };
        
        // Data storage
        this.data = [];
        this.priceRange = { min: 0, max: 0 };
        this.volumeRange = { min: 0, max: 0 };
        
        // Animation state
        this.lastUpdate = 0;
        this.isAnimating = false;
        
        console.log('📊 VFX Chart initialized');
        this.initialize();
    }
    
    /**
     * Initialize the chart
     */
    initialize() {
        this.resize();
        this.setupEventListeners();
        this.startAnimation();
    }
    
    /**
     * Setup event listeners
     */
    setupEventListeners() {
        // Handle window resize
        window.addEventListener('resize', () => this.resize());
        
        // Handle mouse interactions
        this.canvas.addEventListener('mousemove', (e) => this.handleMouseMove(e));
        this.canvas.addEventListener('mouseleave', () => this.handleMouseLeave());
    }
    
    /**
     * Resize canvas to fit container
     */
    resize() {
        const rect = this.canvas.getBoundingClientRect();
        const dpr = window.devicePixelRatio || 1;
        
        this.canvas.width = rect.width * dpr;
        this.canvas.height = rect.height * dpr;
        
        this.ctx.scale(dpr, dpr);
        this.canvas.style.width = rect.width + 'px';
        this.canvas.style.height = rect.height + 'px';
        
        this.width = rect.width;
        this.height = rect.height;
    }
    
    /**
     * Add new data point
     */
    addDataPoint(point) {
        // Ensure point has required properties
        const dataPoint = {
            timestamp: point.timestamp || Date.now(),
            price: point.price || 0,
            volume: point.volume || 0,
            high: point.high || point.price,
            low: point.low || point.price,
            open: point.open || point.price,
            close: point.close || point.price
        };
        
        this.data.push(dataPoint);
        
        // Limit data points for performance
        if (this.data.length > this.options.maxDataPoints) {
            this.data.shift();
        }
        
        // Update ranges
        this.updateRanges();
    }
    
    /**
     * Update price and volume ranges
     */
    updateRanges() {
        if (this.data.length === 0) return;
        
        const prices = this.data.map(d => d.price);
        const volumes = this.data.map(d => d.volume);
        
        this.priceRange = {
            min: Math.min(...prices),
            max: Math.max(...prices)
        };
        
        this.volumeRange = {
            min: Math.min(...volumes),
            max: Math.max(...volumes)
        };
        
        // Add padding to price range
        const padding = (this.priceRange.max - this.priceRange.min) * 0.1;
        this.priceRange.min -= padding;
        this.priceRange.max += padding;
    }
    
    /**
     * Start animation loop
     */
    startAnimation() {
        this.isAnimating = true;
        const animate = (timestamp) => {
            if (!this.isAnimating) return;
            
            if (timestamp - this.lastUpdate >= this.options.updateInterval) {
                this.draw();
                this.lastUpdate = timestamp;
            }
            
            this.animationId = requestAnimationFrame(animate);
        };
        
        this.animationId = requestAnimationFrame(animate);
    }
    
    /**
     * Stop animation
     */
    stopAnimation() {
        this.isAnimating = false;
        if (this.animationId) {
            cancelAnimationFrame(this.animationId);
        }
    }
    
    /**
     * Main drawing function
     */
    draw() {
        const ctx = this.ctx;
        
        // Clear canvas
        ctx.fillStyle = this.options.backgroundColor;
        ctx.fillRect(0, 0, this.width, this.height);
        
        if (this.data.length < 2) return;
        
        // Draw grid
        if (this.options.showGrid) {
            this.drawGrid();
        }
        
        // Draw volume bars (background)
        if (this.options.showVolume) {
            this.drawVolume();
        }
        
        // Draw price line
        this.drawPriceLine();
        
        // Draw price area
        this.drawPriceArea();
        
        // Draw current price indicator
        if (this.options.showCurrentPrice) {
            this.drawCurrentPriceIndicator();
        }
        
        // Draw price labels
        this.drawPriceLabels();
    }
    
    /**
     * Draw grid lines
     */
    drawGrid() {
        const ctx = this.ctx;
        ctx.strokeStyle = this.options.gridColor;
        ctx.lineWidth = 1;
        ctx.setLineDash([2, 4]);
        
        // Horizontal grid lines (price levels)
        const priceSteps = 5;
        for (let i = 0; i <= priceSteps; i++) {
            const y = (this.height / priceSteps) * i;
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(this.width, y);
            ctx.stroke();
        }
        
        // Vertical grid lines (time)
        const timeSteps = 8;
        for (let i = 0; i <= timeSteps; i++) {
            const x = (this.width / timeSteps) * i;
            ctx.beginPath();
            ctx.moveTo(x, 0);
            ctx.lineTo(x, this.height);
            ctx.stroke();
        }
        
        ctx.setLineDash([]);
    }
    
    /**
     * Draw volume bars
     */
    drawVolume() {
        const ctx = this.ctx;
        const volumeHeight = this.height * 0.2; // Bottom 20% for volume
        const barWidth = this.width / this.data.length;
        
        this.data.forEach((point, index) => {
            const x = (this.width / (this.data.length - 1)) * index;
            const volumePercent = point.volume / this.volumeRange.max;
            const barHeight = volumeHeight * volumePercent;
            
            ctx.fillStyle = 'rgba(0, 255, 136, 0.3)';
            ctx.fillRect(x - barWidth/2, this.height - barHeight, barWidth, barHeight);
        });
    }
    
    /**
     * Draw price line with glow effect
     */
    drawPriceLine() {
        const ctx = this.ctx;
        
        // Setup line style
        ctx.strokeStyle = this.options.lineColor;
        ctx.lineWidth = 2;
        ctx.lineCap = 'round';
        ctx.lineJoin = 'round';
        
        // Add glow effect
        ctx.shadowColor = this.options.glowColor;
        ctx.shadowBlur = 10;
        
        // Draw the line
        ctx.beginPath();
        this.data.forEach((point, index) => {
            const x = (this.width / (this.data.length - 1)) * index;
            const y = this.priceToY(point.price);
            
            if (index === 0) {
                ctx.moveTo(x, y);
            } else {
                if (this.options.smoothing) {
                    // Smooth curve using quadratic curves
                    const prevPoint = this.data[index - 1];
                    const prevX = (this.width / (this.data.length - 1)) * (index - 1);
                    const prevY = this.priceToY(prevPoint.price);
                    
                    const cpX = (prevX + x) / 2;
                    const cpY = (prevY + y) / 2;
                    
                    ctx.quadraticCurveTo(cpX, prevY, x, y);
                } else {
                    ctx.lineTo(x, y);
                }
            }
        });
        
        ctx.stroke();
        ctx.shadowBlur = 0;
    }
    
    /**
     * Draw filled area under price line
     */
    drawPriceArea() {
        const ctx = this.ctx;
        
        // Create gradient
        const gradient = ctx.createLinearGradient(0, 0, 0, this.height);
        gradient.addColorStop(0, this.options.fillColor);
        gradient.addColorStop(1, 'rgba(0, 255, 136, 0)');
        
        ctx.fillStyle = gradient;
        
        // Draw area
        ctx.beginPath();
        this.data.forEach((point, index) => {
            const x = (this.width / (this.data.length - 1)) * index;
            const y = this.priceToY(point.price);
            
            if (index === 0) {
                ctx.moveTo(x, y);
            } else {
                ctx.lineTo(x, y);
            }
        });
        
        // Close the path at bottom
        const lastX = (this.width / (this.data.length - 1)) * (this.data.length - 1);
        ctx.lineTo(lastX, this.height);
        ctx.lineTo(0, this.height);
        ctx.closePath();
        ctx.fill();
    }
    
    /**
     * Draw current price indicator
     */
    drawCurrentPriceIndicator() {
        if (this.data.length === 0) return;
        
        const ctx = this.ctx;
        const lastPoint = this.data[this.data.length - 1];
        const x = this.width - 10;
        const y = this.priceToY(lastPoint.price);
        
        // Pulsing circle
        const pulseSize = 2 + Math.sin(Date.now() * 0.005) * 2;
        
        ctx.fillStyle = '#ffdd00';
        ctx.shadowColor = '#ffdd00';
        ctx.shadowBlur = 15;
        
        ctx.beginPath();
        ctx.arc(x, y, pulseSize, 0, Math.PI * 2);
        ctx.fill();
        
        ctx.shadowBlur = 0;
        
        // Price label
        ctx.fillStyle = 'rgba(0, 0, 0, 0.8)';
        ctx.fillRect(x - 40, y - 10, 80, 20);
        
        ctx.fillStyle = '#ffdd00';
        ctx.font = '12px monospace';
        ctx.textAlign = 'center';
        ctx.fillText(lastPoint.price.toFixed(2), x, y + 4);
    }
    
    /**
     * Draw price labels on Y-axis
     */
    drawPriceLabels() {
        const ctx = this.ctx;
        ctx.fillStyle = this.options.textColor;
        ctx.font = '10px monospace';
        ctx.textAlign = 'right';
        
        const priceSteps = 5;
        for (let i = 0; i <= priceSteps; i++) {
            const price = this.priceRange.min + (this.priceRange.max - this.priceRange.min) * (1 - i / priceSteps);
            const y = (this.height / priceSteps) * i;
            
            ctx.fillText(price.toFixed(2), this.width - 5, y + 4);
        }
    }
    
    /**
     * Convert price to Y coordinate
     */
    priceToY(price) {
        const priceRange = this.priceRange.max - this.priceRange.min;
        const pricePercent = (price - this.priceRange.min) / priceRange;
        return this.height * (1 - pricePercent);
    }
    
    /**
     * Convert Y coordinate to price
     */
    yToPrice(y) {
        const priceRange = this.priceRange.max - this.priceRange.min;
        const pricePercent = 1 - (y / this.height);
        return this.priceRange.min + (priceRange * pricePercent);
    }
    
    /**
     * Handle mouse move for crosshair
     */
    handleMouseMove(event) {
        const rect = this.canvas.getBoundingClientRect();
        const x = event.clientX - rect.left;
        const y = event.clientY - rect.top;
        
        // Store mouse position for crosshair
        this.mouseX = x;
        this.mouseY = y;
        this.showCrosshair = true;
    }
    
    /**
     * Handle mouse leave
     */
    handleMouseLeave() {
        this.showCrosshair = false;
    }
    
    /**
     * Clear all data
     */
    clear() {
        this.data = [];
        this.priceRange = { min: 0, max: 0 };
        this.volumeRange = { min: 0, max: 0 };
    }
    
    /**
     * Update chart options
     */
    updateOptions(newOptions) {
        this.options = { ...this.options, ...newOptions };
    }
    
    /**
     * Get chart statistics
     */
    getStats() {
        if (this.data.length === 0) return null;
        
        const prices = this.data.map(d => d.price);
        const volumes = this.data.map(d => d.volume);
        
        return {
            dataPoints: this.data.length,
            priceRange: this.priceRange,
            volumeRange: this.volumeRange,
            currentPrice: this.data[this.data.length - 1].price,
            avgPrice: prices.reduce((a, b) => a + b, 0) / prices.length,
            totalVolume: volumes.reduce((a, b) => a + b, 0)
        };
    }
    
    /**
     * Destroy chart and cleanup
     */
    destroy() {
        this.stopAnimation();
        window.removeEventListener('resize', this.resize);
        this.canvas.removeEventListener('mousemove', this.handleMouseMove);
        this.canvas.removeEventListener('mouseleave', this.handleMouseLeave);
    }
}

/**
 * Multi-symbol chart manager
 */
class VFXChartManager {
    constructor() {
        this.charts = new Map();
        this.activeSymbol = null;
        
        console.log('📈 VFX Chart Manager initialized');
    }
    
    /**
     * Create chart for symbol
     */
    createChart(symbol, canvas, options = {}) {
        const chart = new VFXChart(canvas, options);
        this.charts.set(symbol, chart);
        
        if (!this.activeSymbol) {
            this.activeSymbol = symbol;
        }
        
        console.log(`📊 Created chart for ${symbol}`);
        return chart;
    }
    
    /**
     * Add data to specific symbol chart
     */
    addData(symbol, dataPoint) {
        const chart = this.charts.get(symbol);
        if (chart) {
            chart.addDataPoint(dataPoint);
        }
    }
    
    /**
     * Switch active symbol
     */
    setActiveSymbol(symbol) {
        if (this.charts.has(symbol)) {
            this.activeSymbol = symbol;
            console.log(`📊 Switched to ${symbol} chart`);
        }
    }
    
    /**
     * Get chart for symbol
     */
    getChart(symbol) {
        return this.charts.get(symbol);
    }
    
    /**
     * Remove chart
     */
    removeChart(symbol) {
        const chart = this.charts.get(symbol);
        if (chart) {
            chart.destroy();
            this.charts.delete(symbol);
            
            if (this.activeSymbol === symbol) {
                this.activeSymbol = this.charts.keys().next().value || null;
            }
        }
    }
    
    /**
     * Clear all charts
     */
    clearAll() {
        this.charts.forEach(chart => chart.clear());
    }
    
    /**
     * Destroy all charts
     */
    destroyAll() {
        this.charts.forEach(chart => chart.destroy());
        this.charts.clear();
        this.activeSymbol = null;
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { VFXChart, VFXChartManager };
} else {
    window.VFXChart = VFXChart;
    window.VFXChartManager = VFXChartManager;
}