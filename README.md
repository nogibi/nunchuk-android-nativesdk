# Nunchuk Android Native SDK
Nunchuk Android Native SDK is a wrapper for [libnunchuk](https://github.com/nunchuk-io/libnunchuk).

For more info on our products, please visit [our website](https://nunchuk.io/).

# Building
## Build requirements
NDK: 27.2.12479018

minSdkVersion: 24

## Mac OS
### 1. Xcode Command Line Tools

The Xcode Command Line Tools are a collection of build tools for macOS.
These tools must be installed in order to build Bitcoin Core from source.

To install, run the following command from your terminal:

``` bash
xcode-select --install
```

Upon running the command, you should see a popup appear.
Click on `Install` to continue the installation process.

### 2. Homebrew Package Manager

Homebrew is a package manager for macOS that allows one to install packages from the command line easily.
To install the Homebrew package manager, see: https://brew.sh

Note: If you run into issues while installing Homebrew or pulling packages, refer to [Homebrew's troubleshooting page](https://docs.brew.sh/Troubleshooting).

### 3. Brew Dependencies

Run the following from your terminal:

``` bash
brew install automake libtool boost pkg-config libevent
```

## Linux

Ensure the following packages are installed on your system:
```
make automake ninja-build libtool pkg-config git openjdk-21-jdk
```

## Building
### 1. Pull submodules

``` bash
pushd ${PWD}/src/main/native
git submodule add --force -b main https://github.com/nunchuk-io/libnunchuk.git
git submodule update --init --recursive
bash .install_deps.sh
popd
```

### 2. Export PATH Variables
``` bash
export ANDROID_SDK=/path/to/your/android/sdk
export ANDROID_NDK_HOME=/path/to/your/android/sdk/ndk/27.2.12479018

# On Mac OS, the default paths are:
export ANDROID_SDK=/Users/${USER}/Library/Android/sdk
export ANDROID_NDK_HOME=/Users/${USER}/Library/Android/sdk/ndk/27.2.12479018
# On Linux, the default paths are:
export ANDROID_SDK=$HOME/Android/Sdk
export ANDROID_NDK_HOME=$HOME/Android/Sdk/ndk/27.2.12479018
```

### 3. Build the SDK
```
./gradlew clean assembleArmRelease --stacktrace
./gradlew publish
```


## Satochip

`com.nunchuk.android.satochip.SatochipNativeClient` bridges libnunchuk's Satochip
master-signer and PSBT APIs. Initialize `NunchukNativeSdk` first. Supply a
`SatochipCard` adapter backed by the Satochip Java library and run each complete
operation synchronously off the UI thread, with exclusive access to one
PIN-authenticated card session. The adapter must throw on APDU/transport errors;
`getExtendedKey` must derive on the card even when an xpub is already cached.

- `createMasterSigner` registers the card and caches default xpubs.
- `addMasterSigner` creates metadata from a fingerprint/name without a card, for sync.
- `getSigner` returns a cached xpub or reads and saves a custom-path xpub.
- `cacheMasterSignerXpubs` refills the registered signer's cache.
- `importSeed` applies UTF-8 NFKD normalization before libnunchuk derives the BIP39 seed.
- `signPsbt` accepts a `Wallet` or BSMS/descriptor content. Libnunchuk persists and
  atomically consumes the card-encrypted MuSig2 nonces.

The adapter owns firmware/policy checks and per-hash 2FA authorization, if used.
Its `importSeed` implementation must clear the supplied seed bytes in a `finally`
block. Callbacks are not retained after the native call returns.

The NFC example adapter/UI lives at
`/mnt/sda7/ledger-utils/ledger-example/ledger-android`. The SDK does not bundle the
Satochip Java library. Build against the Satochip-enabled libnunchuk tree using
`NUNCHUK_LIBNUNCHUK_DIR=/mnt/sda7/integrate-satochip/libnunchuk`; the bundled
submodule is left unchanged.
