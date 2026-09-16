FROM mcr.microsoft.com/devcontainers/base:trixie

# Create `~/.cache` with correct permissions so that dev container doesn't hit `EACCES` error during initial image creation.
RUN mkdir -p /home/vscode/.cache && chown vscode:vscode /home/vscode/.cache
