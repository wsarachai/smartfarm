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

# TFLite interpreter — the inference runtime for the /disease endpoint.
# PyTorch publishes NO 32-bit ARM (armv7l) wheels at all, so a Pi 3 B cannot
# import torch under any version; tflite-runtime does. 2.14.0 is the newest
# release with a cp311 armv7l wheel, and its manylinux_2_34 floor is met by
# bookworm's glibc 2.36. (Also resolves on x86_64/aarch64, so dev builds work.)
# --no-deps keeps Debian's apt numpy — otherwise pip rebuilds it from source,
# which is the multi-hour hang this image was rewritten to avoid.
# pip itself is purged afterwards, in the same layer, to keep the image small.
ARG TFLITE_VERSION=2.14.0
RUN apt-get update && apt-get install -y --no-install-recommends python3-pip \
    && pip3 install --no-cache-dir --break-system-packages --no-deps "tflite-runtime==${TFLITE_VERSION}" \
    && apt-get purge -y python3-pip && apt-get autoremove -y \
    && rm -rf /var/lib/apt/lists/* /root/.cache

# Set working directory inside the container
WORKDIR /smartfarm-ai

# Copy the rest of the application files
COPY . .

# Expose the port the AI service runs on
EXPOSE 8000

# Start the decision service
CMD ["python3", "ai_service.py"]
