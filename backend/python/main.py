"""
VFX Trading Platform - Python Strategy Engine
High-performance quantitative trading strategies with Polars
"""

import polars as pl
import numpy as np
from typing import Dict, List, Optional, Tuple, Any
from dataclasses import dataclass
from enum import Enum
import asyncio
import websockets
import json
from datetime import datetime, timedelta
import logging
from abc import ABC, abstractmethod

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class Side(Enum):
    BUY = "BUY"
    SELL = "SELL"

class OrderType(Enum):
    MARKET = "MARKET"
    LIMIT = "LIMIT"

@dataclass
class Signal:
    symbol: str
    side: Side
    confidence: float
    target_price: float
    stop_loss: float
    take_profit: float
    quantity: float
    timestamp: datetime
    strategy_name: str
    metadata: Dict[str, Any] = None

@dataclass
class MarketData:
    symbol: str
    timestamp: datetime
    price: float
    volume: float
    bid: float
    ask: float

class BaseStrategy(ABC):
    """Base class for all trading strategies"""
    
    def __init__(self, name: str, symbols: List[str], config: Dict[str, Any] = None):
        self.name = name
        self.symbols = symbols
        self.config = config or {}
        self.position_sizes = {symbol: 0.0 for symbol in symbols}
        self.performance_metrics = {
            'total_trades': 0,
            'winning_trades': 0,
            'total_pnl': 0.0,
            'max_drawdown': 0.0,
            'sharpe_ratio': 0.0
        }
        
    @abstractmethod
    def generate_signal(self, data: pl.DataFrame) -> Optional[Signal]:
        """Generate trading signals based on market data"""
        pass
    
    @abstractmethod
    def update_data(self, market_data: MarketData) -> None:
        """Update strategy with new market data"""
        pass
    
    def calculate_position_size(self, signal: Signal, available_capital: float) -> float:
        """Calculate optimal position size based on risk management"""
        base_size = available_capital * self.config.get('position_fraction', 0.02)
        confidence_multiplier = signal.confidence ** 2
        return base_size * confidence_multiplier
    
    def should_exit_position(self, symbol: str, current_price: float) -> bool:
        """Determine if we should exit current position"""
        # Implement basic stop-loss and take-profit logic
        return False

