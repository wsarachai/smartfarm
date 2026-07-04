# web-server — Smart Farm Control Center

## Web-server Docker Hub publish (multi-arch)

Use this from `../web-server` to build and push one image tag set for both
desktop (`amd64`) and Jetson (`arm64`):

```bash
docker buildx build --platform linux/amd64,linux/arm64 -t wsarachai/smartfarm-web-server:latest -t wsarachai/smartfarm-web-server:1.0.0 --push .
```

