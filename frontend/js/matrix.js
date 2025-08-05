/**
 * VFX Trading Platform - Matrix Background Effect
 * Cool Matrix-style digital rain animation
 */

class VFXMatrix {
    constructor(canvasId = 'matrix-bg') {
        this.canvas = document.getElementById(canvasId);
        this.ctx = null;
        this.animationId = null;
        this.isRunning = false;
        
        // Matrix settings
        this.settings = {
            fontSize: 14,
            columns: 0,
            drops: [],
            speed: 50, // Lower = faster
            opacity: 0.1,
            color: '#00ff41', // Classic Matrix green
            backgroundColor: 'rgba(0, 0, 0, 0.05)',
            characters: '01',
            enableTrading: true, // Show trading symbols
            tradingSymbols: ['BTC', 'ETH', 'USD', 'EUR', 'GBP', 'JPY', 'BUY', 'SELL', '▲', '▼', '●', '◆']
        };
        
        this.lastFrame = 0;
        this.init();
    }
    
    /**
     * Initialize matrix effect
     */
    init() {
        if (!this.canvas) {
            console.warn('Matrix canvas not found');
            return;
        }
        
        this.ctx = this.canvas.getContext('2d');
        this.setupCanvas();
        this.setupEventListeners();
        this.start();
        
        console.log('🎭 VFX Matrix effect initialized');
    }
    
    /**
     * Setup canvas and calculate columns
     */
    setupCanvas() {
        this.resizeCanvas();
        this.calculateColumns();
        this.initializeDrops();
    }
    
    /**
     * Resize canvas to full screen
     */
    resizeCanvas() {
        this.canvas.width = window.innerWidth;
        this.canvas.height = window.innerHeight;
        
        // Set canvas style
        this.canvas.style.position = 'fixed';
        this.canvas.style.top = '0';
        this.canvas.style.left = '0';
        this.canvas.style.width = '100%';
        this.canvas.style.height = '100%';
        this.canvas.style.zIndex = '-1';
        this.canvas.style.pointerEvents = 'none';
        this.canvas.style.opacity = this.settings.opacity;
    }
    
    /**
     * Calculate number of columns based on canvas width
     */
    calculateColumns() {
        this.settings.columns = Math.floor(this.canvas.width / this.settings.fontSize);
    }
    
    /**
     * Initialize drop positions
     */
    initializeDrops() {
        this.settings.drops = [];
        for (let i = 0; i < this.settings.columns; i++) {
            this.settings.drops[i] = Math.random() * this.canvas.height;
        }
    }
    
    /**
     * Setup event listeners
     */
    setupEventListeners() {
        // Handle window resize
        window.addEventListener('resize', () => {
            this.setupCanvas();
        });
        
        // Handle visibility change (pause when hidden)
        document.addEventListener('visibilitychange', () => {
            if (document.hidden) {
                this.pause();
            } else {
                this.resume();
            }
        });
    }
    
    /**
     * Get random character
     */
    getRandomCharacter() {
        let chars = this.settings.characters;
        
        // Add trading symbols occasionally
        if (this.settings.enableTrading && Math.random() < 0.1) {
            chars = this.settings.tradingSymbols.join('');
        }
        
        return chars.charAt(Math.floor(Math.random() * chars.length));
    }
    
    /**
     * Draw matrix frame
     */
    draw() {
        // Create fade effect
        this.ctx.fillStyle = this.settings.backgroundColor;
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
        
        // Set text properties
        this.ctx.fillStyle = this.settings.color;
        this.ctx.font = `${this.settings.fontSize}px monospace`;
        this.ctx.textAlign = 'center';
        
        // Draw falling characters
        for (let i = 0; i < this.settings.drops.length; i++) {
            const char = this.getRandomCharacter();
            const x = i * this.settings.fontSize;
            const y = this.settings.drops[i] * this.settings.fontSize;
            
            // Add glow effect for trading symbols
            if (this.settings.tradingSymbols.includes(char)) {
                this.ctx.shadowColor = this.settings.color;
                this.ctx.shadowBlur = 10;
            } else {
                this.ctx.shadowBlur = 0;
            }
            
            this.ctx.fillText(char, x, y);
            
            // Reset drop to top when it reaches bottom
            if (y > this.canvas.height && Math.random() > 0.975) {
                this.settings.drops[i] = 0;
            }
            
            // Move drop down
            this.settings.drops[i]++;
        }
    }
    
    /**
     * Animation loop
     */
    animate(currentTime) {
        if (!this.isRunning) return;
        
        // Control frame rate
        if (currentTime - this.lastFrame >= this.settings.speed) {
            this.draw();
            this.lastFrame = currentTime;
        }
        
        this.animationId = requestAnimationFrame((time) => this.animate(time));
    }
    
    /**
     * Start matrix animation
     */
    start() {
        if (this.isRunning) return;
        
        this.isRunning = true;
        this.lastFrame = 0;
        this.animate(0);
        
        console.log('🎭 Matrix animation started');
    }
    
    /**
     * Stop matrix animation
     */
    stop() {
        this.isRunning = false;
        if (this.animationId) {
            cancelAnimationFrame(this.animationId);
            this.animationId = null;
        }
        
        console.log('🎭 Matrix animation stopped');
    }
    
    /**
     * Pause animation
     */
    pause() {
        this.isRunning = false;
        if (this.animationId) {
            cancelAnimationFrame(this.animationId);
            this.animationId = null;
        }
    }
    