class MomentumStrategy(BaseStrategy):
    """High-frequency momentum strategy using EMA crossovers"""
    
    def __init__(self, symbols: List[str], config: Dict[str, Any] = None):
        default_config = {
            'fast_ema': 12,
            'slow_ema': 26,
            'signal_ema': 9,
            'volume_threshold': 1000000,
            'min_confidence': 0.7,
            'lookback_periods': 100
        }
        if config:
            default_config.update(config)
        
        super().__init__("Momentum_Alpha", symbols, default_config)
        self.data_buffer = {}
        
    def update_data(self, market_data: MarketData) -> None:
        """Update strategy data buffer with new market data"""
        symbol = market_data.symbol
        
        # Initialize buffer if needed
        if symbol not in self.data_buffer:
            self.data_buffer[symbol] = pl.DataFrame({
                'timestamp': [],
                'price': [],
                'volume': [],
                'bid': [],
                'ask': []
            })
        
        # Add new data point
        new_row = pl.DataFrame({
            'timestamp': [market_data.timestamp],
            'price': [market_data.price],
            'volume': [market_data.volume],
            'bid': [market_data.bid],
            'ask': [market_data.ask]
        })
        
        self.data_buffer[symbol] = pl.concat([self.data_buffer[symbol], new_row])
        
        # Keep only recent data for performance
        max_rows = self.config['lookback_periods']
        if len(self.data_buffer[symbol]) > max_rows:
            self.data_buffer[symbol] = self.data_buffer[symbol].tail(max_rows)
    
    def generate_signal(self, data: pl.DataFrame) -> Optional[Signal]:
        """Generate momentum signals using EMA crossovers"""
        if len(data) < self.config['slow_ema']:
            return None
        
        # Calculate EMAs using Polars
        df = data.with_columns([
            pl.col('price').ewm_mean(span=self.config['fast_ema']).alias('fast_ema'),
            pl.col('price').ewm_mean(span=self.config['slow_ema']).alias('slow_ema'),
            pl.col('volume').mean().alias('avg_volume')
        ])
        
        # Calculate MACD
        df = df.with_columns([
            (pl.col('fast_ema') - pl.col('slow_ema')).alias('macd'),
        ])
        
        df = df.with_columns([
            pl.col('macd').ewm_mean(span=self.config['signal_ema']).alias('signal_line')
        ])
        
        # Get latest values
        latest = df.tail(1).to_dicts()[0]
        previous = df.tail(2).head(1).to_dicts()[0] if len(df) >= 2 else latest
        
        # Check for crossover
        macd_current = latest['macd']
        macd_previous = previous['macd']
        signal_current = latest['signal_line']
        signal_previous = previous['signal_line']
        
        # Volume filter
        if latest['avg_volume'] < self.config['volume_threshold']:
            return None
        
        # Generate signal
        symbol = latest.get('symbol', self.symbols[0])
        current_price = latest['price']
        
        # Bullish crossover
        if (macd_current > signal_current and 
            macd_previous <= signal_previous and
            macd_current > 0):
            
            confidence = min(abs(macd_current - signal_current) / current_price * 100, 1.0)
            
            if confidence >= self.config['min_confidence']:
                return Signal(
                    symbol=symbol,
                    side=Side.BUY,
                    confidence=confidence,
                    target_price=current_price * 1.02,  # 2% target
                    stop_loss=current_price * 0.99,    # 1% stop loss
                    take_profit=current_price * 1.03,  # 3% take profit
                    quantity=self.calculate_position_size_internal(confidence),
                    timestamp=datetime.now(),
                    strategy_name=self.name,
                    metadata={'macd': macd_current, 'signal': signal_current}
                )
        
        # Bearish crossover
        elif (macd_current < signal_current and 
              macd_previous >= signal_previous and
              macd_current < 0):
            
            confidence = min(abs(signal_current - macd_current) / current_price * 100, 1.0)
            
            if confidence >= self.config['min_confidence']:
                return Signal(
                    symbol=symbol,
                    side=Side.SELL,
                    confidence=confidence,
                    target_price=current_price * 0.98,  # 2% target
                    stop_loss=current_price * 1.01,    # 1% stop loss
                    take_profit=current_price * 0.97,  # 3% take profit
                    quantity=self.calculate_position_size_internal(confidence),
                    timestamp=datetime.now(),
                    strategy_name=self.name,
                    metadata={'macd': macd_current, 'signal': signal_current}
                )
        
        return None
    
    def calculate_position_size_internal(self, confidence: float) -> float:
        """Internal position sizing for momentum strategy"""
        base_size = 1000.0  # Base position size
        return base_size * confidence * self.config.get('leverage', 1.0)

