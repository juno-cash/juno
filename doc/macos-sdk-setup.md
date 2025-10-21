# macOS SDK Setup for Cross-Compilation

This guide explains how to set up the macOS SDK for cross-compiling JunoCash binaries for macOS (including Apple Silicon) from a Linux build environment.

## Overview

To cross-compile for macOS, you need:
1. Xcode SDK (contains headers, libraries, and frameworks)
2. cctools (Apple's binary tools like `ld`, `ar`, etc.)
3. Clang/LLVM compiler (already handled by depends system)

## Method 1: Extract SDK from Xcode (Recommended)

### Prerequisites
- Access to a macOS machine with Xcode installed, OR
- Downloaded Xcode .xip file from Apple Developer

### Step 1: Download Xcode

Visit [Apple Developer Downloads](https://developer.apple.com/download/all/) and download the appropriate Xcode version for your target platform:

**For Intel Mac (x86_64-apple-darwin):**
- **Xcode 11.3.1** (Build ID: 11C505)
- File: `Xcode_11.3.1.xip`
- SDK: macOS 10.14 Mojave

**For Apple Silicon (aarch64-apple-darwin):**
- **Xcode 12.2+** or **Xcode 13.x/14.x** (recommended)
- Example: `Xcode_13.2.1.xip` or `Xcode_14.2.xip`
- SDK: macOS 11.0 Big Sur or macOS 12.0 Monterey

**Important:** Xcode 11.3.1 does NOT support Apple Silicon (it predates ARM64 Macs). You must use Xcode 12.2 or later for Apple Silicon builds.

Note: You'll need a free Apple Developer account.

### Step 2: Extract the SDK

#### On macOS:
```bash
# Install Xcode (if using .xip)
xip -x Xcode_13.2.1.xip  # Or whatever version you downloaded
sudo mv Xcode.app /Applications/

# Extract SDK
mkdir -p ~/MacOSX-SDKs
cd ~/MacOSX-SDKs
xcodebuild -version -sdk macosx Path
# Note the SDK path

# Check the SDK version
xcodebuild -version -sdk macosx SDKVersion
# Example output: 12.1

# Copy the SDK with appropriate name based on your target
# For Intel builds (Xcode 11.3.1):
cp -r /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk ./MacOSX10.14.sdk
tar -czf MacOSX10.14.sdk.tar.gz MacOSX10.14.sdk

# For Apple Silicon builds (Xcode 12.2+):
cp -r /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk ./MacOSX12.0.sdk
tar -czf MacOSX12.0.sdk.tar.gz MacOSX12.0.sdk

# You can also name it generically and the build system will find it
cp -r /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk ./MacOSX.sdk
```

#### On Linux (if you have the .xip):
```bash
# Install extraction tools
sudo apt-get install libxml2-dev libssl-dev zlib1g-dev

# Clone and build xar (if not available)
git clone https://github.com/mackyle/xar.git
cd xar/xar
./autogen.sh --prefix=/usr/local
make
sudo make install

# Extract Xcode
mkdir xcode-extract
cd xcode-extract
xar -xf ../Xcode_11.3.1.xip
cat Xcode.pkg/Payload | gunzip -dc | cpio -i

# Extract SDK
mkdir -p ~/MacOSX-SDKs
cd ~/MacOSX-SDKs
# The SDK will be in xcode-extract/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/
```

### Step 3: Install SDK for JunoCash Build

```bash
# Create SDK directory in JunoCash repo
cd /path/to/juno
mkdir -p depends/SDKs

# Copy or move the SDK
cp -r ~/MacOSX-SDKs/MacOSX10.14.sdk depends/SDKs/

# Or if you have the tarball
tar -xzf MacOSX10.14.sdk.tar.gz -C depends/SDKs/
```

### Step 4: Verify SDK Installation

```bash
ls -la depends/SDKs/MacOSX10.14.sdk

# You should see:
# - usr/include/
# - usr/lib/
# - System/Library/Frameworks/
```

## Method 2: Use Pre-extracted SDK (Alternative)

Some projects provide pre-extracted SDKs. However, we recommend Method 1 for legal compliance and SDK authenticity.

## SDK Configuration

The build system expects SDKs in `depends/SDKs/`:
- **Intel builds**: `MacOSX10.14.sdk` (from Xcode 11.3.1)
- **Apple Silicon builds**: `MacOSX12.0.sdk` or `MacOSX11.0.sdk` (from Xcode 12.2+)
- **Generic**: `MacOSX.sdk` (the build system will detect it)

You can also use a custom SDK path via environment variable:
```bash
export SDK_PATH=/path/to/your/sdk/directory
```

## Building for macOS

**Important SDK Requirements:**
- Intel (x86_64): Use SDK from Xcode 11.3.1 (macOS 10.14)
- Apple Silicon (ARM64): Use SDK from Xcode 12.2+ (macOS 11.0 or 12.0)

### For x86_64 macOS (Intel):
```bash
# Ensure you have MacOSX10.14.sdk from Xcode 11.3.1
cd depends
make HOST=x86_64-apple-darwin18 SDK_PATH=$(pwd)/SDKs -j$(nproc)
cd ..
./autogen.sh
CONFIG_SITE=$PWD/depends/x86_64-apple-darwin18/share/config.site ./configure
make -j$(nproc)
```

### For aarch64 macOS (Apple Silicon):
```bash
# Ensure you have MacOSX12.0.sdk or MacOSX11.0.sdk from Xcode 12.2+
cd depends
make HOST=aarch64-apple-darwin SDK_PATH=$(pwd)/SDKs -j$(nproc)
cd ..
./autogen.sh
CONFIG_SITE=$PWD/depends/aarch64-apple-darwin/share/config.site ./configure
make -j$(nproc)
```

## Automated Builds

The `zcutil/build-release.sh` script handles SDK detection automatically:
```bash
./zcutil/build-release.sh --macos
```

## Troubleshooting

### SDK Not Found
**Error**: `Cannot find macOS SDK`
**Solution**: Ensure SDK is at `depends/SDKs/MacOSX10.14.sdk` or set `SDK_PATH`

### Wrong SDK Version
**Error**: Build fails with missing headers
**Solution**: Verify you have the correct SDK version (10.14 for Xcode 11.3.1)

### cctools Build Fails
**Error**: `native_cctools` package fails to build
**Solution**: Ensure you have required dependencies:
```bash
sudo apt-get install -y \
  autoconf automake bison cmake libtool patch pkg-config \
  libssl-dev libxml2-dev zlib1g-dev
```

### Rust Cross-Compilation Issues
**Error**: Rust fails to cross-compile
**Solution**: Install Rust targets:
```bash
rustup target add x86_64-apple-darwin
rustup target add aarch64-apple-darwin
```

## Legal Considerations

- The macOS SDK is part of Xcode, which is provided by Apple under the Xcode and Apple SDKs Agreement
- You must accept Apple's license agreements to use the SDK
- Cross-compilation is permitted for development purposes
- Distributing the SDK itself is not permitted; each developer should extract their own

## References

- [Xcode Downloads](https://developer.apple.com/download/all/)
- [Apple Developer Agreement](https://developer.apple.com/support/terms/)
- [JunoCash Build Documentation](../README.md)
