"""
FastAPI endpoints for Demo Trading Engine
Integrates with existing VFX UI server and ALT5 data feeds
"""

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect, Depends
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field
from typing import Optional, List, Dict, Any
from decimal import Decimal
from datetime import datetime
import asyncio
import json
import logging
import httpx

from demo_trading_engine import (
    DemoTradingEngine, OrderType, OrderSide, TimeInForce, 
    MarketTick, CommissionStructure, RiskLimits
)

logger = logging.getLogger(__name__)

# Pydantic models for API requests/responses
class OrderRequest(BaseModel):
    instrument: str = Field(..., description="Trading instrument (e.g., 'btc_usdt')")
    side: str = Field(..., description="Order side: 'buy' or 'sell'")
    order_type: str = Field(..., description="Order type: 'market', 'limit', 'stop_loss', 'take_profit'")
    quantity: float = Field(..., gt=0, description="Order quantity")
    price: Optional[float] = Field(None, description="Limit price (required for limit orders)")
    stop_price: Optional[float] = Field(None, description="Stop price (required for stop orders)")
    time_in_force: str = Field("good_till_cancelled", description="Time in force")
    stop_loss: Optional[float] = Field(None, description="Stop loss price")
    take_profit: Optional[float] = Field(None, description="Take profit price")

class AccountConfigRequest(BaseModel):
    starting_balance: float = Field(100000, gt=0, description="Starting account balance")
    max_daily_loss: float = Field(5000, gt=0, description="Maximum daily loss limit")
    max_position_size: float = Field(50000, gt=0, description="Maximum position size")
    max_leverage: float = Field(4, gt=1, description="Maximum leverage")

class ALT5DataFeed:
    """ALT5 data feed integration"""
    
    def __init__(self, base_url: str = "https://trade.alt5pro.com/marketdata/api/v2"):
        self.base_url = base_url
        self.client = httpx.AsyncClient()
        self.subscribers = {}  # instrument -> list of callbacks
        self.running = False
        
    async def start_feed(self):
        """Start the ALT5 data feed"""
        self.running = True
        asyncio.create_task(self._price_feed_loop())
        logger.info("📡 ALT5 data feed started")
    
    async def stop_feed(self):
        """Stop the ALT5 data feed"""
        self.running = False
        await self.client.aclose()
        logger.info("📡 ALT5 data feed stopped")
    
    def subscribe(self, instrument: str, callback):
        """Subscribe to price updates for an instrument"""
        if instrument not in self.subscribers:
            self.subscribers[instrument] = []
        self.subscribers[instrument].append(callback)
        logger.info(f"📊 Subscribed to {instrument}")
    
    async def _price_feed_loop(self):
        """Continuous price feed from ALT5"""
        while self.running:
            try:
                # Fetch ticker data from ALT5
                response = await self.client.get(f"{self.base_url}/marketdata/ticker")
                if response.status_code == 200:
                    tickers = response.json()
                    
                    for ticker in tickers:
                        instrument = ticker['instrument']
                        if instrument in self.subscribers:
                            # Create MarketTick from ALT5 data
                            tick = MarketTick(
                                instrument=instrument,
                                bid=float(ticker['low']),  # Using low as bid for demo
                                ask=float(ticker['high']), # Using high as ask for demo
                                last=float(ticker['close']),
                                timestamp=datetime.now(),
                                volume=float(ticker['volume'])
                            )
                            
                            # Notify all subscribers
                            for callback in self.subscribers[instrument]:
                                try:
                                    await callback(tick)
                                except Exception as e:
                                    logger.error(f"Error in price callback: {e}")
                
                await asyncio.sleep(1)  # Update every second
                
            except Exception as e:
                logger.error(f"Error in ALT5 price feed: {e}")
                await asyncio.sleep(5)

