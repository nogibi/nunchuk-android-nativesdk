package com.nunchuk.android.bitbox

enum class BitBoxProduct {
    UNKNOWN,
    NOVA_MULTI,
    NOVA_BITCOIN_ONLY,
    BITBOX02_MULTI,
    BITBOX02_BITCOIN_ONLY,
}

data class BitBoxDeviceInfo(
    val product: BitBoxProduct,
    val firmwareVersion: String,
    val name: String,
    val unlocked: Boolean,
    val initialized: Boolean,
    val mnemonicPassphraseEnabled: Boolean,
    val securechipModel: String,
    val bluetoothEnabled: Boolean,
    val bluetoothFirmwareVersion: String,
    val bluetoothFirmwareHash: String,
)

data class BitBoxInitializeResult(
    val device: BitBoxDeviceInfo,
    val attestation: BitBoxAttestationStatus,
    val attestationMessage: String,
)
