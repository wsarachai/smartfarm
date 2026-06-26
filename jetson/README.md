# smartfarm
## Run interactive
```
sudo docker exec -it l4t-pytorch-container bash
```

## Test code
```
import torch
print('CUDA Available:', torch.cuda.is_available())
print('Device Name:', torch.cuda.get_device_name(0))
```


