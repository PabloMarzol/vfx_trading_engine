from polygon import WebSocketClient
from polygon.websocket.models import WebSocketMessage
from typing import List
from dotenv import load_dotenv
import os

load_dotenv()


# ============================================= #
# ========= LOGICS ===================== #
# ============================================= #


ws = WebSocketClient(api_key=os.getenv("POL_API_KEY"), subscriptions=["T.AAPL"])


def hdl_message(msg: List[WebSocketMessage]) -> List:
    for m in msg:
        print(m)
        

ws.run(handle_msg=hdl_message)