class DemoTradingAPI:
    """FastAPI integration for demo trading"""
    
    def __init__(self):
        self.engines: Dict[str, DemoTradingEngine] = {}  # session_id -> engine
        self.websockets: Dict[str, WebSocket] = {}  # session_id -> websocket
        self.alt5_feed = ALT5DataFeed()
        
    async def startup(self):
        """Initialize the demo trading API"""
        await self.alt5_feed.start_feed()
        logger.info("🚀 Demo Trading API started")
    
    async def shutdown(self):
        """Cleanup on shutdown"""
        await self.alt5_feed.stop_feed()
        logger.info("🛑 Demo Trading API stopped")
    
    async def create_demo_account(self, session_id: str, config: AccountConfigRequest) -> DemoTradingEngine:
        """Create a new demo trading account"""
        
        # Create commission structure
        commission_structure = CommissionStructure(
            stock_per_share=Decimal('0.005'),
            forex_per_lot=Decimal('4.50'),
            crypto_percentage=Decimal('0.001'),
            minimum_commission=Decimal('1.00')
        )
        
        # Create risk limits
        risk_limits = RiskLimits(
            max_daily_loss=Decimal(str(config.max_daily_loss)),
            max_position_size=Decimal(str(config.max_position_size)),
            max_leverage=Decimal(str(config.max_leverage)),
            allowed_instruments=[]  # Allow all instruments
        )
        
        # Create demo engine
        engine = DemoTradingEngine(
            starting_balance=Decimal(str(config.starting_balance)),
            commission_structure=commission_structure,
            risk_limits=risk_limits
        )
        
        # Set up callbacks for real-time updates
        engine.on_order_update = lambda order: self._broadcast_order_update(session_id, order)
        engine.on_trade_executed = lambda trade: self._broadcast_trade_update(session_id, trade)
        engine.on_position_update = lambda position: self._broadcast_position_update(session_id, position)
        engine.on_account_update = lambda account: self._broadcast_account_update(session_id, account)
        
        # Subscribe to ALT5 data for all common instruments
        common_instruments = [
            'btc_usdt', 'eth_usdt', 'ada_usdt', 'sol_usd', 'usdc_eur',
            'usdc_cad', 'au_usdt', 'dash_usd', 'bat_usd'
        ]
        
        for instrument in common_instruments:
            self.alt5_feed.subscribe(instrument, engine.process_market_data)
        
        self.engines[session_id] = engine
        logger.info(f"💰 Demo account created for session {session_id[:8]}")
        return engine
    
    def get_engine(self, session_id: str) -> DemoTradingEngine:
        """Get demo engine for session"""
        if session_id not in self.engines:
            raise HTTPException(status_code=404, detail="Demo account not found")
        return self.engines[session_id]
    
    async def _broadcast_order_update(self, session_id: str, order):
        """Broadcast order update via WebSocket"""
        if session_id in self.websockets:
            try:
                await self.websockets[session_id].send_text(json.dumps({
                    'type': 'order_update',
                    'data': {
                        'order_id': order.order_id,
                        'instrument': order.instrument,
                        'side': order.side.value,
                        'order_type': order.order_type.value,
                        'quantity': float(order.quantity),
                        'price': float(order.price) if order.price else None,
                        'status': order.status.value,
                        'filled_quantity': float(order.filled_quantity),
                        'avg_fill_price': float(order.avg_fill_price) if order.avg_fill_price else None,
                        'commission': float(order.commission),
                        'created_at': order.created_at.isoformat()
                    }
                }))
            except Exception as e:
                logger.error(f"Error broadcasting order update: {e}")
    
    async def _broadcast_trade_update(self, session_id: str, trade):
        """Broadcast trade execution via WebSocket"""
        if session_id in self.websockets:
            try:
                await self.websockets[session_id].send_text(json.dumps({
                    'type': 'trade_executed',
                    'data': {
                        'trade_id': trade.trade_id,
                        'order_id': trade.order_id,
                        'instrument': trade.instrument,
                        'side': trade.side.value,
                        'quantity': float(trade.quantity),
                        'price': float(trade.price),
                        'commission': float(trade.commission),
                        'timestamp': trade.timestamp.isoformat()
                    }
                }))
            except Exception as e:
                logger.error(f"Error broadcasting trade update: {e}")
    
    async def _broadcast_position_update(self, session_id: str, position):
        """Broadcast position update via WebSocket"""
        if session_id in self.websockets:
            try:
                await self.websockets[session_id].send_text(json.dumps({
                    'type': 'position_update',
                    'data': {
                        'instrument': position.instrument,
                        'quantity': float(position.quantity),
                        'avg_price': float(position.avg_price),
                        'market_value': float(position.market_value),
                        'unrealized_pnl': float(position.unrealized_pnl),
                        'realized_pnl': float(position.realized_pnl),
                        'total_commission': float(position.total_commission),
                        'side': 'long' if position.is_long else 'short' if position.is_short else 'flat',
                        'last_update': position.last_update.isoformat()
                    }
                }))
            except Exception as e:
                logger.error(f"Error broadcasting position update: {e}")
    
    async def _broadcast_account_update(self, session_id: str, account_summary):
        """Broadcast account update via WebSocket"""
        if session_id in self.websockets:
            try:
                await self.websockets[session_id].send_text(json.dumps({
                    'type': 'account_update',
                    'data': account_summary
                }))
            except Exception as e:
                logger.error(f"Error broadcasting account update: {e}")

