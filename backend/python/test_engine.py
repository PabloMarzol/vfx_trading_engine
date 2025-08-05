import asyncio
import websockets
import json
import logging
import random
import time
from datetime import datetime
from typing import Dict, List, Optional
from dataclasses import dataclass, asdict

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

@dataclass
class Order:
    order_id: int
    symbol: str
    side: str
    order_type: str
    quantity: float
    price: float
    status: str
    timestamp: str

@dataclass
class Execution:
    execution_id: int
    order_id: int
    symbol: str
    side: str
    quantity: float
    price: float
    timestamp: str

@dataclass
class MarketData:
    symbol: str
    price: float
    bid: float
    ask: float
    volume: float
    timestamp: str

class MockTradingEngine:
    def __init__(self):
        self.orders: Dict[int, Order] = {}
        self.executions: List[Execution] = []
        self.positions: Dict[str, float] = {}
        self.market_data: Dict[str, MarketData] = {}
        
        # Counters
        self.next_order_id = 1000
        self.next_execution_id = 1
        
        # Connected clients
        self.clients = set()
        
        # Market prices (starting values)
        self.base_prices = {
            'BTC/USD': 68450.0,
            'ETH/USD': 3892.0,
            'AAPL': 175.0,
            'TSLA': 245.0,
            'NVDA': 875.0,
            'SPY': 450.0,
            'QQQ': 385.0
        }
        
        # Initialize market data
        self._initialize_market_data()
        
        logger.info("🚀 VFX Mock Trading Engine initialized")
    
    def _initialize_market_data(self):
        """Initialize market data for all symbols"""
        for symbol, base_price in self.base_prices.items():
            spread = base_price * 0.001  # 0.1% spread
            self.market_data[symbol] = MarketData(
                symbol=symbol,
                price=base_price,
                bid=base_price - spread/2,
                ask=base_price + spread/2,
                volume=random.randint(100000, 1000000),
                timestamp=datetime.now().isoformat()
            )
    
    def submit_order(self, symbol: str, side: str, order_type: str, quantity: float, price: float = 0.0) -> int:
        """Submit a new order"""
        order_id = self.next_order_id
        self.next_order_id += 1
        
        # Create order
        order = Order(
            order_id=order_id,
            symbol=symbol,
            side=side,
            order_type=order_type,
            quantity=quantity,
            price=price,
            status='PENDING',
            timestamp=datetime.now().isoformat()
        )
        
        self.orders[order_id] = order
        
        logger.info(f"📝 Order {order_id}: {side} {quantity} {symbol} @ {price if order_type == 'LIMIT' else 'MARKET'}")
        
        # Execute immediately (simulate instant execution)
        self._execute_order(order_id)
        
        return order_id
    
    def _execute_order(self, order_id: int):
        """Execute an order"""
        order = self.orders.get(order_id)
        if not order:
            return
        
        # Get execution price
        market_data = self.market_data.get(order.symbol)
        if not market_data:
            logger.error(f"No market data for {order.symbol}")
            return
        
        if order.order_type == 'MARKET':
            exec_price = market_data.ask if order.side == 'BUY' else market_data.bid
        else:
            exec_price = order.price
        
        # Add some realistic slippage
        slippage = random.uniform(-0.001, 0.001)  # ±0.1%
        exec_price *= (1 + slippage)
        
        # Create execution
        execution_id = self.next_execution_id
        self.next_execution_id += 1
        
        execution = Execution(
            execution_id=execution_id,
            order_id=order_id,
            symbol=order.symbol,
            side=order.side,
            quantity=order.quantity,
            price=exec_price,
            timestamp=datetime.now().isoformat()
        )
        
        self.executions.append(execution)
        
        # Update order status
        order.status = 'FILLED'
        order.price = exec_price
        
        # Update position
        self._update_position(execution)
        
        logger.info(f"✅ EXECUTED: Order {order_id} - {order.side} {order.quantity} {order.symbol} @ ${exec_price:.2f}")
        
        # Broadcast to clients
        asyncio.create_task(self._broadcast_execution(execution))
    
    def _update_position(self, execution: Execution):
        """Update position after execution"""
        symbol = execution.symbol
        current_pos = self.positions.get(symbol, 0.0)
        
        if execution.side == 'BUY':
            new_pos = current_pos + execution.quantity
        else:
            new_pos = current_pos - execution.quantity
        
        self.positions[symbol] = new_pos
        logger.info(f"📊 Position {symbol}: {new_pos}")
    
    def get_market_data(self, symbol: str) -> Optional[MarketData]:
        """Get current market data for symbol"""
        return self.market_data.get(symbol)
    
    def update_market_prices(self):
        """Update market prices with random movements"""
        for symbol in self.base_prices:
            if symbol in self.market_data:
                # Random price movement (±0.5%)
                change = random.uniform(-0.005, 0.005)
                old_price = self.market_data[symbol].price
                new_price = old_price * (1 + change)
                
                spread = new_price * 0.001
                
                self.market_data[symbol] = MarketData(
                    symbol=symbol,
                    price=new_price,
                    bid=new_price - spread/2,
                    ask=new_price + spread/2,
                    volume=random.randint(100000, 1000000),
                    timestamp=datetime.now().isoformat()
                )
    
    async def _broadcast_execution(self, execution: Execution):
        """Broadcast execution to all connected clients"""
        if not self.clients:
            return
        
        message = {
            'type': 'execution',
            'data': asdict(execution)
        }
        
        # Send to all clients
        disconnected = set()
        for client in self.clients:
            try:
                await client.send(json.dumps(message))
            except websockets.exceptions.ConnectionClosed:
                disconnected.add(client)
        
        # Remove disconnected clients
        self.clients -= disconnected
    
    async def _broadcast_market_data(self, market_data: MarketData):
        """Broadcast market data to all connected clients"""
        if not self.clients:
            return
        
        message = {
            'type': 'market_data',
            'symbol': market_data.symbol,
            'timestamp': market_data.timestamp,
            'price': market_data.price,
            'volume': market_data.volume,
            'bid': market_data.bid,
            'ask': market_data.ask
        }
        
        # Send to all clients
        disconnected = set()
        for client in self.clients:
            try:
                await client.send(json.dumps(message))
            except websockets.exceptions.ConnectionClosed:
                disconnected.add(client)
        
        # Remove disconnected clients
        self.clients -= disconnected

