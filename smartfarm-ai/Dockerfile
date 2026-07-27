FROM python:3.9-slim

# Set working directory inside the container
WORKDIR /smartfarm-ai


# Copy requirements.txt to leverage Docker cache
COPY requirements.txt .

# Install dependencies (will fetch pre-compiled wheels for ARM/Raspberry Pi)
RUN pip install --no-cache-dir -r requirements.txt

# Copy the rest of the application files
COPY . .

# Expose the port the AI service runs on
EXPOSE 8000

# Start the decision service
CMD ["python3", "ai_service.py"]