class MeanReversionStrategy(BaseStrategy):
    """Mean reversion strategy using Bollinger Bands and RSI"""
    
    def __init__(self, symbols: List[str], config: Dict[str, Any] = None):
        default_config = {
            'bb_period': 20,
            'bb_std': 2.0,
            'rsi_period': 14,
            'rsi_oversold': 30,
            'rsi_overbought': 70,
            'min_confidence': 0.6,
            'lookback_periods': 100
        }
        if config:
            default_config.update(config)
        
        super().__init__("Mean_Reversion", symbols, default_config)
        self.data_buffer = {}
    
    def update_data(self, market_data: MarketData) -> None:
        """Update strategy data buffer"""
        symbol = market_data.symbol
        
        if symbol not in self.data_buffer:
            self.data_buffer[symbol] = pl.DataFrame({
                'timestamp': [],
                'price': [],
                'volume': [],
                'high': [],
                'low': [],
                'close': []
            })
        
        # Simulate OHLC from current price
        new_row = pl.DataFrame({
            'timestamp': [market_data.timestamp],
            'price': [market_data.price],
            'volume': [market_data.volume],
            'high': [market_data.price * 1.001],
            'low': [market_data.price * 0.999],
            'close': [market_data.price]
        })
        
        self.data_buffer[symbol] = pl.concat([self.data_buffer[symbol], new_row])
        
        if len(self.data_buffer[symbol]) > self.config['lookback_periods']:
            self.data_buffer[symbol] = self.data_buffer[symbol].tail(self.config['lookback_periods'])
    
    def generate_signal(self, data: pl.DataFrame) -> Optional[Signal]:
        """Generate mean reversion signals"""
        if len(data) < max(self.config['bb_period'], self.config['rsi_period']):
            return None
        
        # Calculate Bollinger Bands
        df = data.with_columns([
            pl.col('close').rolling_mean(self.config['bb_period']).alias('bb_middle'),
            pl.col('close').rolling_std(self.config['bb_period']).alias('bb_std')
        ])
        
        df = df.with_columns([
            (pl.col('bb_middle') + pl.col('bb_std') * self.config['bb_std']).alias('bb_upper'),
            (pl.col('bb_middle') - pl.col('bb_std') * self.config['bb_std']).alias('bb_lower')
        ])
        
        # Calculate RSI
        df = df.with_columns([
            pl.col('close').diff().alias('price_change')
        ])
        
        df = df.with_columns([
            pl.when(pl.col('price_change') > 0).then(pl.col('price_change')).otherwise(0).alias('gain'),
            pl.when(pl.col('price_change') < 0).then(-pl.col('price_change')).otherwise(0).alias('loss')
        ])
        
        df = df.with_columns([
            pl.col('gain').rolling_mean(self.config['rsi_period']).alias('avg_gain'),
            pl.col('loss').rolling_mean(self.config['rsi_period']).alias('avg_loss')
        ])
        
        df = df.with_columns([
            (100 - (100 / (1 + pl.col('avg_gain') / pl.col('avg_loss')))).alias('rsi')
        ])
        
        # Get latest values
        latest = df.tail(1).to_dicts()[0]
        current_price = latest['close']
        bb_upper = latest['bb_upper']
        bb_lower = latest['bb_lower']
        rsi = latest['rsi']
        
        symbol = latest.get('symbol', self.symbols[0])
        
        # Oversold condition (Buy signal)
        if (current_price <= bb_lower and 
            rsi <= self.config['rsi_oversold']):
            
            confidence = min((self.config['rsi_oversold'] - rsi) / 30.0 + 
                           (bb_lower - current_price) / current_price, 1.0)
            
            if confidence >= self.config['min_confidence']:
                return Signal(
                    symbol=symbol,
                    side=Side.BUY,
                    confidence=confidence,
                    target_price=latest['bb_middle'],
                    stop_loss=current_price * 0.98,
                    take_profit=current_price * 1.04,
                    quantity=self.calculate_position_size_internal(confidence),
                    timestamp=datetime.now(),
                    strategy_name=self.name,
                    metadata={'rsi': rsi, 'bb_position': 'lower'}
                )
        
        # Overbought condition (Sell signal)
        elif (current_price >= bb_upper and 
              rsi >= self.config['rsi_overbought']):
            
            confidence = min((rsi - self.config['rsi_overbought']) / 30.0 + 
                           (current_price - bb_upper) / current_price, 1.0)
            
            if confidence >= self.config['min_confidence']:
                return Signal(
                    symbol=symbol,
                    side=Side.SELL,
                    confidence=confidence,
                    target_price=latest['bb_middle'],
                    stop_loss=current_price * 1.02,
                    take_profit=current_price * 0.96,
                    quantity=self.calculate_position_size_internal(confidence),
                    timestamp=datetime.now(),
                    strategy_name=self.name,
                    metadata={'rsi': rsi, 'bb_position': 'upper'}
                )
        
        return None
    
    def calculate_position_size_internal(self, confidence: float) -> float:
        """Position sizing for mean reversion"""
        base_size = 800.0
        return base_size * confidence * 0.8  # More conservative

