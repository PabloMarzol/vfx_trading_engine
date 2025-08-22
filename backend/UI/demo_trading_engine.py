"""
VFX Demo Trading Engine
Real-time paper trading simulation using ALT5 price data
Supports multi-instrument portfolio tracking with full risk management
"""

import asyncio
import json
import time
import uuid
from datetime import datetime, timezone
from decimal import Decimal, ROUND_HALF_UP
from typing import Dict, List, Optional, Any, Callable
from dataclasses import dataclass, asdict, field
from enum import Enum
import logging

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class OrderType(Enum):
    MARKET = "market"
    LIMIT = "limit"
    STOP_LOSS = "stop_loss"
    TAKE_PROFIT = "take_profit"

class OrderSide(Enum):
    BUY = "buy"
    SELL = "sell"

class OrderStatus(Enum):
    PENDING = "pending"
    FILLED = "filled"
    PARTIAL = "partial"
    CANCELLED = "cancelled"
    REJECTED = "rejected"

class TimeInForce(Enum):
    GTC = "good_till_cancelled"
    DAY = "day"
    IOC = "immediate_or_cancel"
    FOK = "fill_or_kill"

@dataclass
class MarketTick:
    instrument: str
    bid: float
    ask: float
    last: float
    timestamp: datetime
    volume: float = 0.0
    
    @property
    def mid_price(self) -> float:
        return (self.bid + self.ask) / 2

@dataclass
class Order:
    order_id: str
    instrument: str
    side: OrderSide
    order_type: OrderType
    quantity: Decimal
    price: Optional[Decimal] = None
    stop_price: Optional[Decimal] = None
    status: OrderStatus = OrderStatus.PENDING
    time_in_force: TimeInForce = TimeInForce.GTC
    created_at: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
    filled_quantity: Decimal = Decimal('0')
    avg_fill_price: Optional[Decimal] = None
    commission: Decimal = Decimal('0')
    parent_order_id: Optional[str] = None  # For stop-loss/take-profit orders
    
    @property
    def remaining_quantity(self) -> Decimal:
        return self.quantity - self.filled_quantity
    
    @property
    def is_filled(self) -> bool:
        return self.status == OrderStatus.FILLED
    
    @property
    def is_active(self) -> bool:
        return self.status in [OrderStatus.PENDING, OrderStatus.PARTIAL]

@dataclass
class Trade:
    trade_id: str
    order_id: str
    instrument: str
    side: OrderSide
    quantity: Decimal
    price: Decimal
    commission: Decimal
    timestamp: datetime = field(default_factory=lambda: datetime.now(timezone.utc))

@dataclass
class Position:
    instrument: str
    quantity: Decimal = Decimal('0')
    avg_price: Decimal = Decimal('0')
    unrealized_pnl: Decimal = Decimal('0')
    realized_pnl: Decimal = Decimal('0')
    total_commission: Decimal = Decimal('0')
    market_value: Decimal = Decimal('0')
    last_update: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
    
    @property
    def is_long(self) -> bool:
        return self.quantity > 0
    
    @property
    def is_short(self) -> bool:
        return self.quantity < 0
    
    @property
    def is_flat(self) -> bool:
        return self.quantity == 0
    
    def update_market_value(self, current_price: Decimal):
        self.market_value = abs(self.quantity) * current_price
        if not self.is_flat:
            self.unrealized_pnl = (current_price - self.avg_price) * self.quantity
        else:
            self.unrealized_pnl = Decimal('0')
        self.last_update = datetime.now(timezone.utc)

@dataclass
class RiskLimits:
    max_daily_loss: Decimal = Decimal('5000')  # $5,000 default
    max_position_size: Decimal = Decimal('50000')  # $50,000 per position
    max_leverage: Decimal = Decimal('4')  # 4:1 leverage
    allowed_instruments: List[str] = field(default_factory=list)

@dataclass
class CommissionStructure:
    stock_per_share: Decimal = Decimal('0.005')  # $0.005 per share
    forex_per_lot: Decimal = Decimal('4.50')     # $4.50 per standard lot
    crypto_percentage: Decimal = Decimal('0.001')  # 0.1%
    minimum_commission: Decimal = Decimal('1.00')  # $1.00 minimum