# Initialize the demo API
demo_api = DemoTradingAPI()

# FastAPI app integration
def setup_demo_routes(app: FastAPI):
    """Add demo trading routes to existing FastAPI app"""
    
    @app.on_event("startup")
    async def startup_event():
        await demo_api.startup()
    
    @app.on_event("shutdown")
    async def shutdown_event():
        await demo_api.shutdown()
    
    @app.post("/api/demo/account/create")
    async def create_demo_account(
        config: AccountConfigRequest,
        session_id: str = "default"
    ):
        """Create a new demo trading account"""
        try:
            engine = await demo_api.create_demo_account(session_id, config)
            return {
                "success": True,
                "account_id": engine.account_id,
                "starting_balance": float(engine.starting_balance),
                "message": "Demo account created successfully"
            }
        except Exception as e:
            logger.error(f"Error creating demo account: {e}")
            raise HTTPException(status_code=500, detail=str(e))
    
    @app.get("/api/demo/account/summary")
    async def get_account_summary(session_id: str = "default"):
        """Get account summary"""
        engine = demo_api.get_engine(session_id)
        return engine.get_account_summary()
    
    @app.post("/api/demo/orders/place")
    async def place_order(
        order_request: OrderRequest,
        session_id: str = "default"
    ):
        """Place a new order"""
        try:
            engine = demo_api.get_engine(session_id)
            
            # Convert string enums
            side = OrderSide.BUY if order_request.side.lower() == 'buy' else OrderSide.SELL
            order_type = OrderType(order_request.order_type.lower())
            time_in_force = TimeInForce(order_request.time_in_force.lower())
            
            order_id = await engine.place_order(
                instrument=order_request.instrument,
                side=side,
                order_type=order_type,
                quantity=Decimal(str(order_request.quantity)),
                price=Decimal(str(order_request.price)) if order_request.price else None,
                stop_price=Decimal(str(order_request.stop_price)) if order_request.stop_price else None,
                time_in_force=time_in_force,
                stop_loss=Decimal(str(order_request.stop_loss)) if order_request.stop_loss else None,
                take_profit=Decimal(str(order_request.take_profit)) if order_request.take_profit else None
            )
            
            return {
                "success": True,
                "order_id": order_id,
                "message": "Order placed successfully"
            }
        
        except ValueError as e:
            raise HTTPException(status_code=400, detail=str(e))
        except Exception as e:
            logger.error(f"Error placing order: {e}")
            raise HTTPException(status_code=500, detail=str(e))
    
    @app.delete("/api/demo/orders/{order_id}")
    async def cancel_order(
        order_id: str,
        session_id: str = "default"
    ):
        """Cancel an order"""
        try:
            engine = demo_api.get_engine(session_id)
            success = await engine.cancel_order(order_id)
            
            if success:
                return {"success": True, "message": "Order cancelled successfully"}
            else:
                raise HTTPException(status_code=404, detail="Order not found or cannot be cancelled")
        
        except Exception as e:
            logger.error(f"Error cancelling order: {e}")
            raise HTTPException(status_code=500, detail=str(e))
    
    @app.get("/api/demo/orders")
    async def get_orders(
        active_only: bool = False,
        session_id: str = "default"
    ):
        """Get order history"""
        engine = demo_api.get_engine(session_id)
        return engine.get_orders(active_only=active_only)
    
    @app.get("/api/demo/positions")
    async def get_positions(session_id: str = "default"):
        """Get current positions"""
        engine = demo_api.get_engine(session_id)
        return engine.get_positions()
    
    @app.get("/api/demo/trades")
    async def get_trades(session_id: str = "default"):
        """Get trade history"""
        engine = demo_api.get_engine(session_id)
        return engine.get_trades()
    
    @app.post("/api/demo/account/reset")
    async def reset_account(
        new_balance: Optional[float] = None,
        session_id: str = "default"
    ):
        """Reset demo account"""
        try:
            engine = demo_api.get_engine(session_id)
            balance = Decimal(str(new_balance)) if new_balance else None
            await engine.reset_account(balance)
            
            return {
                "success": True,
                "message": "Account reset successfully",
                "new_balance": float(engine.cash_balance)
            }
        
        except Exception as e:
            logger.error(f"Error resetting account: {e}")
            raise HTTPException(status_code=500, detail=str(e))
    
    @app.get("/api/demo/instruments")
    async def get_available_instruments():
        """Get list of available trading instruments from ALT5"""
        try:
            async with httpx.AsyncClient() as client:
                response = await client.get("https://trade.alt5pro.com/marketdata/api/v2/marketdata/assets")
                if response.status_code == 200:
                    assets = response.json()
                    instruments = []
                    
                    for asset in assets:
                        if asset.get('liquid', False):  # Only liquid assets
                            instruments.append({
                                'id': asset['id'],
                                'name': asset['asset_name'],
                                'can_trade': asset.get('can_deposit', False) and asset.get('can_withdrawal', False),
                                'scale': asset.get('scale', 2)
                            })
                    
                    return instruments
                else:
                    raise HTTPException(status_code=500, detail="Failed to fetch instruments")
        
        except Exception as e:
            logger.error(f"Error fetching instruments: {e}")
            raise HTTPException(status_code=500, detail=str(e))
    
    @app.websocket("/ws/demo/{session_id}")
    async def websocket_endpoint(websocket: WebSocket, session_id: str):
        """WebSocket endpoint for real-time updates"""
        await websocket.accept()
        demo_api.websockets[session_id] = websocket
        
        try:
            # Send initial account summary
            if session_id in demo_api.engines:
                engine = demo_api.engines[session_id]
                await websocket.send_text(json.dumps({
                    'type': 'connected',
                    'data': {
                        'message': 'Connected to demo trading feed',
                        'account_summary': engine.get_account_summary()
                    }
                }))
            
            # Keep connection alive
            while True:
                try:
                    # Wait for any message (ping/pong)
                    data = await websocket.receive_text()
                    # Echo back for heartbeat
                    await websocket.send_text(json.dumps({
                        'type': 'pong',
                        'timestamp': datetime.now().isoformat()
                    }))
                except WebSocketDisconnect:
                    break
        
        except Exception as e:
            logger.error(f"WebSocket error: {e}")
        finally:
            # Clean up
            if session_id in demo_api.websockets:
                del demo_api.websockets[session_id]
            logger.info(f"🔌 WebSocket disconnected for session {session_id[:8]}")
    
    return app

# Usage example for integrating with existing server.py
if __name__ == "__main__":
    # This shows how to integrate with your existing FastAPI app
    from fastapi import FastAPI
    import uvicorn
    
    app = FastAPI(title="VFX Demo Trading API", version="1.0.0")
    
    # Setup demo routes
    app = setup_demo_routes(app)
    
    # Add other existing routes here...
    
    uvicorn.run(app, host="0.0.0.0", port=8000)