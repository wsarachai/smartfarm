import torch
import time

def test_gpu():
    print("=" * 40)
    print("NVIDIA GPU & PYTORCH SANITY CHECK")
    print("=" * 40)

    # 1. Basic Availability Check
    cuda_available = torch.cuda.is_available()
    print(f"CUDA Available: {cuda_available}")
    
    if not cuda_available:
        print("\n[ERROR] CUDA is not available. PyTorch cannot see your GPU.")
        print("Please check your NVIDIA drivers, JetPack/CUDA toolkit installation, or Docker runtime.")
        return

    # 2. Device Properties
    device_count = torch.cuda.device_count()
    print(f"Number of GPUs detected: {device_count}")
    
    current_device = torch.cuda.current_device()
    device_name = torch.cuda.get_device_name(current_device)
    print(f"Active GPU Index: {current_device}")
    print(f"GPU Device Name: {device_name}")
    
    # 3. Memory Allocation & Capability Test
    print("\n--- Running Matrix Multiplication Test ---")
    try:
        # Set device
        device = torch.device("cuda")
        
        # Create two large random matrices directly on the GPU
        size = 5000
        print(f"Allocating two {size}x{size} matrices on GPU memory...")
        x = torch.randn(size, size, device=device)
        y = torch.randn(size, size, device=device)
        
        # Benchmark the matrix multiplication
        print("Executing matrix multiplication (X * Y)...")
        start_time = time.time()
        
        # torch.matmul handles the operation. 
        # cuda.synchronize() ensures the GPU finishes the task before we stop the clock.
        result = torch.matmul(x, y)
        torch.cuda.synchronize() 
        
        end_time = time.time()
        execution_time = end_time - start_time
        
        print(f"Success! Operation completed in: {execution_time:.4f} seconds")
        
        # 4. Memory Diagnostics
        print("\n--- GPU Memory Statistics ---")
        allocated = torch.cuda.memory_allocated(device) / (1024 ** 2)
        reserved = torch.cuda.memory_reserved(device) / (1024 ** 2)
        print(f"Allocated Memory: {allocated:.2f} MB")
        print(f"Reserved/Cached Memory: {reserved:.2f} MB")
        
        # Clear memory
        del x, y, result
        torch.cuda.empty_cache()
        print("\n[PASS] GPU is fully functional and communicating with PyTorch.")
        
    except Exception as e:
        print(f"\n[FAIL] An error occurred during the GPU test: {e}")

if __name__ == "__main__":
    test_gpu()