class DemoTradingEngine:
    """
    Advanced demo trading engine with real-time ALT5 data integration
    Supports full portfolio management with risk controls
    """
    
    def __init__(self, 
                 starting_balance: Decimal = Decimal('100000'),
                 commission_structure: Optional[CommissionStructure] = None,
                 risk_limits: Optional[RiskLimits] = None):
        
        # Account state
        self.account_id = str(uuid.uuid4())
        self.starting_balance = starting_balance
        self.cash_balance = starting_balance
        self.equity = starting_balance
        self.margin_used = Decimal('0')
        self.margin_available = starting_balance
        
        # Trading data
        self.orders: Dict[str, Order] = {}
        self.trades: List[Trade] = []
        self.positions: Dict[str, Position] = {}
        self.market_data: Dict[str, MarketTick] = {}
        
        # Configuration
        self.commission_structure = commission_structure or CommissionStructure()
        self.risk_limits = risk_limits or RiskLimits()
        
        # State tracking
        self.daily_pnl = Decimal('0')
        self.total_pnl = Decimal('0')
        self.daily_trades = 0
        self.total_trades = 0
        self.is_running = False
        
        # Event callbacks
        self.on_order_update: Optional[Callable] = None
        self.on_trade_executed: Optional[Callable] = None
        self.on_position_update: Optional[Callable] = None
        self.on_account_update: Optional[Callable] = None
        
        logger.info(f"🎯 Demo Trading Engine initialized - Account: {self.account_id[:8]}")
        logger.info(f"💰 Starting Balance: ${self.starting_balance:,.2f}")
    
    def connect_alt5_feed(self, alt5_data_callback: Callable):
        """Connect to ALT5 real-time price feed"""
        self.alt5_callback = alt5_data_callback
        logger.info("📡 Connected to ALT5 real-time data feed")
    
    async def process_market_data(self, tick: MarketTick):
        """Process incoming market data from ALT5"""
        self.market_data[tick.instrument] = tick
        
        # Update position values if we have a position in this instrument
        if tick.instrument in self.positions:
            position = self.positions[tick.instrument]
            position.update_market_value(Decimal(str(tick.last)))
            
            # Trigger position update callback
            if self.on_position_update:
                await self.on_position_update(position)
        
        # Check for pending orders that might be triggered
        await self._check_pending_orders(tick)
        
        # Update account equity
        await self._update_account_equity()
    
    async def place_order(self, 
                         instrument: str,
                         side: OrderSide,
                         order_type: OrderType,
                         quantity: Decimal,
                         price: Optional[Decimal] = None,
                         stop_price: Optional[Decimal] = None,
                         time_in_force: TimeInForce = TimeInForce.GTC,
                         stop_loss: Optional[Decimal] = None,
                         take_profit: Optional[Decimal] = None) -> str:
        """Place a new order with optional stop-loss and take-profit"""
        
        # Validate order
        validation_error = await self._validate_order(instrument, side, order_type, quantity, price)
        if validation_error:
            logger.error(f"❌ Order validation failed: {validation_error}")
            raise ValueError(validation_error)
        
        # Create primary order
        order_id = str(uuid.uuid4())
        order = Order(
            order_id=order_id,
            instrument=instrument,
            side=side,
            order_type=order_type,
            quantity=quantity,
            price=price,
            stop_price=stop_price,
            time_in_force=time_in_force
        )
        
        self.orders[order_id] = order
        
        # Handle market orders immediately
        if order_type == OrderType.MARKET:
            await self._execute_market_order(order)
        
        # Create stop-loss order if specified
        if stop_loss and order.is_filled:
            await self._create_stop_loss_order(order, stop_loss)
        
        # Create take-profit order if specified  
        if take_profit and order.is_filled:
            await self._create_take_profit_order(order, take_profit)
        
        # Trigger callback
        if self.on_order_update:
            await self.on_order_update(order)
        
        logger.info(f"📝 Order placed: {order_id[:8]} - {side.value.upper()} {quantity} {instrument}")
        return order_id
    
    async def cancel_order(self, order_id: str) -> bool:
        """Cancel a pending order"""
        if order_id not in self.orders:
            return False
        
        order = self.orders[order_id]
        if not order.is_active:
            return False
        
        order.status = OrderStatus.CANCELLED
        
        # Trigger callback
        if self.on_order_update:
            await self.on_order_update(order)
        
        logger.info(f"❌ Order cancelled: {order_id[:8]}")
        return True
    
    async def _validate_order(self, instrument: str, side: OrderSide, 
                            order_type: OrderType, quantity: Decimal, 
                            price: Optional[Decimal]) -> Optional[str]:
        """Validate order against risk limits and account balance"""
        
        # Check if instrument is allowed
        if (self.risk_limits.allowed_instruments and 
            instrument not in self.risk_limits.allowed_instruments):
            return f"Instrument {instrument} not allowed"
        
        # Get current market price
        if instrument not in self.market_data:
            return f"No market data available for {instrument}"
        
        tick = self.market_data[instrument]
        estimated_price = price or Decimal(str(tick.ask if side == OrderSide.BUY else tick.bid))
        position_value = quantity * estimated_price
        
        # Check position size limit
        if position_value > self.risk_limits.max_position_size:
            return f"Position size ${position_value} exceeds limit ${self.risk_limits.max_position_size}"
        
        # Check available margin for buy orders
        if side == OrderSide.BUY:
            required_margin = position_value / self.risk_limits.max_leverage
            if required_margin > self.margin_available:
                return f"Insufficient margin: required ${required_margin}, available ${self.margin_available}"
        
        # Check daily loss limit
        if self.daily_pnl < -self.risk_limits.max_daily_loss:
            return f"Daily loss limit exceeded: ${abs(self.daily_pnl)}"
        
        return None
    
    async def _execute_market_order(self, order: Order):
        """Execute market order immediately"""
        if order.instrument not in self.market_data:
            order.status = OrderStatus.REJECTED
            return
        
        tick = self.market_data[order.instrument]
        execution_price = Decimal(str(tick.ask if order.side == OrderSide.BUY else tick.bid))
        
        # Calculate commission
        commission = self._calculate_commission(order.instrument, order.quantity, execution_price)
        
        # Execute the trade
        await self._execute_trade(order, execution_price, order.quantity, commission)
    
    async def _execute_trade(self, order: Order, price: Decimal, 
                           quantity: Decimal, commission: Decimal):
        """Execute a trade and update positions"""
        
        # Create trade record
        trade_id = str(uuid.uuid4())
        trade = Trade(
            trade_id=trade_id,
            order_id=order.order_id,
            instrument=order.instrument,
            side=order.side,
            quantity=quantity,
            price=price,
            commission=commission
        )
        
        self.trades.append(trade)
        
        # Update order
        order.filled_quantity += quantity
        order.avg_fill_price = price
        order.commission += commission
        order.status = OrderStatus.FILLED if order.remaining_quantity == 0 else OrderStatus.PARTIAL
        
        # Update position
        await self._update_position(trade)
        
        # Update cash balance
        trade_value = quantity * price
        if order.side == OrderSide.BUY:
            self.cash_balance -= trade_value + commission
        else:
            self.cash_balance += trade_value - commission
        
        # Update statistics
        self.daily_trades += 1
        self.total_trades += 1
        
        # Trigger callbacks
        if self.on_trade_executed:
            await self.on_trade_executed(trade)
        if self.on_order_update:
            await self.on_order_update(order)
        
        logger.info(f"✅ Trade executed: {trade_id[:8]} - {order.side.value.upper()} {quantity} {order.instrument} @ ${price}")
    
    async def _update_position(self, trade: Trade):
        """Update position based on executed trade"""
        instrument = trade.instrument
        
        if instrument not in self.positions:
            self.positions[instrument] = Position(instrument=instrument)
        
        position = self.positions[instrument]
        
        # Calculate new position
        if trade.side == OrderSide.BUY:
            new_quantity = position.quantity + trade.quantity
            if position.quantity >= 0:  # Adding to long or creating long
                total_cost = (position.quantity * position.avg_price) + (trade.quantity * trade.price)
                position.avg_price = total_cost / new_quantity if new_quantity > 0 else Decimal('0')
            else:  # Covering short
                if new_quantity <= 0:  # Still short or flat
                    realized_pnl = trade.quantity * (position.avg_price - trade.price)
                    position.realized_pnl += realized_pnl
                    self.daily_pnl += realized_pnl
                    self.total_pnl += realized_pnl
                else:  # Flipped to long
                    cover_quantity = abs(position.quantity)
                    realized_pnl = cover_quantity * (position.avg_price - trade.price)
                    position.realized_pnl += realized_pnl
                    self.daily_pnl += realized_pnl
                    self.total_pnl += realized_pnl
                    position.avg_price = trade.price
            position.quantity = new_quantity
        
        else:  # SELL
            new_quantity = position.quantity - trade.quantity
            if position.quantity <= 0:  # Adding to short or creating short
                total_cost = (abs(position.quantity) * position.avg_price) + (trade.quantity * trade.price)
                position.avg_price = total_cost / abs(new_quantity) if new_quantity < 0 else Decimal('0')
            else:  # Selling long
                if new_quantity >= 0:  # Still long or flat
                    realized_pnl = trade.quantity * (trade.price - position.avg_price)
                    position.realized_pnl += realized_pnl
                    self.daily_pnl += realized_pnl
                    self.total_pnl += realized_pnl
                else:  # Flipped to short
                    sell_quantity = position.quantity
                    realized_pnl = sell_quantity * (trade.price - position.avg_price)
                    position.realized_pnl += realized_pnl
                    self.daily_pnl += realized_pnl
                    self.total_pnl += realized_pnl
                    position.avg_price = trade.price
            position.quantity = new_quantity
        
        # Update commission
        position.total_commission += trade.commission
        
        # Update market value with current price
        if instrument in self.market_data:
            current_price = Decimal(str(self.market_data[instrument].last))
            position.update_market_value(current_price)
        
        # Trigger callback
        if self.on_position_update:
            await self.on_position_update(position)
    
    def _calculate_commission(self, instrument: str, quantity: Decimal, price: Decimal) -> Decimal:
        """Calculate commission based on instrument type"""
        # Simple commission calculation - can be enhanced based on instrument type
        if 'USD' in instrument.upper() or 'EUR' in instrument.upper():  # FX
            lots = quantity / Decimal('100000')  # Standard lot size
            commission = lots * self.commission_structure.forex_per_lot
        elif any(crypto in instrument.upper() for crypto in ['BTC', 'ETH', 'ADA', 'SOL']):  # Crypto
            trade_value = quantity * price
            commission = trade_value * self.commission_structure.crypto_percentage
        else:  # Stocks
            commission = quantity * self.commission_structure.stock_per_share
        
        return max(commission, self.commission_structure.minimum_commission)
    
    async def _check_pending_orders(self, tick: MarketTick):
        """Check if any pending orders should be triggered"""
        for order in self.orders.values():
            if not order.is_active or order.instrument != tick.instrument:
                continue
            
            should_execute = False
            execution_price = None
            
            if order.order_type == OrderType.LIMIT:
                if order.side == OrderSide.BUY and tick.ask <= order.price:
                    should_execute = True
                    execution_price = order.price
                elif order.side == OrderSide.SELL and tick.bid >= order.price:
                    should_execute = True
                    execution_price = order.price
            
            elif order.order_type == OrderType.STOP_LOSS:
                if order.side == OrderSide.SELL and tick.last <= order.stop_price:
                    should_execute = True
                    execution_price = Decimal(str(tick.bid))
                elif order.side == OrderSide.BUY and tick.last >= order.stop_price:
                    should_execute = True
                    execution_price = Decimal(str(tick.ask))
            
            elif order.order_type == OrderType.TAKE_PROFIT:
                if order.side == OrderSide.SELL and tick.last >= order.stop_price:
                    should_execute = True
                    execution_price = Decimal(str(tick.bid))
                elif order.side == OrderSide.BUY and tick.last <= order.stop_price:
                    should_execute = True
                    execution_price = Decimal(str(tick.ask))
            
            if should_execute and execution_price:
                commission = self._calculate_commission(order.instrument, order.quantity, execution_price)
                await self._execute_trade(order, execution_price, order.quantity, commission)
    
    async def _create_stop_loss_order(self, parent_order: Order, stop_loss_price: Decimal):
        """Create stop-loss order for filled position"""
        opposite_side = OrderSide.SELL if parent_order.side == OrderSide.BUY else OrderSide.BUY
        
        stop_order_id = str(uuid.uuid4())
        stop_order = Order(
            order_id=stop_order_id,
            instrument=parent_order.instrument,
            side=opposite_side,
            order_type=OrderType.STOP_LOSS,
            quantity=parent_order.filled_quantity,
            stop_price=stop_loss_price,
            parent_order_id=parent_order.order_id
        )
        
        self.orders[stop_order_id] = stop_order
        logger.info(f"🛡️ Stop-loss created: {stop_order_id[:8]} @ ${stop_loss_price}")
    
    async def _create_take_profit_order(self, parent_order: Order, take_profit_price: Decimal):
        """Create take-profit order for filled position"""
        opposite_side = OrderSide.SELL if parent_order.side == OrderSide.BUY else OrderSide.BUY
        
        tp_order_id = str(uuid.uuid4())
        tp_order = Order(
            order_id=tp_order_id,
            instrument=parent_order.instrument,
            side=opposite_side,
            order_type=OrderType.TAKE_PROFIT,
            quantity=parent_order.filled_quantity,
            stop_price=take_profit_price,
            parent_order_id=parent_order.order_id
        )
        
        self.orders[tp_order_id] = tp_order
        logger.info(f"🎯 Take-profit created: {tp_order_id[:8]} @ ${take_profit_price}")
    
    async def _update_account_equity(self):
        """Update total account equity based on positions"""
        total_position_value = sum(pos.market_value for pos in self.positions.values())
        total_unrealized_pnl = sum(pos.unrealized_pnl for pos in self.positions.values())
        
        self.equity = self.cash_balance + total_unrealized_pnl
        self.margin_used = total_position_value / self.risk_limits.max_leverage
        self.margin_available = max(Decimal('0'), self.equity - self.margin_used)
        
        # Trigger callback
        if self.on_account_update:
            await self.on_account_update(self.get_account_summary())
    
    def get_account_summary(self) -> Dict[str, Any]:
        """Get complete account summary"""
        total_unrealized_pnl = sum(pos.unrealized_pnl for pos in self.positions.values())
        total_realized_pnl = sum(pos.realized_pnl for pos in self.positions.values())
        
        return {
            'account_id': self.account_id,
            'starting_balance': float(self.starting_balance),
            'cash_balance': float(self.cash_balance),
            'equity': float(self.equity),
            'margin_used': float(self.margin_used),
            'margin_available': float(self.margin_available),
            'daily_pnl': float(self.daily_pnl),
            'total_pnl': float(self.total_pnl),
            'unrealized_pnl': float(total_unrealized_pnl),
            'realized_pnl': float(total_realized_pnl),
            'daily_trades': self.daily_trades,
            'total_trades': self.total_trades,
            'open_positions': len([p for p in self.positions.values() if not p.is_flat]),
            'active_orders': len([o for o in self.orders.values() if o.is_active])
        }
    
    def get_positions(self) -> List[Dict[str, Any]]:
        """Get all positions"""
        return [
            {
                'instrument': pos.instrument,
                'quantity': float(pos.quantity),
                'avg_price': float(pos.avg_price),
                'market_value': float(pos.market_value),
                'unrealized_pnl': float(pos.unrealized_pnl),
                'realized_pnl': float(pos.realized_pnl),
                'total_commission': float(pos.total_commission),
                'side': 'long' if pos.is_long else 'short' if pos.is_short else 'flat',
                'last_update': pos.last_update.isoformat()
            }
            for pos in self.positions.values()
        ]
    
    def get_orders(self, active_only: bool = False) -> List[Dict[str, Any]]:
        """Get order history"""
        orders = [o for o in self.orders.values() if not active_only or o.is_active]
        return [
            {
                'order_id': order.order_id,
                'instrument': order.instrument,
                'side': order.side.value,
                'order_type': order.order_type.value,
                'quantity': float(order.quantity),
                'price': float(order.price) if order.price else None,
                'stop_price': float(order.stop_price) if order.stop_price else None,
                'status': order.status.value,
                'filled_quantity': float(order.filled_quantity),
                'avg_fill_price': float(order.avg_fill_price) if order.avg_fill_price else None,
                'commission': float(order.commission),
                'created_at': order.created_at.isoformat(),
                'time_in_force': order.time_in_force.value
            }
            for order in orders
        ]
    
    def get_trades(self) -> List[Dict[str, Any]]:
        """Get trade history"""
        return [
            {
                'trade_id': trade.trade_id,
                'order_id': trade.order_id,
                'instrument': trade.instrument,
                'side': trade.side.value,
                'quantity': float(trade.quantity),
                'price': float(trade.price),
                'commission': float(trade.commission),
                'timestamp': trade.timestamp.isoformat()
            }
            for trade in self.trades
        ]
    
    async def reset_account(self, new_balance: Optional[Decimal] = None):
        """Reset demo account to initial state"""
        self.cash_balance = new_balance or self.starting_balance
        self.equity = self.cash_balance
        self.margin_used = Decimal('0')
        self.margin_available = self.cash_balance
        
        self.orders.clear()
        self.trades.clear()
        self.positions.clear()
        
        self.daily_pnl = Decimal('0')
        self.total_pnl = Decimal('0')
        self.daily_trades = 0
        self.total_trades = 0
        
        logger.info(f"🔄 Demo account reset - New balance: ${self.cash_balance:,.2f}")