class ArbitrageStrategy(BaseStrategy):
    """Statistical arbitrage strategy"""
    
    def __init__(self, symbols: List[str], config: Dict[str, Any] = None):
        default_config = {
            'correlation_window': 60,
            'zscore_threshold': 2.0,
            'lookback_periods': 200,
            'min_correlation': 0.7
        }
        if config:
            default_config.update(config)
        
        super().__init__("Statistical_Arbitrage", symbols, default_config)
        self.data_buffer = {}
        self.pairs = []
        self._initialize_pairs()
    
    def _initialize_pairs(self):
        """Initialize trading pairs for arbitrage"""
        # Example pairs - in real implementation, find correlated pairs
        if len(self.symbols) >= 2:
            self.pairs = [(self.symbols[i], self.symbols[j]) 
                         for i in range(len(self.symbols)) 
                         for j in range(i+1, len(self.symbols))]
    
    def update_data(self, market_data: MarketData) -> None:
        """Update data buffer for arbitrage analysis"""
        symbol = market_data.symbol
        
        if symbol not in self.data_buffer:
            self.data_buffer[symbol] = pl.DataFrame({
                'timestamp': [],
                'price': [],
                'log_price': []
            })
        
        log_price = np.log(market_data.price)
        new_row = pl.DataFrame({
            'timestamp': [market_data.timestamp],
            'price': [market_data.price],
            'log_price': [log_price]
        })
        
        self.data_buffer[symbol] = pl.concat([self.data_buffer[symbol], new_row])
        
        if len(self.data_buffer[symbol]) > self.config['lookback_periods']:
            self.data_buffer[symbol] = self.data_buffer[symbol].tail(self.config['lookback_periods'])
    
    def generate_signal(self, data: pl.DataFrame) -> Optional[Signal]:
        """Generate arbitrage signals based on pair relationships"""
        if len(self.pairs) == 0:
            return None
        
        for pair in self.pairs:
            symbol1, symbol2 = pair
            
            if (symbol1 not in self.data_buffer or 
                symbol2 not in self.data_buffer):
                continue
            
            df1 = self.data_buffer[symbol1]
            df2 = self.data_buffer[symbol2]
            
            if (len(df1) < self.config['correlation_window'] or 
                len(df2) < self.config['correlation_window']):
                continue
            
            # Calculate spread and z-score
            recent_df1 = df1.tail(self.config['correlation_window'])
            recent_df2 = df2.tail(self.config['correlation_window'])
            
            if len(recent_df1) != len(recent_df2):
                continue
            
            # Merge dataframes on timestamp for alignment
            merged = recent_df1.join(recent_df2, on='timestamp', suffix='_2')
            
            if len(merged) < self.config['correlation_window']:
                continue
            
            # Calculate correlation
            correlation = merged.select([
                pl.corr('log_price', 'log_price_2').alias('correlation')
            ]).item()
            
            if abs(correlation) < self.config['min_correlation']:
                continue
            
            # Calculate spread and z-score
            merged = merged.with_columns([
                (pl.col('log_price') - pl.col('log_price_2')).alias('spread')
            ])
            
            spread_mean = merged['spread'].mean()
            spread_std = merged['spread'].std()
            current_spread = merged['spread'].tail(1).item()
            
            if spread_std == 0:
                continue
            
            zscore = (current_spread - spread_mean) / spread_std
            
            # Generate signals based on z-score
            if abs(zscore) > self.config['zscore_threshold']:
                confidence = min(abs(zscore) / 4.0, 1.0)  # Normalize confidence
                
                if zscore > self.config['zscore_threshold']:
                    # Spread too high, sell symbol1, buy symbol2
                    return Signal(
                        symbol=symbol1,
                        side=Side.SELL,
                        confidence=confidence,
                        target_price=recent_df1['price'].tail(1).item() * 0.99,
                        stop_loss=recent_df1['price'].tail(1).item() * 1.01,
                        take_profit=recent_df1['price'].tail(1).item() * 0.98,
                        quantity=500.0 * confidence,
                        timestamp=datetime.now(),
                        strategy_name=self.name,
                        metadata={
                            'pair': f"{symbol1}/{symbol2}",
                            'zscore': zscore,
                            'correlation': correlation
                        }
                    )
                elif zscore < -self.config['zscore_threshold']:
                    # Spread too low, buy symbol1, sell symbol2
                    return Signal(
                        symbol=symbol1,
                        side=Side.BUY,
                        confidence=confidence,
                        target_price=recent_df1['price'].tail(1).item() * 1.01,
                        stop_loss=recent_df1['price'].tail(1).item() * 0.99,
                        take_profit=recent_df1['price'].tail(1).item() * 1.02,
                        quantity=500.0 * confidence,
                        timestamp=datetime.now(),
                        strategy_name=self.name,
                        metadata={
                            'pair': f"{symbol1}/{symbol2}",
                            'zscore': zscore,
                            'correlation': correlation
                        }
                    )
        
        return None

