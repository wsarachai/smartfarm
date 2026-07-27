FROM debian:bookworm-slim

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install Python 3, numpy, and pillow using pre-compiled Debian packages.
# This prevents compiling numpy and Pillow from source, which fails/hangs on Raspberry Pi 3 B.
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 \
    python3-numpy \
    python3-pillow \
    && rm -rf /var/lib/apt/lists/*

# Set working directory inside the container
WORKDIR /smartfarm-ai

# Copy the rest of the application files
COPY . .

# Expose the port the AI service runs on
EXPOSE 8000

# Start the decision service
CMD ["python3", "ai_service.py"]
