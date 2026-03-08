# FastAPI Lab Course Server

A simple FastAPI server with endpoints for getting time and date information.

## Installation

1. Navigate to the project directory:
```bash
cd small_react_server
```

2. Install the required dependencies:
```bash
pip install -r requirements.txt
```

## Running the Server

Start the server with:
```bash
uvicorn main:app --reload --host 0.0.0.0 --port 8000
```

Or using Python module syntax:
```bash
python -m uvicorn main:app --reload --host 0.0.0.0 --port 8000
```

The server will be accessible at:
- Local: `http://localhost:8000`
- Network: `http://<your-ip>:8000`

To stop the server, press `Ctrl+C` in the terminal.

## API Endpoints

### GET /makelabcourse
Returns a default greeting message.

**Example:**
```bash
curl http://localhost:8000/makelabcourse
```

**Response:**
```json
{"response": "hello hello"}
```

### GET /makelabcourse/time
Returns the current time in 24-hour format (HH:MM).

**Example:**
```bash
curl http://localhost:8000/makelabcourse/time
```

**Response:**
```json
{"response": "15:47"}
```

### GET /makelabcourse/date
Returns the current date in readable format.

**Example:**
```bash
curl http://localhost:8000/makelabcourse/date
```

**Response:**
```json
{"response": "November 27, 2025"}
```

## Interactive API Documentation

FastAPI automatically generates interactive API documentation. Once the server is running, visit:
- Swagger UI: `http://localhost:8000/docs`
- ReDoc: `http://localhost:8000/redoc`

## Testing with Postman

1. Open Postman
2. Select `GET` method
3. Enter one of the URLs:
   - `http://localhost:8000/makelabcourse`
   - `http://localhost:8000/makelabcourse/time`
   - `http://localhost:8000/makelabcourse/date`
4. Click "Send"

No headers or body required!
