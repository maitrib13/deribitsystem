import websockets
import json
import asyncio
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

async def get_auth_token():
    # Deribit WebSocket URL
    url = "wss://test.deribit.com/ws/api/v2"
    
    async with websockets.connect(url) as ws:
        logger.info("Connected to Deribit WebSocket")
        
        # Auth message
        auth_msg = {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "public/auth",
            "params": {
                "grant_type": "client_credentials",
                "client_id": "TYleJrdp",
                "client_secret": "qLNt6KnQOhh3QqiG8Z-l7sZkHp542OC7h6FvkaDGGF8"
            }
        }
        
        # Send authentication request
        await ws.send(json.dumps(auth_msg))
        logger.info(f"Sent auth request: {json.dumps(auth_msg, indent=2)}")
        
        # Get response
        response = await ws.recv()
        data = json.loads(response)
        logger.info("Received response:")
        print(json.dumps(data, indent=2))
        
        if "result" in data:
            logger.info("Authentication successful!")
            logger.info(f"Access Token: {data['result']['access_token']}")
            logger.info(f"Refresh Token: {data['result']['refresh_token']}")
            logger.info(f"Expires In: {data['result']['expires_in']} seconds")
        else:
            logger.error("Authentication failed!")
            if "error" in data:
                logger.error(f"Error: {data['error']}")

async def main():
    try:
        await get_auth_token()
    except Exception as e:
        logger.error(f"Error: {str(e)}")

if __name__ == "__main__":
    asyncio.run(main())