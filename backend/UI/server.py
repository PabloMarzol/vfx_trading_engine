
import asyncio
import logging
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Any
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.staticfiles import StaticFiles
from fastapi.responses import HTMLResponse, FileResponse
from pydantic import BaseModel
import httpx


from demo_api_endpoints import setup_demo_routes

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class MarketTick(BaseModel):
    instrument: str
    timestamp: str
    open: float
    high: float
    low: float
    close: float
    volume: float

class OrderBookEntry(BaseModel):
    price: float
    amount: float

class OrderBook(BaseModel):
    instrument: str
    bids: List[OrderBookEntry]
    asks: List[OrderBookEntry]

class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []
        
    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)
        logger.info(f"Client connected. Total: {len(self.active_connections)}")
        
    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)
        logger.info(f"Client disconnected. Total: {len(self.active_connections)}")
        
    async def broadcast(self, message: dict):
        if not self.active_connections:
            return
            
        disconnected = []
        for connection in self.active_connections:
            try:
                await connection.send_json(message)
            except Exception:
                disconnected.append(connection)
        
        for conn in disconnected:
            self.disconnect(conn)

class CryptoDataManager:
    def __init__(self):
        self.base_url = "https://trade.alt5pro.com"
        self.cache: Dict[str, Any] = {}
        self.cache_ttl = 30
        
    async def get_ticker_data(self) -> List[MarketTick]:
        cache_key = "ticker_data"
        now = datetime.now()
        
        if cache_key in self.cache:
            cached_time, data = self.cache[cache_key]
            if (now - cached_time).seconds < self.cache_ttl:
                return data
        
        try:
            timeout = httpx.Timeout(10.0)
            async with httpx.AsyncClient(timeout=timeout) as client:
                response = await client.get(f"{self.base_url}/marketdata/api/v2/marketdata/ticker")
                response.raise_for_status()
                
                ticker_data = []
                for item in response.json():
                    ticker_data.append(MarketTick(
                        instrument=item["instrument"],
                        timestamp=item["end"],
                        open=item["open"],
                        high=item["high"],
                        low=item["low"],
                        close=item["close"],
                        volume=item["volume"]
                    ))
                
                self.cache[cache_key] = (now, ticker_data)
                logger.info(f"Fetched {len(ticker_data)} instruments")
                return ticker_data
                
        except Exception as e:
            logger.error(f"Error fetching ticker data: {e}")
            # Return cached data if available
            if cache_key in self.cache:
                _, data = self.cache[cache_key]
                return data
            return []
    
    async def get_historical_data(self, instrument: str, timeframe: str = "1m", days: int = 7) -> List[MarketTick]:
        cache_key = f"history_{instrument}_{timeframe}_{days}"
        now = datetime.now()
        
        if cache_key in self.cache:
            cached_time, data = self.cache[cache_key]
            if (now - cached_time).seconds < self.cache_ttl * 2:
                return data
        
        try:
            end_date = datetime.now()
            start_date = end_date - timedelta(days=days)
            
            timeout = httpx.Timeout(15.0)
            async with httpx.AsyncClient(timeout=timeout) as client:
                url = f"{self.base_url}/marketdata/instruments/{instrument}/history"
                params = {
                    "startDate": start_date.strftime("%Y-%m-%d"),
                    "endDate": end_date.strftime("%Y-%m-%d"),
                    "type": timeframe,
                    "count": 1000
                }
                
                response = await client.get(url, params=params)
                response.raise_for_status()
                
                result = response.json()
                if not result.get("success", False):
                    return []
                
                historical_data = []
                for item in result.get("data", []):
                    historical_data.append(MarketTick(
                        instrument=item["instrument"],
                        timestamp=item["start"],
                        open=item["open"],
                        high=item["high"],
                        low=item["low"],
                        close=item["close"],
                        volume=item["volume"]
                    ))
                
                self.cache[cache_key] = (now, historical_data)
                return historical_data
                
        except Exception as e:
            logger.error(f"Error fetching historical data for {instrument}: {e}")
            return []
    
    async def get_order_book(self, instrument: str) -> Optional[OrderBook]:
        try:
            timeout = httpx.Timeout(10.0)
            async with httpx.AsyncClient(timeout=timeout) as client:
                response = await client.get(f"{self.base_url}/marketdata/api/v2/marketdata/depth/{instrument}")
                response.raise_for_status()
                
                data = response.json()
                
                bids = [OrderBookEntry(price=item["price"], amount=item["amount"]) 
                       for item in data.get("bids", [])]
                asks = [OrderBookEntry(price=item["price"], amount=item["amount"]) 
                       for item in data.get("asks", [])]
                
                return OrderBook(
                    instrument=data["instrument"],
                    bids=bids,
                    asks=asks
                )
                
        except Exception as e:
            logger.error(f"Error fetching order book for {instrument}: {e}")
            return None