class StrategyManager:
    """Manages multiple trading strategies and coordinates execution"""
    
    def __init__(self, cpp_engine_url: str = "ws://localhost:8080"):
        self.strategies: Dict[str, BaseStrategy] = {}
        self.active_strategies: List[str] = []
        self.cpp_engine_url = cpp_engine_url
        self.websocket = None
        self.running = False
        
    def add_strategy(self, strategy: BaseStrategy) -> None:
        """Add a trading strategy"""
        self.strategies[strategy.name] = strategy
        logger.info(f"Added strategy: {strategy.name}")
    
    def activate_strategy(self, strategy_name: str) -> None:
        """Activate a strategy for live trading"""
        if strategy_name in self.strategies:
            if strategy_name not in self.active_strategies:
                self.active_strategies.append(strategy_name)
                logger.info(f"Activated strategy: {strategy_name}")
        else:
            logger.error(f"Strategy not found: {strategy_name}")
    
    def deactivate_strategy(self, strategy_name: str) -> None:
        """Deactivate a strategy"""
        if strategy_name in self.active_strategies:
            self.active_strategies.remove(strategy_name)
            logger.info(f"Deactivated strategy: {strategy_name}")
    
    async def connect_to_cpp_engine(self) -> None:
        """Connect to C++ trading engine via WebSocket"""
        try:
            self.websocket = await websockets.connect(self.cpp_engine_url)
            logger.info("Connected to C++ trading engine")
        except Exception as e:
            logger.error(f"Failed to connect to C++ engine: {e}")
            raise
    
    async def send_order_to_engine(self, signal: Signal) -> None:
        """Send trading signal to C++ engine"""
        if not self.websocket:
            logger.error("Not connected to C++ engine")
            return
        
        order_message = {
            'type': 'order',
            'symbol': signal.symbol,
            'side': signal.side.value,
            'quantity': signal.quantity,
            'order_type': 'MARKET',
            'metadata': {
                'strategy': signal.strategy_name,
                'confidence': signal.confidence,
                'timestamp': signal.timestamp.isoformat()
            }
        }
        
        try:
            await self.websocket.send(json.dumps(order_message))
            logger.info(f"Sent order: {signal.symbol} {signal.side.value} {signal.quantity}")
        except Exception as e:
            logger.error(f"Failed to send order: {e}")
    
    async def process_market_data(self, market_data: MarketData) -> None:
        """Process incoming market data and generate signals"""
        signals = []
        
        for strategy_name in self.active_strategies:
            strategy = self.strategies[strategy_name]
            
            # Update strategy with new data
            strategy.update_data(market_data)
            
            # Generate signal if enough data
            if market_data.symbol in strategy.data_buffer:
                data = strategy.data_buffer[market_data.symbol]
                signal = strategy.generate_signal(data)
                
                if signal:
                    signals.append(signal)
                    logger.info(f"Generated signal: {signal.strategy_name} - {signal.symbol} {signal.side.value}")
        
        # Execute signals
        for signal in signals:
            await self.send_order_to_engine(signal)
    
    async def start(self) -> None:
        """Start the strategy manager"""
        await self.connect_to_cpp_engine()
        self.running = True
        logger.info("Strategy manager started")
        
        # Start listening for market data and engine responses
        async for message in self.websocket:
            try:
                data = json.loads(message)
                
                if data.get('type') == 'market_data':
                    market_data = MarketData(
                        symbol=data['symbol'],
                        timestamp=datetime.fromisoformat(data['timestamp']),
                        price=data['price'],
                        volume=data['volume'],
                        bid=data['bid'],
                        ask=data['ask']
                    )
                    await self.process_market_data(market_data)
                
                elif data.get('type') == 'execution':
                    logger.info(f"Execution confirmed: {data}")
                
            except Exception as e:
                logger.error(f"Error processing message: {e}")
    
    def stop(self) -> None:
        """Stop the strategy manager"""
        self.running = False
        if self.websocket:
            asyncio.create_task(self.websocket.close())
        logger.info("Strategy manager stopped")

# Example usage and configuration
async def main():
    """Main entry point for the strategy engine"""
    
    # Initialize strategies
    symbols = ['BTC/USD', 'ETH/USD', 'AAPL', 'TSLA', 'NVDA']
    
    momentum_strategy = MomentumStrategy(
        symbols=symbols,
        config={
            'fast_ema': 12,
            'slow_ema': 26,
            'signal_ema': 9,
            'min_confidence': 0.75,
            'leverage': 2.0
        }
    )
    
    mean_reversion_strategy = MeanReversionStrategy(
        symbols=symbols,
        config={
            'bb_period': 20,
            'rsi_period': 14,
            'min_confidence': 0.65
        }
    )
    
    arbitrage_strategy = ArbitrageStrategy(
        symbols=symbols,
        config={
            'zscore_threshold': 2.5,
            'min_correlation': 0.8
        }
    )
    
    # Initialize strategy manager
    manager = StrategyManager()
    
    # Add strategies
    manager.add_strategy(momentum_strategy)
    manager.add_strategy(mean_reversion_strategy)
    manager.add_strategy(arbitrage_strategy)
    
    # Activate strategies
    manager.activate_strategy("Momentum_Alpha")
    manager.activate_strategy("Mean_Reversion")
    # manager.activate_strategy("Statistical_Arbitrage")  # Activate as needed
    
    # Start trading
    try:
        await manager.start()
    except KeyboardInterrupt:
        logger.info("Shutting down strategy engine...")
        manager.stop()
    except Exception as e:
        logger.error(f"Strategy engine error: {e}")
        manager.stop()

if __name__ == "__main__":
    asyncio.run(main())