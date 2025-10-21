#!/usr/bin/env bash

export LC_ALL=C
set -eu

# JunoCash Release Packaging Utility
# Packages built binaries into distributable archives

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default settings
PLATFORM=""
VERSION=""
OUTPUT_DIR="${REPO_ROOT}/release"
SRC_DIR="${REPO_ROOT}/src"

# Print functions
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Usage information
usage() {
    cat <<EOF
Usage: $0 -p PLATFORM -v VERSION [OPTIONS]

Package JunoCash binaries into distributable archives.

REQUIRED:
    -p, --platform PLATFORM Platform identifier (linux-x86_64, win64, macos-x86_64, macos-arm64)
    -v, --version VERSION   Version string (e.g., 1.0.0)

OPTIONS:
    -s, --src-dir DIR       Directory containing built binaries (default: ./src)
    -o, --output DIR        Output directory for packages (default: ./release)
    -h, --help              Show this help message

SUPPORTED PLATFORMS:
    linux-x86_64            Linux 64-bit
    linux-arm64             Linux ARM64
    win64                   Windows 64-bit
    macos-x86_64            macOS Intel
    macos-arm64             macOS Apple Silicon

EXAMPLES:
    # Package Linux build
    $0 -p linux-x86_64 -v 1.0.0

    # Package Windows build with custom paths
    $0 -p win64 -v 1.0.0 -s ./build/windows -o ./dist

EOF
    exit 0
}

# Parse command line arguments
while [ $# -gt 0 ]; do
    case "$1" in
        -p|--platform)
            PLATFORM="$2"
            shift 2
            ;;
        -v|--version)
            VERSION="$2"
            shift 2
            ;;
        -s|--src-dir)
            SRC_DIR="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            print_error "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Validate required arguments
if [ -z "$PLATFORM" ] || [ -z "$VERSION" ]; then
    print_error "Platform and version are required"
    echo "Use --help for usage information"
    exit 1
fi

# Validate platform
case "$PLATFORM" in
    linux-x86_64|linux-arm64|win64|macos-x86_64|macos-arm64)
        ;;
    *)
        print_error "Invalid platform: $PLATFORM"
        echo "Use --help for supported platforms"
        exit 1
        ;;
esac

# Determine file extensions and archive format
EXE_EXT=""
ARCHIVE_EXT="tar.gz"
if [[ "$PLATFORM" == win* ]]; then
    EXE_EXT=".exe"
    ARCHIVE_EXT="zip"
fi

print_info "Packaging JunoCash v${VERSION} for ${PLATFORM}"

# Check source directory
if [ ! -d "$SRC_DIR" ]; then
    print_error "Source directory not found: $SRC_DIR"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Create temporary packaging directory
PKG_DIR="$OUTPUT_DIR/junocash-${VERSION}-${PLATFORM}"
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR/bin"
mkdir -p "$PKG_DIR/doc"

print_info "Copying binaries..."

# Copy binaries
BINARIES_FOUND=0

if [ -f "$SRC_DIR/junocashd${EXE_EXT}" ]; then
    cp "$SRC_DIR/junocashd${EXE_EXT}" "$PKG_DIR/bin/"
    print_success "Copied: junocashd${EXE_EXT}"
    BINARIES_FOUND=$((BINARIES_FOUND + 1))
fi

if [ -f "$SRC_DIR/junocash-cli${EXE_EXT}" ]; then
    cp "$SRC_DIR/junocash-cli${EXE_EXT}" "$PKG_DIR/bin/"
    print_success "Copied: junocash-cli${EXE_EXT}"
    BINARIES_FOUND=$((BINARIES_FOUND + 1))
fi

if [ -f "$SRC_DIR/junocash-tx${EXE_EXT}" ]; then
    cp "$SRC_DIR/junocash-tx${EXE_EXT}" "$PKG_DIR/bin/"
    print_success "Copied: junocash-tx${EXE_EXT}"
    BINARIES_FOUND=$((BINARIES_FOUND + 1))
fi

# Look for wallet tool in Rust target directory
if [ -f "$REPO_ROOT/target/release/junocashd-wallet-tool${EXE_EXT}" ]; then
    cp "$REPO_ROOT/target/release/junocashd-wallet-tool${EXE_EXT}" "$PKG_DIR/bin/"
    print_success "Copied: junocashd-wallet-tool${EXE_EXT}"
    BINARIES_FOUND=$((BINARIES_FOUND + 1))
fi

if [ $BINARIES_FOUND -eq 0 ]; then
    print_error "No binaries found in $SRC_DIR"
    rm -rf "$PKG_DIR"
    exit 1
fi

print_info "Copying documentation..."

# Copy documentation
if [ -f "$REPO_ROOT/README.md" ]; then
    cp "$REPO_ROOT/README.md" "$PKG_DIR/"
fi

if [ -f "$REPO_ROOT/COPYING" ]; then
    cp "$REPO_ROOT/COPYING" "$PKG_DIR/"
fi

# Create release notes
cat > "$PKG_DIR/RELEASE-NOTES.txt" <<EOF
JunoCash v${VERSION} - ${PLATFORM}
$(date -u '+%Y-%m-%d %H:%M:%S UTC')

This is a binary release of JunoCash for ${PLATFORM}.

INSTALLATION:
1. Extract this archive to your desired location
2. Add the 'bin' directory to your PATH, or run binaries directly
3. Create a configuration file at ~/.junocash/junocash.conf

INCLUDED BINARIES:
$(cd "$PKG_DIR/bin" && ls -1)

GETTING STARTED:
See README.md for detailed instructions on:
- Running a node
- Mining
- Wallet management
- Network configuration

