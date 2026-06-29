import torch
import time


def test_gpu():
    print("=" * 45)
    print("  JETSON GPU + PYTORCH SANITY CHECK")
    print("=" * 45)

    print(f"PyTorch version : {torch.__version__}")
    print(f"CUDA version    : {torch.version.cuda}")
    print(f"CUDA available  : {torch.cuda.is_available()}")

    if not torch.cuda.is_available():
        print("\n[FAIL] CUDA not available — check JetPack / Docker runtime.")
        return

    dev = torch.device("cuda")
    name = torch.cuda.get_device_name(0)
    cap = torch.cuda.get_device_capability(0)
    print(f"GPU             : {name}  (sm_{cap[0]}{cap[1]})")

    # ── FP32 matmul ──────────────────────────────────────────────────────────
    print("\n[FP32] 4096×4096 matmul")
    a = torch.randn(4096, 4096, device=dev)
    b = torch.randn(4096, 4096, device=dev)
    torch.matmul(a, b); torch.cuda.synchronize()          # warm-up
    t0 = time.perf_counter()
    torch.matmul(a, b); torch.cuda.synchronize()
    fp32_ms = (time.perf_counter() - t0) * 1000
    print(f"  elapsed : {fp32_ms:.1f} ms")
    del a, b

    # ── FP16 matmul (Jetson Ampere supports tensor-core FP16) ────────────────
    print("\n[FP16] 4096×4096 matmul")
    a = torch.randn(4096, 4096, device=dev, dtype=torch.float16)
    b = torch.randn(4096, 4096, device=dev, dtype=torch.float16)
    torch.matmul(a, b); torch.cuda.synchronize()          # warm-up
    t0 = time.perf_counter()
    torch.matmul(a, b); torch.cuda.synchronize()
    fp16_ms = (time.perf_counter() - t0) * 1000
    print(f"  elapsed : {fp16_ms:.1f} ms  (speedup vs FP32: {fp32_ms/fp16_ms:.1f}×)")
    del a, b

    # ── Memory snapshot ───────────────────────────────────────────────────────
    torch.cuda.empty_cache()
    alloc_mb = torch.cuda.memory_allocated(dev) / 1024**2
    reserv_mb = torch.cuda.memory_reserved(dev) / 1024**2
    print(f"\n[MEM] allocated {alloc_mb:.1f} MB  |  reserved {reserv_mb:.1f} MB")

    print("\n[PASS] GPU functional.\n")


if __name__ == "__main__":
    test_gpu()