    /**
     * Resume animation
     */
    resume() {
        if (!this.isRunning && this.canvas) {
            this.start();
        }
    }
    
    /**
     * Update settings
     */
    updateSettings(newSettings) {
        this.settings = { ...this.settings, ...newSettings };
        
        // Recalculate if font size changed
        if (newSettings.fontSize) {
            this.calculateColumns();
            this.initializeDrops();
        }
        
        // Update canvas opacity
        if (newSettings.opacity !== undefined) {
            this.canvas.style.opacity = newSettings.opacity;
        }
    }
    
    /**
     * Set matrix intensity (opacity)
     */
    setIntensity(intensity) {
        this.updateSettings({ opacity: Math.max(0, Math.min(1, intensity)) });
    }
    
    /**
     * Set animation speed
     */
    setSpeed(speed) {
        this.updateSettings({ speed: Math.max(10, Math.min(200, speed)) });
    }
    
    /**
     * Toggle trading symbols
     */
    toggleTradingSymbols(enabled) {
        this.updateSettings({ enableTrading: enabled });
    }
    
    /**
     * Change matrix color
     */
    setColor(color) {
        this.updateSettings({ color });
    }
    
    /**
     * Add custom characters
     */
    addCharacters(chars) {
        this.settings.characters += chars;
    }
    
    /**
     * Reset to default characters
     */
    resetCharacters() {
        this.settings.characters = '01';
    }
    
    /**
     * Create special effect for trading events
     */
    tradingPulse(type = 'success') {
        const originalIntensity = this.settings.opacity;
        const originalColor = this.settings.color;
        
        // Color mapping for different events
        const colors = {
            success: '#00ff88',
            warning: '#ffdd00',
            error: '#ff4444',
            info: '#00aaff'
        };
        
        // Pulse effect
        this.setColor(colors[type] || colors.info);
        this.setIntensity(0.3);
        
        setTimeout(() => {
            this.setIntensity(0.2);
        }, 100);
        
        setTimeout(() => {
            this.setIntensity(0.1);
        }, 200);
        
        setTimeout(() => {
            this.setColor(originalColor);
            this.setIntensity(originalIntensity);
        }, 500);
    }
    
    /**
     * Create data stream effect
     */
    dataStream() {
        const originalChars = this.settings.characters;
        
        // Temporarily add more binary and trading symbols
        this.settings.characters = '01010101BTCETHUSDEURBUYSELL▲▼●◆';
        this.setSpeed(20); // Faster
        
        setTimeout(() => {
            this.settings.characters = originalChars;
            this.setSpeed(50); // Back to normal
        }, 2000);
    }
    
    /**
     * Get performance stats
     */
    getStats() {
        return {
            isRunning: this.isRunning,
            columns: this.settings.columns,
            canvasSize: {
                width: this.canvas.width,
                height: this.canvas.height
            },
            settings: { ...this.settings }
        };
    }
    
    /**
     * Destroy matrix effect
     */
    destroy() {
        this.stop();
        window.removeEventListener('resize', this.setupCanvas);
        document.removeEventListener('visibilitychange', this.pause);
        
        if (this.canvas) {
            this.canvas.style.display = 'none';
        }
        
        console.log('🎭 Matrix effect destroyed');
    }
}

/**
 * Matrix presets for different moods
 */
class VFXMatrixPresets {
    static presets = {
        subtle: {
            opacity: 0.05,
            speed: 80,
            color: '#00ff41',
            enableTrading: false
        },
        
        normal: {
            opacity: 0.1,
            speed: 50,
            color: '#00ff41',
            enableTrading: true
        },
        
        intense: {
            opacity: 0.2,
            speed: 30,
            color: '#00ff88',
            enableTrading: true
        },
        
        trading: {
            opacity: 0.15,
            speed: 40,
            color: '#00ff88',
            enableTrading: true,
            characters: '01BTCETHUSDEUR▲▼'
        },
        
        cyberpunk: {
            opacity: 0.25,
            speed: 25,
            color: '#ff0080',
            enableTrading: true,
            characters: '01ABCDEFGHIJKLMNOPQRSTUVWXYZ'
        },
        
        minimal: {
            opacity: 0.03,
            speed: 100,
            color: '#004400',
            enableTrading: false,
            characters: '01'
        }
    };
    
    static apply(matrix, presetName) {
        const preset = this.presets[presetName];
        if (preset) {
            matrix.updateSettings(preset);
            console.log(`🎭 Applied matrix preset: ${presetName}`);
        } else {
            console.warn(`Matrix preset not found: ${presetName}`);
        }
    }
    
    static list() {
        return Object.keys(this.presets);
    }
}

// Auto-initialize matrix effect when DOM is loaded
window.addEventListener('DOMContentLoaded', () => {
    // Check if matrix canvas exists
    const matrixCanvas = document.getElementById('matrix-bg');
    if (matrixCanvas) {
        window.vfxMatrix = new VFXMatrix();
        
        // Apply trading preset by default
        VFXMatrixPresets.apply(window.vfxMatrix, 'trading');
        
        // Add matrix control to global scope for debugging
        window.matrixPresets = VFXMatrixPresets;
    }
});

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { VFXMatrix, VFXMatrixPresets };
} else {
    window.VFXMatrix = VFXMatrix;
    window.VFXMatrixPresets = VFXMatrixPresets;
}