For more information, visit: https://github.com/junocash/juno

SECURITY:
- Verify the SHA256 checksum of this archive matches the published value
- Always download from official sources
- Use strong RPC passwords and restrict network access

LICENSE:
See COPYING for license information.

EOF

# Create example configuration file
cat > "$PKG_DIR/junocash.conf.example" <<'CONFEOF'
# JunoCash Configuration File Example
# Copy this to ~/.junocash/junocash.conf and customize as needed

##
## Network Settings
##

# Connect to specific nodes
#addnode=seed1.junocash.com
#addnode=seed2.junocash.com

# Maximum number of inbound+outbound connections
#maxconnections=125

##
## RPC Settings
##

# RPC server settings (required for junocash-cli)
#rpcuser=your_username_here
#rpcpassword=your_secure_password_here

# Allow RPC connections from specific IPs (default: 127.0.0.1)
#rpcallowip=127.0.0.1
#rpcallowip=192.168.1.0/24

# RPC port (default: 8233 for mainnet)
#rpcport=8233

##
## Mining Settings
##

# Enable mining (CPU mining with RandomX)
#gen=1

# Number of mining threads (0 = auto-detect)
#genproclimit=0

##
## Optional Developer Donation
##

# Donate a percentage of mining rewards (0-100)
#donationpercentage=0
#donationaddress=t1HwfuDqt2oAVexgpjDHg9yB7UpCKSmEES7

##
## Advanced Settings
##

# Enable debug logging
#debug=1

# Prune blockchain to reduce disk usage (in MiB, 0=disable)
#prune=0

# Data directory
#datadir=/path/to/data

CONFEOF

# Create platform-specific instructions
case "$PLATFORM" in
    win64)
        cat > "$PKG_DIR/START-HERE.txt" <<'WINEOF'
JunoCash for Windows

QUICK START:
1. Open Command Prompt in this directory
2. Run: bin\junocashd.exe -daemon
3. Wait for sync, then use: bin\junocash-cli.exe getblockchaininfo

CONFIGURATION:
Create a file at: %APPDATA%\JunoCash\junocash.conf
See junocash.conf.example for options

MINING:
Add to junocash.conf:
  gen=1

Or use RPC:
  bin\junocash-cli.exe setgenerate true

NOTES:
- You may need to allow junocashd.exe through Windows Firewall
- Blockchain data will be stored in %APPDATA%\JunoCash
- Use junocash-cli.exe to interact with the daemon

For more help, see README.md
WINEOF
        ;;
    linux-*|macos-*)
        cat > "$PKG_DIR/START-HERE.txt" <<'UNIXEOF'
JunoCash for Unix/macOS

QUICK START:
1. Open Terminal in this directory
2. Run: ./bin/junocashd -daemon
3. Wait for sync, then use: ./bin/junocash-cli getblockchaininfo

INSTALLATION (Optional):
  sudo cp bin/* /usr/local/bin/

CONFIGURATION:
Create a file at: ~/.junocash/junocash.conf
See junocash.conf.example for options

MINING:
Add to junocash.conf:
  gen=1

Or use RPC:
  ./bin/junocash-cli setgenerate true

NOTES:
- Blockchain data will be stored in ~/.junocash
- Use junocash-cli to interact with the daemon
- Consider running as a systemd service (see docs)

For more help, see README.md
UNIXEOF
        ;;
esac

print_info "Creating archive..."

# Create archive
cd "$OUTPUT_DIR"
ARCHIVE_NAME="junocash-${VERSION}-${PLATFORM}.${ARCHIVE_EXT}"

if [ "$ARCHIVE_EXT" = "zip" ]; then
    if command -v zip >/dev/null 2>&1; then
        zip -r -q "$ARCHIVE_NAME" "$(basename "$PKG_DIR")"
    else
        print_error "zip command not found"
        print_info "Install with: sudo apt-get install zip"
        exit 1
    fi
else
    tar -czf "$ARCHIVE_NAME" "$(basename "$PKG_DIR")"
fi

print_success "Archive created: $ARCHIVE_NAME"

# Generate SHA256 checksum
print_info "Generating checksum..."
if command -v sha256sum >/dev/null 2>&1; then
    CHECKSUM=$(sha256sum "$ARCHIVE_NAME" | awk '{print $1}')
    echo "$CHECKSUM  $ARCHIVE_NAME" >> "SHA256SUMS-${VERSION}.txt"
elif command -v shasum >/dev/null 2>&1; then
    CHECKSUM=$(shasum -a 256 "$ARCHIVE_NAME" | awk '{print $1}')
    echo "$CHECKSUM  $ARCHIVE_NAME" >> "SHA256SUMS-${VERSION}.txt"
else
    print_error "No SHA256 utility found"
    CHECKSUM="N/A"
fi

# Clean up temporary directory
rm -rf "$PKG_DIR"

# Print summary
cd "$REPO_ROOT"
print_success "Packaging complete!"
echo ""
print_info "Package Summary:"
echo "  Archive: $OUTPUT_DIR/$ARCHIVE_NAME"
echo "  Size: $(du -h "$OUTPUT_DIR/$ARCHIVE_NAME" | cut -f1)"
echo "  SHA256: $CHECKSUM"
echo ""
print_info "To verify the package:"
echo "  cd $OUTPUT_DIR"
if command -v sha256sum >/dev/null 2>&1; then
    echo "  sha256sum -c SHA256SUMS-${VERSION}.txt"
else
    echo "  shasum -a 256 -c SHA256SUMS-${VERSION}.txt"
fi