class WebSocketServer:
    def __init__(self, trading_engine: MockTradingEngine):
        self.engine = trading_engine
        self.running = False
    
    async def handle_client(self, websocket, path):
        """Handle incoming WebSocket connections"""
        self.engine.clients.add(websocket)
        client_addr = websocket.remote_address
        logger.info(f"🔌 Client connected: {client_addr} (Total: {len(self.engine.clients)})")
        
        try:
            # Send welcome message
            welcome = {
                'type': 'connected',
                'message': 'Connected to VFX Trading Engine',
                'timestamp': datetime.now().isoformat()
            }
            await websocket.send(json.dumps(welcome))
            
            # Handle incoming messages
            async for message in websocket:
                try:
                    data = json.loads(message)
                    await self._process_message(websocket, data)
                except json.JSONDecodeError:
                    logger.error(f"Invalid JSON from {client_addr}")
                except Exception as e:
                    logger.error(f"Error processing message from {client_addr}: {e}")
        
        except websockets.exceptions.ConnectionClosed:
            pass
        finally:
            self.engine.clients.discard(websocket)
            logger.info(f"🔌 Client disconnected: {client_addr} (Total: {len(self.engine.clients)})")
    
    async def _process_message(self, websocket, data):
        """Process incoming message from client"""
        msg_type = data.get('type')
        
        if msg_type == 'order':
            # Handle order submission
            order_id = self.engine.submit_order(
                symbol=data.get('symbol'),
                side=data.get('side'),
                order_type=data.get('order_type', 'MARKET'),
                quantity=data.get('quantity'),
                price=data.get('price', 0.0)
            )
            
            # Send confirmation
            response = {
                'type': 'order_confirmation',
                'order_id': order_id,
                'timestamp': datetime.now().isoformat()
            }
            await websocket.send(json.dumps(response))
        
        elif msg_type == 'subscribe':
            # Handle market data subscription
            symbols = data.get('symbols', [])
            for symbol in symbols:
                market_data = self.engine.get_market_data(symbol)
                if market_data:
                    await self.engine._broadcast_market_data(market_data)
            
            response = {
                'type': 'subscribed',
                'symbols': symbols,
                'timestamp': datetime.now().isoformat()
            }
            await websocket.send(json.dumps(response))
        
        elif msg_type == 'ping':
            # Handle ping
            response = {
                'type': 'pong',
                'timestamp': data.get('timestamp')
            }
            await websocket.send(json.dumps(response))
        
        elif msg_type == 'strategy_control':
            # Handle strategy control
            logger.info(f"Strategy control: {data.get('action')} {data.get('strategy')}")
            response = {
                'type': 'strategy_response',
                'action': data.get('action'),
                'strategy': data.get('strategy'),
                'status': 'success'
            }
            await websocket.send(json.dumps(response))
    
    async def start_market_data_feed(self):
        """Start broadcasting market data updates"""
        while self.running:
            try:
                # Update prices
                self.engine.update_market_prices()
                
                # Broadcast updates for active symbols
                for symbol, market_data in self.engine.market_data.items():
                    if random.random() < 0.3:  # 30% chance to broadcast each symbol
                        await self.engine._broadcast_market_data(market_data)
                
                await asyncio.sleep(2)  # Update every 2 seconds
            except Exception as e:
                logger.error(f"Error in market data feed: {e}")
                await asyncio.sleep(5)
    
    async def start_server(self, host='localhost', port=8080):
        """Start the WebSocket server"""
        self.running = True
        
        logger.info(f"🚀 Starting VFX WebSocket Server on {host}:{port}")
        
        # Start market data feed
        market_task = asyncio.create_task(self.start_market_data_feed())
        
        # Start WebSocket server
        async with websockets.serve(self.handle_client, host, port):
            logger.info(f"✅ VFX Trading Engine server ready at ws://{host}:{port}")
            logger.info("📊 Market data feed active")
            logger.info("💹 Ready to accept trading strategies")
            
            try:
                # Keep server running
                await asyncio.Future()  # Run forever
            except KeyboardInterrupt:
                logger.info("🛑 Shutting down server...")
                self.running = False
                market_task.cancel()

async def main():
    """Main entry point"""
    print("===============================================")
    print("  VFX Trading Engine - Python Mock Server")
    print("===============================================")
    print()
    
    # Create trading engine
    engine = MockTradingEngine()
    
    # Create WebSocket server
    server = WebSocketServer(engine)
    
    try:
        # Start server
        await server.start_server()
    except KeyboardInterrupt:
        logger.info("Server stopped by user")
    except Exception as e:
        logger.error(f"Server error: {e}")

if __name__ == "__main__":
    asyncio.run(main())