# Initialize FastAPI app
app = FastAPI(title="VFX Trading UI", version="1.0.0")

app = setup_demo_routes(app)

connection_manager = ConnectionManager()
crypto_manager = CryptoDataManager()

@app.get("/api/instruments")
async def get_instruments():
    ticker_data = await crypto_manager.get_ticker_data()
    instruments = [{"symbol": tick.instrument, "name": tick.instrument.upper()} 
                  for tick in ticker_data]
    return {"instruments": instruments}

@app.get("/api/ticker/{instrument}")
async def get_ticker(instrument: str):
    ticker_data = await crypto_manager.get_ticker_data()
    for tick in ticker_data:
        if tick.instrument == instrument:
            return tick.model_dump()
    
    raise HTTPException(status_code=404, detail="Instrument not found")

@app.get("/api/history/{instrument}")
async def get_history(instrument: str, timeframe: str = "1m", days: int = 7):
    data = await crypto_manager.get_historical_data(instrument, timeframe, days)
    return {"data": [tick.model_dump() for tick in data]}

@app.get("/api/orderbook/{instrument}")
async def get_orderbook(instrument: str):
    orderbook = await crypto_manager.get_order_book(instrument)
    if not orderbook:
        raise HTTPException(status_code=404, detail="Order book not found")
    
    return orderbook.model_dump()

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await connection_manager.connect(websocket)
    
    try:
        while True:
            data = await websocket.receive_json()
            
            if data.get("type") == "subscribe":
                instruments = data.get("instruments", [])
                logger.info(f"Client subscribed to: {instruments}")
                
                for instrument in instruments:
                    ticker_data = await crypto_manager.get_ticker_data()
                    for tick in ticker_data:
                        if tick.instrument == instrument:
                            await websocket.send_json({
                                "type": "ticker",
                                "data": tick.model_dump()
                            })
                    
                    orderbook = await crypto_manager.get_order_book(instrument)
                    if orderbook:
                        await websocket.send_json({
                            "type": "orderbook",
                            "data": orderbook.model_dump()
                        })
            
            elif data.get("type") == "get_history":
                instrument = data.get("instrument")
                timeframe = data.get("timeframe", "1m")
                
                if instrument:
                    history = await crypto_manager.get_historical_data(instrument, timeframe)
                    await websocket.send_json({
                        "type": "history",
                        "instrument": instrument,
                        "timeframe": timeframe,
                        "data": [tick.model_dump() for tick in history]
                    })
            
    except WebSocketDisconnect:
        connection_manager.disconnect(websocket)
    except Exception as e:
        logger.error(f"WebSocket error: {e}")
        connection_manager.disconnect(websocket)

# Background task with better error handling
market_task = None

async def market_data_updater():
    while True:
        try:
            ticker_data = await crypto_manager.get_ticker_data()
            
            if ticker_data:  # Only broadcast if we have data
                await connection_manager.broadcast({
                    "type": "market_update",
                    "timestamp": datetime.now().isoformat(),
                    "data": [tick.model_dump() for tick in ticker_data]
                })
            
            await asyncio.sleep(2)  
        except Exception as e:
            logger.error(f"Market data updater error: {e}")
            await asyncio.sleep(30)  # Longer wait on error

@app.on_event("startup")
async def startup_event():
    global market_task
    market_task = asyncio.create_task(market_data_updater())
    logger.info("🚀 VFX Trading UI Backend started")

@app.on_event("shutdown")
async def shutdown_event():
    global market_task
    if market_task:
        market_task.cancel()

app.mount("/", StaticFiles(directory="static", html=True), name="static")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="localhost", port=8000, log_level="info")