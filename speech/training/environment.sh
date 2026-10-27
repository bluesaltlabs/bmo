# Source from the repository root before training/setup commands.
mkdir -p .tmp/piper-training-cache .tmp/torch .tmp/huggingface .tmp/matplotlib .tmp/compiler-cache
export TMPDIR="$PWD/.tmp"
export PIP_CACHE_DIR="$PWD/.tmp/piper-training-cache"
export PIP_DISABLE_PIP_VERSION_CHECK=1
export TORCH_HOME="$PWD/.tmp/torch"
export HF_HOME="$PWD/.tmp/huggingface"
export HF_HUB_DISABLE_TELEMETRY=1
export ORT_DISABLE_TELEMETRY=1
export XDG_CACHE_HOME="$PWD/.tmp/piper-training-cache"
export MPLCONFIGDIR="$PWD/.tmp/matplotlib"
export CLANG_MODULE_CACHE_PATH="$PWD/.tmp/compiler-cache"
export TORCHINDUCTOR_CACHE_DIR="$PWD/.tmp/torchinductor"
export TORCH_EXTENSIONS_DIR="$PWD/.tmp/torch-extensions"
export TRITON_CACHE_DIR="$PWD/.tmp/triton"
export PYTORCH_ENABLE_MPS_FALLBACK=1
