
import os
import sys
import shutil
from pathlib import Path

def setup_directories():
    """Create the required directory structure"""
    
    # Create backend/ui directory
    ui_dir = Path("backend/ui")
    ui_dir.mkdir(parents=True, exist_ok=True)
    
    # Create static directory for frontend
    static_dir = Path("backend/ui/static")
    static_dir.mkdir(exist_ok=True)
    
    return ui_dir, static_dir

def create_files(ui_dir, static_dir):
    """Create the necessary files"""
    backend_content = """
VFX Trading UI - FastAPI Backend
Serves the trading UI and integrates with crypto API and WebSocket engine
"""
    with open(ui_dir / "server.py", "w") as f:
        f.write(backend_content)

def main():
    """Main setup function"""
    print("===============================================")
    print("     VFX Trading UI Setup")
    print("===============================================")
    print()
    
    # Check if we're in the right directory
    if not Path("backend").exists():
        print("❌ Error: Please run this from the project root directory")
        print("   (The directory containing the 'backend' folder)")
        sys.exit(1)
    
    # Setup directories
    ui_dir, static_dir = setup_directories()
    
    # Create files
    create_files(ui_dir, static_dir)
    
    # Create requirements.txt if it doesn't exist
    if not Path("backend/ui/requirements.txt").exists():
        with open("backend/ui/requirements.txt", "w") as f:
            f.write("""fastapi==0.104.1
uvicorn[standard]==0.24.0
websockets==12.0
httpx==0.25.2
polars==0.19.19
pydantic==2.5.0
python-multipart==0.0.6""")
        print("✅ Created requirements.txt")
    
    print()
    print("🚀 Setup complete! To start the trading UI:")
    print()
    print("1. Install dependencies:")
    print("   cd backend/ui")
    print("   pip install -r requirements.txt")
    print()
    print("2. Start the server:")
    print("   python server.py")
    print()
    print("3. Open your browser:")
    print("   http://localhost:8000")
    print()
    print("📊 Features:")
    print("   - Multi-instrument selection (137 crypto pairs)")
    print("   - Real-time price updates")
    print("   - Interactive TradingView charts")
    print("   - Live order book visualization")
    print("   - Multiple timeframes (1m, 5m, 15m, 1h, 4h, 1d)")
    print("   - WebSocket real-time data")
    print()

if __name__ == "__main__":
    main()